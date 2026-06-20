#include <ntifs.h>
#include <ntstrsafe.h>
#include "../Types/ApcInjectApiSetTypes.h"
#include "../Api/ApcInjectNativeApi.h"
#include "ModuleResolver.h"
#include "UtilityFunctions.h"

NTSTATUS BBResolveApiSet(
    IN PEPROCESS pProcess,
    IN PUNICODE_STRING name,
    IN PUNICODE_STRING baseImage,
    OUT PUNICODE_STRING resolved
)
{
    NTSTATUS status = STATUS_NOT_FOUND;
    PPEB pPeb = PsGetProcessPeb(pProcess);
    if (pPeb == NULL)
    {
        return STATUS_NOT_FOUND;
    }
    PAPISET_NAMESPACE_ARRAY pApiSetMap = (PAPISET_NAMESPACE_ARRAY)pPeb->ApiSetMap;

    // Invalid name
    if (name == NULL || name->Length < 4 * sizeof(WCHAR) || name->Buffer == NULL ||
        (memcmp(name->Buffer, L"api-", 4) != 0 && memcmp(name->Buffer, L"ext-", 4) != 0))
        return STATUS_NOT_FOUND;

    // Iterate api set map
    for (ULONG i = 0; i < pApiSetMap->Count; i++)
    {
        PAPISET_NAMESPACE_ENTRY pDescriptor = NULL;
        PAPISET_VALUE_ARRAY pHostArray = NULL;
        wchar_t apiNameBuf[255] = { 0 };
        UNICODE_STRING apiName = { 0 };

#ifdef _WIN10_
        pDescriptor = (PAPISET_NAMESPACE_ENTRY)((PUCHAR)pApiSetMap + pApiSetMap->End + i * sizeof(API_SET_NAMESPACE_ENTRY_10));
        pHostArray = (PAPISET_VALUE_ARRAY)((PUCHAR)pApiSetMap + pApiSetMap->Start + sizeof(API_SET_VALUE_ARRAY_10) * pDescriptor->Size);

        memcpy(apiNameBuf, (PUCHAR)pApiSetMap + pHostArray->NameOffset, pHostArray->NameLength);
#else
        pDescriptor = pApiSetMap->Array + i;
        memcpy(apiNameBuf, (PUCHAR)pApiSetMap + pDescriptor->NameOffset, pDescriptor->NameLength);
#endif
        RtlUnicodeStringInit(&apiName, apiNameBuf);

        // Check if this is a target api
        if (BBSafeSearchString(name, &apiName, TRUE) >= 0)
        {
            PAPISET_VALUE_ENTRY pHost = NULL;
            wchar_t apiHostNameBuf[255] = { 0 };
            UNICODE_STRING apiHostName = { 0 };

#ifdef _WIN10_
            pHost = (PAPISET_VALUE_ENTRY)((PUCHAR)pApiSetMap + pHostArray->DataOffset);
#else
            pHostArray = (PAPISET_VALUE_ARRAY)((PUCHAR)pApiSetMap + pDescriptor->DataOffset);
            pHost = pHostArray->Array;
#endif
            // Sanity check
            if (pHostArray->Count < 1)
                return STATUS_NOT_FOUND;

            memcpy(apiHostNameBuf, (PUCHAR)pApiSetMap + pHost->ValueOffset, pHost->ValueLength);
            RtlUnicodeStringInit(&apiHostName, apiHostNameBuf);

            // No base name redirection
            if (pHostArray->Count == 1 || baseImage == NULL || baseImage->Buffer[0] == 0)
            {
                BBSafeInitString(resolved, &apiHostName);
                return STATUS_SUCCESS;
            }
            // Redirect accordingly to base name
            else
            {
                UNICODE_STRING baseImageName = { 0 };
                BBStripPath(baseImage, &baseImageName);

                if (RtlCompareUnicodeString(&apiHostName, &baseImageName, TRUE) == 0)
                {
                    memset(apiHostNameBuf, 0, sizeof(apiHostNameBuf));
                    memcpy(apiHostNameBuf, (PUCHAR)pApiSetMap + pHost[1].ValueOffset, pHost[1].ValueLength);
                    RtlCreateUnicodeString(resolved, apiHostNameBuf);
                    return STATUS_SUCCESS;
                }
                else
                {
                    BBSafeInitString(resolved, &apiHostName);
                    return STATUS_SUCCESS;
                }
            }
        }
    }

    return status;
}

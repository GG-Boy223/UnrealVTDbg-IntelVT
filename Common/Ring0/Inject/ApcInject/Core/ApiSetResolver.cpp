#include <ntifs.h>
#include <ntstrsafe.h>
#include "../Types/ApcInjectApiSetTypes.h"
#include "../Api/ApcInjectNativeApi.h"
#include "ModuleResolver.h"
#include "UtilityFunctions.h"

namespace
{
    bool copy_api_set_string(
        OUT PWCHAR destination,
        SIZE_T destination_bytes,
        IN const void* api_set_map,
        IN ULONG offset,
        IN ULONG length)
    {
        constexpr SIZE_T wchar_size = sizeof(WCHAR);

        if (destination == nullptr ||
            api_set_map == nullptr ||
            length == 0 ||
            (length % wchar_size) != 0 ||
            length >= destination_bytes)
        {
            return false;
        }

        const unsigned __int8* source =
            reinterpret_cast<const unsigned __int8*>(api_set_map) + offset;
        RtlCopyMemory(destination, source, length);
        destination[length / wchar_size] = L'\0';
        return true;
    }
}

/// <param name="name">Name to resolve</param>
/// <param name="baseImage">Parent image name</param>
/// <param name="resolved">Resolved name if any</param>
/// <returns>Status code</returns>
/// <summary>
/// Try to resolve image via API SET map
/// </summary>
/// <param name="pProcess">Target process. Must be run in the context of this process</param>
NTSTATUS BBResolveApiSet(
    IN PEPROCESS pProcess,
    IN PUNICODE_STRING name,
    IN PUNICODE_STRING baseImage,
    OUT PUNICODE_STRING resolved
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
)
{
    NTSTATUS status = STATUS_NOT_FOUND;
    if (pProcess == nullptr || name == nullptr || resolved == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

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

    if (pApiSetMap == nullptr)
    {
        return STATUS_NOT_FOUND;
    }

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

        if (!copy_api_set_string(
                apiNameBuf,
                sizeof(apiNameBuf),
                pApiSetMap,
                pHostArray->NameOffset,
                pHostArray->NameLength))
        {
            return STATUS_INVALID_PARAMETER;
        }
#else
        pDescriptor = pApiSetMap->Array + i;
        if (!copy_api_set_string(
                apiNameBuf,
                sizeof(apiNameBuf),
                pApiSetMap,
                pDescriptor->NameOffset,
                pDescriptor->NameLength))
        {
            return STATUS_INVALID_PARAMETER;
        }
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

            if (!copy_api_set_string(
                    apiHostNameBuf,
                    sizeof(apiHostNameBuf),
                    pApiSetMap,
                    pHost->ValueOffset,
                    pHost->ValueLength))
            {
                return STATUS_INVALID_PARAMETER;
            }
            RtlUnicodeStringInit(&apiHostName, apiHostNameBuf);

            // No base name redirection
            if (pHostArray->Count == 1 || baseImage == NULL || baseImage->Buffer == NULL || baseImage->Buffer[0] == 0)
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
                    if (!copy_api_set_string(
                        apiHostNameBuf,
                        sizeof(apiHostNameBuf),
                        pApiSetMap,
                        pHost[1].ValueOffset,
                        pHost[1].ValueLength))
                    {
                        return STATUS_INVALID_PARAMETER;
                    }
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

    //ExReleaseFastMutex(&LongFlagsMutex);
    return status;
}

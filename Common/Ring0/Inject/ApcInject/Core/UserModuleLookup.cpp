#include <ntifs.h>
#include <ntstrsafe.h>
#include "../../../SymbolicAccess/Utils/Log.h"
#include "../Api/ApcInjectNativeApi.h"
#include "ModuleResolver.h"

PVOID BBGetUserModule(IN PEPROCESS pProcess, IN PUNICODE_STRING ModuleName, IN BOOLEAN isWow64)
{
    ASSERT(pProcess != NULL);
    if (pProcess == NULL)
        return NULL;

    __try
    {
        LARGE_INTEGER time = { 0 };
        time.QuadPart = -250ll * 10 * 1000;

        if (isWow64)
        {
            PPEB32 pPeb32 = (PPEB32)PsGetProcessWow64Process(pProcess);
            if (pPeb32 == NULL)
            {
                outLog("BBGetUserModule failed to get Wow64 PEB");
                return NULL;
            }

            for (INT i = 0; !pPeb32->Ldr && i < 10; i++)
            {
                KeDelayExecutionThread(KernelMode, TRUE, &time);
            }

            if (!pPeb32->Ldr)
            {
                outLog("BBGetUserModule Wow64 loader is null");
                return NULL;
            }

            for (PLIST_ENTRY32 pListEntry = (PLIST_ENTRY32)((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList.Flink;
                pListEntry != &((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList;
                pListEntry = (PLIST_ENTRY32)pListEntry->Flink)
            {
                UNICODE_STRING ustr;
                PLDR_DATA_TABLE_ENTRY32 pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

                RtlUnicodeStringInit(&ustr, (PWCH)pEntry->BaseDllName.Buffer);

                if (RtlCompareUnicodeString(&ustr, ModuleName, TRUE) == 0)
                    return (PVOID)pEntry->DllBase;
            }
        }
        else
        {
            PPEB pPeb = PsGetProcessPeb(pProcess);
            if (!pPeb)
            {
                outLog("BBGetUserModule failed to get PEB");
                return NULL;
            }

            for (INT i = 0; !pPeb->Ldr && i < 10; i++)
            {
                KeDelayExecutionThread(KernelMode, TRUE, &time);
            }

            if (!pPeb->Ldr)
            {
                outLog("BBGetUserModule loader is null");
                return NULL;
            }

            for (PLIST_ENTRY pListEntry = pPeb->Ldr->InLoadOrderModuleList.Flink;
                pListEntry != &pPeb->Ldr->InLoadOrderModuleList;
                pListEntry = pListEntry->Flink)
            {
                PLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
                if (RtlCompareUnicodeString(&pEntry->BaseDllName, ModuleName, TRUE) == 0)
                    return pEntry->DllBase;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        outLog("BBGetUserModule Exception, Code: 0x%X\n", GetExceptionCode());
    }

    return NULL;
}
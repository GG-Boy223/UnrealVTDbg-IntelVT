#include "../../Driver.h"
#include "../../ntos/inc/mmtypes.h"
#include "../../ntos/inc/ntdbg.h"
#include "../../ntos/inc/ketypes.h"
#include "../../ntos/inc/extypes.h"
#include "../../ntos/inc/ntosdef.h"
#include "../../ntos/inc/amd64.h"
#include "../../ntos/inc/mi.h"
#include "../../ntos/inc/pstypes.h"
#include "../../ntos/inc/obtypes.h"
#include "../../ntos/inc/peb_teb.h"
#include "../../Log/log.h"
#include "../../List/MyList.h"
#include "../../ntos/inc/ntlpcapi.h"
#include "../../ntos/inc/psp.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"
#include "../../Hooks/EptHook/EptHook.h"
#include "ProtectProcess.h"

//是被保护的文件
BOOLEAN IsProtectFile(PUNICODE_STRING ImageFileName)
{
    PLIST_ENTRY ListHead, NextEntry;
    PFILEDATA fileData;

    if (ImageFileName && StrIsValid2(*ImageFileName))
    {
        ExAcquireFastMutex(&g_ProtectFileObjList.Mutex);
        __try
        {
            ListHead = &g_ProtectFileObjList.list_entry.ListHead;
            NextEntry = ListHead->Flink;
            while (ListHead != NextEntry)
            {
                fileData = CONTAINING_RECORD(NextEntry,
                    FILEDATA,
                    list_entry);
                if (/*RtlUnicodeStringContains(ImageFileName, &fileData->fileName, TRUE)*/ RtlEqualUnicodeString(ImageFileName, &fileData->fileName, FALSE))
                {
                    return TRUE;
                }
                /* Move to the next entry */
                NextEntry = NextEntry->Flink;
            }
            return FALSE;
        }
        __finally
        {
            ExReleaseFastMutex(&g_ProtectFileObjList.Mutex);
        }
    }
    else
    {
        return FALSE;
    }
}

//是白名单进程
NTSTATUS IsWhiteListProcess(_In_ HANDLE ProcessHandle,  //要读取的目标进程
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead)
{
    NTSTATUS Status = STATUS_SUCCESS;

    _EPROCESS* Process;
    PUNICODE_STRING ImageFileName;
    UNICODE_STRING fileName;
    UNICODE_STRING CurrentProcName, PassProcName, PassProcName2;
    BOOLEAN bIsPassProc = FALSE;

    /* Reference the process */
    NTSTATUS Status2 = ObReferenceObjectByHandle(ProcessHandle,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        KernelMode,
        (PVOID*)&Process,
        NULL);
    if (NT_SUCCESS(Status2))
    {
        Status2 = SeLocateProcessImageName((PEPROCESS)Process, &ImageFileName);
        if (NT_SUCCESS(Status2))
        {
            WCHAR SubStr[256] = { 0 };
            if (GetModuleFileName(&SubStr[0], ImageFileName))
            {
                RtlInitUnicodeString(&fileName, SubStr);
                if (StrIsValid2(fileName))
                {
                    WCHAR SubStr2[256] = { 0 };
                    Status2 = GetProcessName(PsGetCurrentProcess(), &SubStr2[0]);
                    if (NT_SUCCESS(Status2))
                    {
                        RtlInitUnicodeString(&CurrentProcName, SubStr2);
                        RtlInitUnicodeString(&PassProcName, _T("csrss.exe"));
                        RtlInitUnicodeString(&PassProcName2, _T("BloodDbg.exe"));
                        if (RtlEqualUnicodeString(&CurrentProcName, &PassProcName, FALSE) || RtlEqualUnicodeString(&CurrentProcName, &PassProcName2, FALSE))
                        {
                            bIsPassProc = TRUE;
                        }
                    }
                    if (IsProtectFile(&fileName) && (!bIsPassProc))
                    {
                        outLog("当前进程: %s   >>>>>>>>>   目标进程: %s      BaseAddress: %p   Size: %d", ((_EPROCESS*)PsGetCurrentProcess())->ImageFileName, Process->ImageFileName, BaseAddress, BufferSize);
                        //RtlZeroMemory(Buffer, BufferSize);
                        Status = STATUS_ACCESS_DENIED;
                    }
                }
            }
            if (ImageFileName)
            {
                ExFreePool(ImageFileName);
            }
        }
        if (Process)
        {
            ObDereferenceObject(Process);
        }
    }
    return Status;
}
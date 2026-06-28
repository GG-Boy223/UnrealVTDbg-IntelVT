#include "DebugEventHandlersInternal.h"

#pragma optimize("", off)
EXTERN_C VOID PspExitThread(_In_ NTSTATUS ExitStatus)
{
    Debugport_Struct s;
    PDEBUG_OBJECT Port;
    PDEBUG_PROCESS DebugProcess;
    _EPROCESS* Process = (_EPROCESS*)PsGetCurrentProcess();

    DbgkStateOpGuard stateGuard;
    if (IsDbgkUnloadInProgress())
    {
        s.id = 0xfbbd6252520ea3d5; //PspExitThread 经16位md5哈希摘要
        s.DebugPort = 0;
        if (Original_PspExitThread != NULL)
        {
            Original_PspExitThread(ExitStatus);
        }
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    //DbgBreakPoint();
    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        Port = DebugProcess->DebugObject;
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
        Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
    }

    s.id = 0xfbbd6252520ea3d5; //PspExitThread 经16位md5哈希摘要
    s.DebugPort = (size_t)Port;
    Original_PspExitThread(ExitStatus);
}
#pragma optimize("", on)

EXTERN_C VOID DbgkExitThread(
    NTSTATUS ExitStatus)
{
    PVOID Port = NULL;
    DBGKM_APIMSG ApiMsg = { 0 }; // [rsp+20h] [rbp-118h]
    _EPROCESS* Process; //进程对象
    PETHREAD Thread;
    PDEBUG_PROCESS DebugProcess;

    PAGED_CODE();

    Process = (_EPROCESS*)PsGetCurrentProcess();

    DbgPrint("\xCF\xDF\xB3\xCC\xCD\xCB\xB3\xF6\n");
    PrintProcessName(Process);

    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        Port = DebugProcess->DebugObject;
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
        Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
    }

    // /* Send the message on the LPC Port */
    if (!Port)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    Thread = PsGetCurrentThread();

    size_t ptr_CrossThreadFlags = (size_t)Thread + ethread_offset::CrossThreadFlags;
    CROSSTHREADFLAGS CrossThreadFlags = *(CROSSTHREADFLAGS*)ptr_CrossThreadFlags;

    if (CrossThreadFlags.ThreadInserted)
    {
        ApiMsg.u.ExitThread.ExitStatus = ExitStatus;
        ApiMsg.h.u1.Length = 0x34000C;
        ApiMsg.h.u2.ZeroInit = LPC_DEBUG_EVENT;
        ApiMsg.ApiNumber = DbgKmExitThreadApi;
        DbgkpSendApiMessage(Process, TRUE, &ApiMsg);
        DbgPrint("\xD7\xAA\xB7\xA2\xCF\xDF\xB3\xCC\xCD\xCB\xB3\xF6\xCA\xC2\xBC\xFE\n");
    }
}

EXTERN_C
//pass:
//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
DbgkExitProcess(
    NTSTATUS ExitStatus)
{
    PVOID Port = NULL;
    DBGKM_APIMSG ApiMsg = { 0 }; // [rsp+20h] [rbp-118h]
    _EPROCESS* Process; //进程对象
    PETHREAD Thread;
    PDEBUG_PROCESS DebugProcess;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    DbgPrint("\xBD\xF8\xB3\xCC\xCD\xCB\xB3\xF6\n");

    Process = (_EPROCESS*)PsGetCurrentProcess();

    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        Port = DebugProcess->DebugObject;
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
        Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
    }

    // /* Send the message on the LPC Port */
    if (!Port)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    Thread = PsGetCurrentThread();

    size_t ptr_CrossThreadFlags = (size_t)Thread + ethread_offset::CrossThreadFlags;
    CROSSTHREADFLAGS CrossThreadFlags = *(CROSSTHREADFLAGS*)ptr_CrossThreadFlags;

    if (CrossThreadFlags.ThreadInserted)
    {
        size_t ptr_ExitTime = (size_t)Process + eprocess_offset::ExitTime;
        KeQuerySystemTime(ptr_ExitTime);

        ApiMsg.u.ExitProcess.ExitStatus = ExitStatus;
        ApiMsg.h.u1.Length = 0x34000C;
        ApiMsg.h.u2.ZeroInit = LPC_DEBUG_EVENT;
        ApiMsg.ApiNumber = DbgKmExitProcessApi;
        DbgkpSendApiMessage(Process, FALSE, &ApiMsg);
        PrintProcessName(Process);
    }
}

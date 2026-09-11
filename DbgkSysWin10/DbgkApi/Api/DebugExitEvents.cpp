#include "DebugEventHandlersInternal.h"

#pragma optimize("", off)
EXTERN_C
VOID PspExitThread(_In_ NTSTATUS ExitStatus)
{
    Debugport_Struct s;
    PDEBUG_OBJECT Port = NULL;
    PDEBUG_PROCESS DebugProcess = NULL;
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
        return;
    }

    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        if (DebugProcess != NULL && DebugProcess->DebugObject != NULL)
        {
            Port = DebugProcess->DebugObject;
        }
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
        Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
    }

    s.id = 0xfbbd6252520ea3d5; //PspExitThread 经16位md5哈希摘要
    s.DebugPort = (size_t)Port;

    if (IsDbgkUnloadInProgress())
    {
        s.DebugPort = 0;
    }

    Original_PspExitThread(ExitStatus);
}
#pragma optimize("", on)


EXTERN_C
VOID DbgkExitThread(NTSTATUS ExitStatus)
{
    PVOID Port = NULL;
    DBGKM_APIMSG ApiMsg = { 0 }; // [rsp+20h] [rbp-118h]
    _EPROCESS* Process; //进程对象
    PETHREAD Thread;
    PDEBUG_PROCESS DebugProcess = NULL;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return;
    }

    Process = (_EPROCESS*)PsGetCurrentProcess();
    PrintProcessName(Process);

    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        if (DebugProcess != NULL && DebugProcess->DebugObject != NULL)
        {
            Port = DebugProcess->DebugObject;
        }
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
        Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
    }

    if (!Port)
    {
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
    }
}

EXTERN_C
VOID DbgkExitProcess(NTSTATUS ExitStatus)
{
    PVOID Port = NULL;
    DBGKM_APIMSG ApiMsg = { 0 }; // [rsp+20h] [rbp-118h]
    _EPROCESS* Process; //进程对象
    PETHREAD Thread;
    PDEBUG_PROCESS DebugProcess = NULL;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return;
    }

    Process = (_EPROCESS*)PsGetCurrentProcess();

    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        if (DebugProcess != NULL && DebugProcess->DebugObject != NULL)
        {
            Port = DebugProcess->DebugObject;
        }
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
        Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
    }

    if (!Port)
    {
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

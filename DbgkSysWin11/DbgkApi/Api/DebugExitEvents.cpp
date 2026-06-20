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
        s.id = 0xfbbd6252520ea3d5;
        s.DebugPort = 0;
        if (Original_PspExitThread != NULL)
        {
            Original_PspExitThread(ExitStatus);
        }
        return;
    }

    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        Port = DebugProcess->DebugObject;
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
        Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
    }

    s.id = 0xfbbd6252520ea3d5;
    s.DebugPort = (size_t)Port;
    Original_PspExitThread(ExitStatus);
}
#pragma optimize("", on)

EXTERN_C VOID DbgkExitThread(
    NTSTATUS ExitStatus)
{
    PVOID Port = NULL;
    DBGKM_APIMSG ApiMsg = { 0 };
    _EPROCESS* Process;
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
        DbgPrint("\xD7\xAA\xB7\xA2\xCF\xDF\xB3\xCC\xCD\xCB\xB3\xF6\xCA\xC2\xBC\xFE\n");
    }
}

EXTERN_C
VOID
DbgkExitProcess(
    NTSTATUS ExitStatus)
{
    PVOID Port = NULL;
    DBGKM_APIMSG ApiMsg = { 0 };
    _EPROCESS* Process;
    PETHREAD Thread;
    PDEBUG_PROCESS DebugProcess;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
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

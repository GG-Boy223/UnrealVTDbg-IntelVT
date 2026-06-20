#include "DebugEventsCommon.h"
#include "../../Driver.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"

KPROCESSOR_MODE KeGetPreviousMode()
{
    _ETHREAD* Thread = (_ETHREAD*)KeGetCurrentThread();
    size_t kthread_base = (size_t)Thread + ethread_offset::Tcb;
    size_t ptr_PreviousMode = kthread_base + kthread_offset::PreviousMode;
    return *(KPROCESSOR_MODE*)ptr_PreviousMode;
}

KPROCESSOR_MODE KeSetPreviousMode(KPROCESSOR_MODE PreviousMode)
{
    KPROCESSOR_MODE Old_PreviousMode;
    _ETHREAD* Thread = (_ETHREAD*)KeGetCurrentThread();
    size_t kthread_base = (size_t)Thread + ethread_offset::Tcb;
    size_t ptr_PreviousMode = kthread_base + kthread_offset::PreviousMode;
    Old_PreviousMode = *(KPROCESSOR_MODE*)ptr_PreviousMode;
    *(KPROCESSOR_MODE*)ptr_PreviousMode = PreviousMode;
    return Old_PreviousMode;
}

LONG ExSystemExceptionFilter(VOID)
{
    return (KeGetPreviousMode() != KernelMode ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH);
}

EXTERN_C
VOID KiDispatchException(IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN KPROCESSOR_MODE PreviousMode,
    IN BOOLEAN FirstChance)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (Original_KiDispatchException != NULL)
    {
        Original_KiDispatchException(ExceptionRecord, ExceptionFrame, TrapFrame, PreviousMode, FirstChance);
    }
}

BOOLEAN IsDebugger(PEPROCESS Process)
{
    BOOLEAN result = FALSE;
    PLIST_ENTRY ListHead, NextEntry;
    PDEBUGGER_TABLE_ENTRY entry;

    if (Process == NULL || DbgkIsDriverRuntimeUnloading() || KeGetCurrentIrql() > APC_LEVEL)
    {
        return FALSE;
    }

    HANDLE pid = PsGetProcessId(Process);

    ExAcquireFastMutex(&g_DebuggerList.Mutex);
    ListHead = &g_DebuggerList.list_entry.ListHead;
    NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        entry = CONTAINING_RECORD(NextEntry,
            DEBUGGER_TABLE_ENTRY,
            list_entry);

        if (entry)
        {
            if (entry->dwPid == HandleToULong(pid))
            {
                result = TRUE;
                break;
            }
        }

        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_DebuggerList.Mutex);
    return result;
}

BOOLEAN IsProtectTargetProcess(_EPROCESS* Process)
{
    BOOLEAN result = FALSE;
    if (Process == g_ProtectTargetProcess)
    {
        result = TRUE;
    }
    return result;
}

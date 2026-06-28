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
    // *(KPROCESSOR_MODE*)ptr_PreviousMode = PreviousMode;
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

//判断目标进程是否是我们自己的调试器
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
                // *DebugProcess = entry;
                result = TRUE;
                break;
            }
        }

        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_DebuggerList.Mutex);
    return result;
}

//    __finally
//
//BOOLEAN SetDebugTargetProcess(_EPROCESS* Process, PDEBUG_OBJECT DebugObject)
//{
//    BOOLEAN result = FALSE;
//    g_DebugProcessList->Lock();
//    __try
//    {
//        if (IsDebugger(NULL))
//        {
//            PDEBUG_PROCESS tmp = new DEBUG_PROCESS;
//            RtlZeroMemory(tmp, sizeof(DEBUG_PROCESS));
//            tmp->Process = Process;
//            tmp->DebugObject = DebugObject;
//            g_DebugProcessList->Add(tmp);
//            result = TRUE;
//        }
//        return result;
//    }
//        g_DebugProcessList->UnLock();
//}
BOOLEAN IsProtectTargetProcess(_EPROCESS* Process)
{
    BOOLEAN result = FALSE;
    if (Process == g_ProtectTargetProcess)
    {
        // *DebugProcess = entry;
        result = TRUE;
    }
    return result;
}

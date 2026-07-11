#include "DebugObjectCommonInternal.h"

BOOLEAN IsDbgkUnloadInProgress()
{
    return InterlockedCompareExchange(&g_DbgkDriverUnloading, 0, 0) != 0;
}

BOOLEAN LookupDebugTargetProcessObject(_EPROCESS* Process, PDEBUG_OBJECT* DebugObject)
{
    BOOLEAN result = FALSE;

    if (DebugObject == NULL)
    {
        return FALSE;
    }

    *DebugObject = NULL;

    if (Process == NULL || IsDbgkUnloadInProgress() || KeGetCurrentIrql() > APC_LEVEL)
    {
        return FALSE;
    }

    ExAcquireFastMutex(&g_DebugProcessList.Mutex);
    PLIST_ENTRY ListHead = &g_DebugProcessList.list_entry.ListHead;
    PLIST_ENTRY NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        PDEBUG_PROCESS entry = CONTAINING_RECORD(NextEntry, DEBUG_PROCESS, list_entry);
        if (entry && entry->Process == Process)
        {
            *DebugObject = entry->DebugObject;
            // *DebugObject = entry->DebugObject;
            result = (*DebugObject != NULL);
            break;
        }

        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_DebugProcessList.Mutex);
    return result;
}

BOOLEAN DetachDebugTargetProcessObject(_EPROCESS* Process, PDEBUG_OBJECT DebugObject, PBOOLEAN DebugObjectReferenced)
{
    if (DebugObjectReferenced == NULL)
    {
        return FALSE;
    }

    *DebugObjectReferenced = FALSE;
    if (Process == NULL || DebugObject == NULL || KeGetCurrentIrql() > APC_LEVEL)
    {
        return FALSE;
    }

    PDEBUG_PROCESS detachedEntry = NULL;
    ExAcquireFastMutex(&g_DebugProcessList.Mutex);
    PLIST_ENTRY listHead = &g_DebugProcessList.list_entry.ListHead;
    for (PLIST_ENTRY current = listHead->Flink; current != listHead; current = current->Flink)
    {
        PDEBUG_PROCESS entry = CONTAINING_RECORD(current, DEBUG_PROCESS, list_entry);
        if (entry->Process == Process && entry->DebugObject == DebugObject)
        {
            RemoveEntryList(current);
            if (g_DebugProcessList.list_entry.Count != 0)
            {
                g_DebugProcessList.list_entry.Count--;
            }

            *DebugObjectReferenced = entry->DebugObjectReferenced;
            entry->Process = NULL;
            entry->DebugObject = NULL;
            entry->DebugObjectReferenced = FALSE;
            detachedEntry = entry;
            break;
        }
    }
    ExReleaseFastMutex(&g_DebugProcessList.Mutex);

    if (detachedEntry != NULL)
    {
        free_pool(detachedEntry);
        return TRUE;
    }

    return FALSE;
}

BOOLEAN InsertDebuggerList(PDEBUGGER_TABLE_ENTRY Debugger)
{
    if (Debugger == NULL || KeGetCurrentIrql() > APC_LEVEL)
    {
        return FALSE;
    }

    ExAcquireFastMutex(&g_DebuggerList.Mutex);
    if (IsDbgkUnloadInProgress())
    {
        ExReleaseFastMutex(&g_DebuggerList.Mutex);
        return FALSE;
    }

    InsertNode(&g_DebuggerList.list_entry, &Debugger->list_entry);
    ExReleaseFastMutex(&g_DebuggerList.Mutex);
    return TRUE;
}

BOOLEAN InsertDebugProcessList(PDEBUG_PROCESS debug_process)
{
    if (debug_process == NULL || KeGetCurrentIrql() > APC_LEVEL)
    {
        return FALSE;
    }

    ExAcquireFastMutex(&g_DebugProcessList.Mutex);
    if (IsDbgkUnloadInProgress())
    {
        ExReleaseFastMutex(&g_DebugProcessList.Mutex);
        return FALSE;
    }

    InsertNode(&g_DebugProcessList.list_entry, &debug_process->list_entry);
    ExReleaseFastMutex(&g_DebugProcessList.Mutex);
    return TRUE;
}

//移除元素
VOID DeleteDebugProcess(PDEBUG_OBJECT DebugObject)
{
    PLIST_ENTRY ListHead, NextEntry;
    PDEBUG_PROCESS entry;

    if (DebugObject == NULL || IsDbgkUnloadInProgress() || KeGetCurrentIrql() > APC_LEVEL)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    ExAcquireFastMutex(&g_DebugProcessList.Mutex);
    ListHead = &g_DebugProcessList.list_entry.ListHead;
    NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        entry = CONTAINING_RECORD(NextEntry,
            DEBUG_PROCESS,
            list_entry);

        if (entry)
        {
            if (entry->DebugObject == DebugObject)
            {
                RemoveEntryList(NextEntry);
                if (g_DebugProcessList.list_entry.Count != 0)
                {
                    g_DebugProcessList.list_entry.Count--;
                }
                free_pool(entry);
                break;
            }
        }

        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_DebugProcessList.Mutex);
}

//设置调试对象
BOOLEAN SetDebugTargetProcess(_EPROCESS* Process, PDEBUG_OBJECT DebugObject)
{
    BOOLEAN result = FALSE;

    if (Process == NULL || DebugObject == NULL || IsDbgkUnloadInProgress() || KeGetCurrentIrql() > APC_LEVEL)
    {
        return FALSE;
    }

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
        PDEBUG_PROCESS entry = allocate_pool<DEBUG_PROCESS>();
        if (entry == NULL)
        {
            return FALSE;
        }

        RtlZeroMemory(entry, sizeof(DEBUG_PROCESS));
        entry->Process = Process;
        entry->DebugObject = DebugObject;
        ExInitializeFastMutex(&entry->Mutex);
        if (InsertDebugProcessList(entry))
        {
            // *DebugProcess = entry;
            result = TRUE;
        }
        else
        {
            free_pool(entry);
        }
    }
    return result;
}

BOOLEAN IsDebugTargetProcess(
    IN _EPROCESS* Process,
    OUT PDEBUG_PROCESS* DebugProcess)
{
    BOOLEAN result = FALSE;

    if (DebugProcess == NULL)
    {
        return FALSE;
    }

    *DebugProcess = NULL;

    if (Process == NULL || IsDbgkUnloadInProgress() || KeGetCurrentIrql() > APC_LEVEL)
    {
        return FALSE;
    }

    PLIST_ENTRY ListHead, NextEntry;
    PDEBUG_PROCESS entry;

    ExAcquireFastMutex(&g_DebugProcessList.Mutex);
    ListHead = &g_DebugProcessList.list_entry.ListHead;
    NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        entry = CONTAINING_RECORD(NextEntry,
            DEBUG_PROCESS,
            list_entry);

        if (entry)
        {
            if (entry->Process == Process)
            {
                *DebugProcess = entry;
                // *DebugProcess = entry;
                result = TRUE;
                break;
            }
        }

        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_DebugProcessList.Mutex);
    return result;
}

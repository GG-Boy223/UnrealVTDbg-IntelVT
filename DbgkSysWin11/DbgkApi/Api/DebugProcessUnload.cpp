#include "DebugObjectCommonInternal.h"

namespace
{
    VOID DetachDebugProcessEntry(PDEBUG_PROCESS DebugProcess)
    {
        if (DebugProcess == NULL)
        {
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return;
        }

        PDEBUG_OBJECT DebugObject = NULL;
        _EPROCESS* Process = NULL;
        BOOLEAN debugObjectReferenced = FALSE;

        ExAcquireFastMutex(&DebugProcess->Mutex);
        DebugObject = DebugProcess->DebugObject;
        Process = DebugProcess->Process;
        debugObjectReferenced = DebugProcess->DebugObjectReferenced;
        DebugProcess->Process = NULL;
        DebugProcess->DebugObject = NULL;
        DebugProcess->DebugObjectReferenced = FALSE;
        ExReleaseFastMutex(&DebugProcess->Mutex);

        if (DebugObject != NULL && debugObjectReferenced)
        {
            // /* Reference the debug object */
            ObReferenceObject(DebugObject);
            WakeDebugObjectEvents(DebugObject);
        }

        if (Process != NULL && DebugObject != NULL && debugObjectReferenced && eprocess_offset::DebugPort != 0xFFFFFFFF)
        {
            BOOLEAN portLocked = FALSE;
            __try
            {
                size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
                // /* Acquire the port lock */
                ExAcquireFastMutex(&DbgkpProcessDebugPortMutex);
                portLocked = TRUE;

                if (*(PDEBUG_OBJECT*)ptr_DebugPort == DebugObject)
                {
                    *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                }

                ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);
                portLocked = FALSE;

                if (DbgkpMarkProcessPeb != NULL)
                {
                    DbgkpMarkProcessPeb(Process); //清理进程的peb结构
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                if (portLocked)
                {
                    ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);
                }
            }
        }

        if (DebugObject != NULL && debugObjectReferenced)
        {
            ObDereferenceObject(DebugObject); // process-debug reference
            ObDereferenceObject(DebugObject); // temporary cleanup reference
        }
    }

    VOID ReleaseDebugProcessTable(PDEBUG_PROCESS_TABLE Table, BOOLEAN DetachEntries)
    {
        if (Table == NULL)
        {
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return;
        }

        for (;;)
        {
            PDEBUG_PROCESS entry = NULL;

            ExAcquireFastMutex(&Table->Mutex);
            if (!IsListEmpty(&Table->list_entry.ListHead))
            {
                PLIST_ENTRY currentEntry = RemoveHeadList(&Table->list_entry.ListHead);
                if (Table->list_entry.Count != 0)
                {
                    Table->list_entry.Count--;
                }

                entry = CONTAINING_RECORD(currentEntry, DEBUG_PROCESS, list_entry);
            }
            else
            {
                InitializeListHead(&Table->list_entry.ListHead);
                Table->list_entry.Count = 0;
            }
            ExReleaseFastMutex(&Table->Mutex);

            if (entry == NULL)
            {
                break;
            }

            if (DetachEntries)
            {
                DetachDebugProcessEntry(entry);
            }

            free_pool(entry);
        }
    }
}

VOID QueueDebugProcessListForUnload()
{
    ExAcquireFastMutex(&g_DebugProcessList.Mutex);
    ExAcquireFastMutex(&g_DebugProcessUnloadList.Mutex);

    while (!IsListEmpty(&g_DebugProcessList.list_entry.ListHead))
    {
        PLIST_ENTRY entry = RemoveHeadList(&g_DebugProcessList.list_entry.ListHead);
        if (g_DebugProcessList.list_entry.Count != 0)
        {
            g_DebugProcessList.list_entry.Count--;
        }

        InsertTailList(&g_DebugProcessUnloadList.list_entry.ListHead, entry);
        g_DebugProcessUnloadList.list_entry.Count++;
    }

    InitializeListHead(&g_DebugProcessList.list_entry.ListHead);
    g_DebugProcessList.list_entry.Count = 0;

    ExReleaseFastMutex(&g_DebugProcessUnloadList.Mutex);
    ExReleaseFastMutex(&g_DebugProcessList.Mutex);
}

VOID DetachDebugProcessUnloadList()
{
    PLIST_ENTRY listHead = &g_DebugProcessUnloadList.list_entry.ListHead;
    PLIST_ENTRY nextEntry = listHead->Flink;

    while (listHead != nextEntry)
    {
        PDEBUG_PROCESS entry = CONTAINING_RECORD(nextEntry, DEBUG_PROCESS, list_entry);
        nextEntry = nextEntry->Flink;
        DetachDebugProcessEntry(entry);
    }
}

//释放列表
VOID ReleaseDebugProcessList()
{
    ReleaseDebugProcessTable(&g_DebugProcessList, TRUE);
    ReleaseDebugProcessTable(&g_DebugProcessUnloadList, FALSE);
}

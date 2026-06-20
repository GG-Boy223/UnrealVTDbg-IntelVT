#include "ReadWriteInternal.h"

VOID InsertBreakpointList(PBREAKPOINT_TABLE_ENTRY entry)
{
    if (entry)
    {
        ExAcquireFastMutex(&g_BreakpointList.Mutex);
        InsertNode(&g_BreakpointList.list_entry, &entry->list_entry);
        ExReleaseFastMutex(&g_BreakpointList.Mutex);
    }
}

VOID TraceBreakpoint(_EPROCESS* Process, PVOID BaseAddress, PVOID Buffer)
{
    PBREAKPOINT_TABLE_ENTRY entry = allocate_pool<BREAKPOINT_TABLE_ENTRY>();
    if (entry)
    {
        entry->Process = Process;
        entry->BreakpointAddress = BaseAddress;
        InsertBreakpointList(entry);
    }
}

VOID DeleteBreakpoint(_EPROCESS* Process, PVOID BaseAddress, PVOID Buffer)
{
    PLIST_ENTRY ListHead, NextEntry;
    PBREAKPOINT_TABLE_ENTRY entry;

    ExAcquireFastMutex(&g_BreakpointList.Mutex);
    ListHead = &g_BreakpointList.list_entry.ListHead;
    NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        entry = CONTAINING_RECORD(NextEntry,
            BREAKPOINT_TABLE_ENTRY,
            list_entry);

        if (entry)
        {
            if (entry->Process == Process &&
                entry->BreakpointAddress == BaseAddress)
            {
                RemoveEntryList(NextEntry);
                free_pool(entry);
                break;
            }
        }

        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_BreakpointList.Mutex);
}

BOOLEAN exist_breakpoint(_EPROCESS* Process, PVOID BaseAddress)
{
    PLIST_ENTRY ListHead, NextEntry;
    PBREAKPOINT_TABLE_ENTRY entry;
    BOOLEAN exist = FALSE;

    ExAcquireFastMutex(&g_BreakpointList.Mutex);
    ListHead = &g_BreakpointList.list_entry.ListHead;
    NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        entry = CONTAINING_RECORD(NextEntry,
            BREAKPOINT_TABLE_ENTRY,
            list_entry);

        if (entry)
        {
            if (entry->Process == Process &&
                entry->BreakpointAddress == BaseAddress)
            {
                exist = TRUE;
                break;
            }
        }

        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_BreakpointList.Mutex);
    return exist;
}

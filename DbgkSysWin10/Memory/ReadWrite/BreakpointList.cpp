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
        //entry->firstByte = *(BYTE*)Buffer;
        //entry->originalByte = *(BYTE*)BaseAddress;
        // 持有 EPROCESS 引用：断点表记录的是裸指针。若不引用，目标进程退出后
        // EPROCESS 池块可能被新进程复用（ABA），exist_breakpoint 会把新进程误判为
        // 已安装断点的旧进程，导致 0xCC 写入被静默吞掉或删除错误的 VT watch。
        if (Process != NULL)
        {
            ObReferenceObject(Process);
        }
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
                if (entry->Process != NULL)
                {
                    ObDereferenceObject(entry->Process);
                }
                free_pool(entry);
                break;
            }
        }

        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_BreakpointList.Mutex);
}

//检查断点是否已存在
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

        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_BreakpointList.Mutex);
    return exist;
}

// 释放断点表：条目在 TraceBreakpoint 中持有目标 EPROCESS 引用，
// 卸载时必须成对释放；不能复用通用 ReleaseListEntries（它只 free_pool 条目本身）。
VOID ReleaseBreakpointList()
{
    ExAcquireFastMutex(&g_BreakpointList.Mutex);
    while (!IsListEmpty(&g_BreakpointList.list_entry.ListHead))
    {
        PLIST_ENTRY current = RemoveHeadList(&g_BreakpointList.list_entry.ListHead);
        PBREAKPOINT_TABLE_ENTRY entry =
            CONTAINING_RECORD(current, BREAKPOINT_TABLE_ENTRY, list_entry);
        if (entry->Process != NULL)
        {
            ObDereferenceObject(entry->Process);
        }
        free_pool(entry);
    }
    g_BreakpointList.list_entry.Count = 0;
    ExReleaseFastMutex(&g_BreakpointList.Mutex);
}

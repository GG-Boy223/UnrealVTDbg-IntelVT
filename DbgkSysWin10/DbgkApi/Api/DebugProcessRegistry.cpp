#include "DebugObjectCommonInternal.h"
#include "DebugObjectCommon.h"

namespace
{
    // 退役而不是立即 free：IsDebugTargetProcess() 把 DEBUG_PROCESS 指针交给调用方在锁外使用
    // （典型：Protect/Thread/ProtectDrxX64.cpp 会 ExAcquireFastMutex(&DebugProcess->Mutex)
    // 并读写 shadow Context/wow64Context），而 NtRemoveProcessDebug → DeleteDebugProcess、
    // DbgkpCloseObject → DetachDebugTargetProcessObject 可能并发摘链。
    // 若在这两条路径上直接 free_pool(entry)，在途调用会写入已释放池块（S1 UAF）。
    //
    // 退役语义：把表项转入 g_DebugProcessUnloadList，真正的 free 只发生在
    // DbgkUnInitialize() → QueueDebugProcessListForUnload → DetachDebugProcessUnloadList →
    // WaitForDbgkStateOpsDrain → ReleaseDebugProcessList 之后，即所有带
    // DbgkRuntimeCallGuard 的在途调用都已退出时才释放。
    // 表项的 Process/DebugObject 在退役前已清空，持有陈旧指针的调用方只会读到空值，
    // 其 Mutex 仍然有效；退役产生的内存由进程生命周期内的 attach/detach 次数决定，卸载时全部回收。
    VOID RetireDebugProcessEntry(PDEBUG_PROCESS entry)
    {
        if (entry == NULL)
        {
            return;
        }

        ExAcquireFastMutex(&g_DebugProcessUnloadList.Mutex);
        if (!entry->Retired)
        {
            entry->Retired = TRUE;
            InsertTailList(&g_DebugProcessUnloadList.list_entry.ListHead, &entry->list_entry);
            g_DebugProcessUnloadList.list_entry.Count++;
        }
        ExReleaseFastMutex(&g_DebugProcessUnloadList.Mutex);
    }
}

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
        RetireDebugProcessEntry(detachedEntry);
        return TRUE;
    }

    return FALSE;
}

// 在 g_DebugProcessList.Mutex 保护下登记“调试对象额外引用”的所有权。
//
// 根因：DbgkpSetProcessDebugObject 旧逻辑先用 IsDebugTargetProcess() 取到表项指针，
// 释放全局锁之后才写 entry->DebugObjectReferenced = TRUE。而驱动卸载路径
// （QueueDebugProcessListForUnload -> DetachDebugProcessUnloadList -> free_pool）
// 与 WaitForDbgkStateOpsDrain() 的顺序相反：先摘链/释放表项，再等待在途运行期调用结束。
// 于是一个还在 DbgkpSetProcessDebugObject 里的 NtDebugActiveProcess 可以把标志写进
// 已被 free_pool 释放的表项（释放后写入 = 池内存破坏），或让已取得的 DEBUG_OBJECT
// 引用永远无人释放（引用泄漏）。
//
// 修复：登记动作与表项查找放在同一把全局锁内。表项一旦被卸载事务摘链，
// 本函数必然查找失败；调用方随即自行返还刚取得的引用，不再存在写已释放表项的窗口。
BOOLEAN MarkDebugProcessDebugObjectReferenced(_EPROCESS* Process, PDEBUG_OBJECT DebugObject)
{
    BOOLEAN result = FALSE;

    if (Process == NULL || DebugObject == NULL || KeGetCurrentIrql() > APC_LEVEL)
    {
        return FALSE;
    }

    ExAcquireFastMutex(&g_DebugProcessList.Mutex);
    if (!IsDbgkUnloadInProgress())
    {
        PLIST_ENTRY ListHead = &g_DebugProcessList.list_entry.ListHead;
        for (PLIST_ENTRY NextEntry = ListHead->Flink; NextEntry != ListHead; NextEntry = NextEntry->Flink)
        {
            PDEBUG_PROCESS entry = CONTAINING_RECORD(NextEntry, DEBUG_PROCESS, list_entry);
            if (entry->Process == Process && entry->DebugObject == DebugObject)
            {
                entry->DebugObjectReferenced = TRUE;
                result = TRUE;
                break;
            }
        }
    }
    ExReleaseFastMutex(&g_DebugProcessList.Mutex);

    return result;
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
//
// 必须同时按 (Process, DebugObject) 匹配：同一个 DEBUG_OBJECT 允许被
// NtDebugActiveProcess 附加到多个目标进程（Windows 调试语义即为多进程调试）。
// 旧实现只比较 DebugObject，NtRemoveProcessDebug(B) 会删掉列表中第一个
// DebugObject 相同的表项（可能是 A 的），导致：
//   1) B 的虚拟调试表项残留，移除后事件仍按 B 继续派发；
//   2) A 的表项被误删时其 DebugObjectReferenced 引用被提前释放，B 表项在卸载
//      清理时还会再释放一次，DEBUG_OBJECT 引用计数下溢 → 提前销毁（UAF 风险）。
VOID DeleteDebugProcess(_EPROCESS* Process, PDEBUG_OBJECT DebugObject)
{
    PLIST_ENTRY ListHead, NextEntry;
    PDEBUG_PROCESS entry;
    BOOLEAN releaseDebugObjectReference = FALSE;

    if (Process == NULL || DebugObject == NULL || IsDbgkUnloadInProgress() || KeGetCurrentIrql() > APC_LEVEL)
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
            if (entry->Process == Process && entry->DebugObject == DebugObject)
            {
                // DbgkpSetProcessDebugObject 在虚拟调试目标上额外 ObReferenceObject 了一次，
                // 并把所有权记录在 DebugObjectReferenced。DetachDebugProcessEntry /
                // DetachDebugTargetProcessObject 都会成对释放；这里删除表项同样必须释放，
                // 否则每次 NtRemoveProcessDebug 都会泄漏一次 DEBUG_OBJECT 引用。
                releaseDebugObjectReference = entry->DebugObjectReferenced;
                entry->Process = NULL;
                entry->DebugObject = NULL;
                entry->DebugObjectReferenced = FALSE;
                RemoveEntryList(NextEntry);
                if (g_DebugProcessList.list_entry.Count != 0)
                {
                    g_DebugProcessList.list_entry.Count--;
                }
                RetireDebugProcessEntry(entry);
                break;
            }
        }

        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_DebugProcessList.Mutex);

    if (releaseDebugObjectReference)
    {
        ObDereferenceObject(DebugObject);
    }
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

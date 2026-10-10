#include "../HookInternal.h"

void CleanupDebugSessionState(const char* reason)
{
    if (InterlockedCompareExchange(&HookInternal::DebugSessionCleanupInProgress, 1, 0) != 0)
    {
        return;
    }

    const char* cleanupReason = reason ? reason : "";
    std::vector<VT_BREAK_POINT> int3Breakpoints;
    int3Breakpoints.reserve(8);

    const bool startupBreakpointRestored =
        RestoreBaseThreadInitThunkStartupBreakpoint();
    if (!startupBreakpointRestored)
    {
        logger.Log("调试会话清理: 启动断点恢复失败: errorCode=1007, source=%s", cleanupReason);
    }

    INT3BreakpointList.Lock();
    for (const auto& breakpoint : INT3BreakpointList)
    {
        int3Breakpoints.push_back(breakpoint);
    }
    INT3BreakpointList.UnLock();

    int releasedInt3Count = 0;
    for (const auto& breakpoint : int3Breakpoints)
    {
        if (HookInternal::DeleteInt3BreakpointFromDriver(breakpoint))
        {
            ++releasedInt3Count;
            continue;
        }

        // 旧日志包含 watchid，避免在日志文件暴露内部断点编号。
        // logger.Log("调试会话清理: 删除隐藏int3失败 地址:%p watchid:%d 来源:%s", ...);
        logger.Log("调试会话清理: 删除隐藏int3失败: errorCode=1010, addr=0x%llX, cr3=0x%llX, watchid=%d, pid=%u, source=%s",
            breakpoint.VirtualAddress,
            breakpoint.cr3,
            breakpoint.watchid,
            breakpoint.pid,
            cleanupReason);
    }

    // staleInt3Count 必须表示驱动删除失败、真正残留的隐藏 INT3：本地
    // INT3BreakpointList 会在本函数末尾无条件清空，若直接取列表大小，已成功
    // 释放的记录也会被计成残留（出现 releasedInt3=1/1 仍报 errorCode=1008）。
    const auto staleInt3Count = static_cast<unsigned long long>(int3Breakpoints.size() - releasedInt3Count);

    INT3BreakpointList.Lock();
    INT3BreakpointList.clear();
    HookInternal::Int3BreakpointMetaList.clear();
    HookInternal::ResetActiveHiddenInt3Count();
    INT3BreakpointList.UnLock();

    RemoveBreakpoint();

    BreakpointList.Lock();
    const auto staleHardwareCount = static_cast<unsigned long long>(BreakpointList.size());
    BreakpointList.clear();
    InterlockedExchange(&g_active_hardware_breakpoint_count, 0);
    BreakpointList.UnLock();

    InterlockedExchange(&g_debug_condition_detected, 0);
    InterlockedExchange(&g_debug_register_status, 0);

    DebugHitList.Lock();
    DebugHitList.clear();
    DebugHitList.UnLock();

    clear_vt_breakpoint_state();

    g_first_breakpoint = FALSE;
    g_SetDbgBreakPoint = {};
    g_process_info = {};
    g_target_pid = 0;
    g_target_cr3 = 0;

    logger.Log("调试会话清理完成 来源:%s 启动断点恢复:%d 隐藏int3:%d 未释放int3:%llu 硬件残留:%llu",
        cleanupReason,
        startupBreakpointRestored ? 1 : 0,
        releasedInt3Count,
        staleInt3Count,
        staleHardwareCount);

    if (!startupBreakpointRestored ||
        releasedInt3Count < static_cast<int>(int3Breakpoints.size()) ||
        staleInt3Count != 0 ||
        staleHardwareCount != 0)
    {
        logger.Log("调试会话清理未完全成功: errorCode=1008, source=%s, startupRestored=%d, releasedInt3=%d/%zu, staleInt3=%llu, staleHw=%llu",
            cleanupReason,
            startupBreakpointRestored ? 1 : 0,
            releasedInt3Count,
            int3Breakpoints.size(),
            staleInt3Count,
            staleHardwareCount);
    }

    InterlockedExchange(&HookInternal::DebugSessionCleanupInProgress, 0);
}

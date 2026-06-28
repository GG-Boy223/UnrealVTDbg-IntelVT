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
        logger.Log("调试会话清理: 删除隐藏int3失败 地址:%p 来源:%s",
            reinterpret_cast<PVOID>(breakpoint.VirtualAddress),
            cleanupReason);
    }

    INT3BreakpointList.Lock();
    const auto staleInt3Count = static_cast<unsigned long long>(INT3BreakpointList.size());
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

    logger.Log("调试会话清理完成 来源:%s 隐藏int3:%d 本地int3残留:%llu 硬件残留:%llu",
        cleanupReason,
        releasedInt3Count,
        staleInt3Count,
        staleHardwareCount);

    InterlockedExchange(&HookInternal::DebugSessionCleanupInProgress, 0);
}

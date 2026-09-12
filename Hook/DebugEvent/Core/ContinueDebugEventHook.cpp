#include "DebugEventInternal.h"
#include "../../HookCallSet/Hooks/HookInternal.h"

BOOL
WINAPI
NewContinueDebugEvent(
    _In_ DWORD dwProcessId,
    _In_ DWORD dwThreadId,
    _In_ DWORD dwContinueStatus
)
{
    if (g_process_info.isCreate && dwContinueStatus == DBG_CONTINUE && g_first_breakpoint)
    {
        g_first_breakpoint = FALSE;
        if (g_SetDbgBreakPoint.BaseThreadInitThunkPatched &&
            !RestoreBaseThreadInitThunkStartupBreakpoint())
        {
            logger.Log("自动取消启动断点恢复失败，会话清理时会再次尝试");
        }
    }
    // CE/x64dbg 的手动 F5 软件断点必须保持到用户再次 F5/删除。
    // 这里不能在 Continue 时主动删除隐藏 INT3；真正删除只由写回原字节路径处理。
    const BOOL continueResult = Sys_ContinueDebugEvent(dwProcessId, dwThreadId, dwContinueStatus);
    if (continueResult)
    {
        DebugEventInternal::ClearDebugHitRecord(dwThreadId);
    }
    return continueResult;
}

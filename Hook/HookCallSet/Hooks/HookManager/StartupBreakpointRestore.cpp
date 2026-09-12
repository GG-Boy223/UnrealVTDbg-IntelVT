#include "../HookInternal.h"

// 返回值表示是否已恢复原始字节。目标已退出或地址无效时返回 true,避免调用方再次重试。
bool RestoreBaseThreadInitThunkStartupBreakpoint()
{
    if (!g_SetDbgBreakPoint.BaseThreadInitThunkPatched)
    {
        return true;
    }

    if (!BaseThreadInitThunk)
    {
        g_SetDbgBreakPoint.BaseThreadInitThunkPatched = FALSE;
        return true;
    }

    if (!g_process_info.ProcessHandle)
    {
        g_SetDbgBreakPoint.BaseThreadInitThunkPatched = FALSE;
        return true;
    }

    const PVOID breakpointAddress = (PVOID)((ULONG_PTR)BaseThreadInitThunk + 4);
    DWORD oldProtect = 0;
    const PFN_VIRTUALPROTECTEX virtualProtectEx =
        Sys_VirtualProtectEx ? Sys_VirtualProtectEx : VirtualProtectEx;
    const PFN_WRITEPROCESSMEMORY writeProcessMemory =
        Sys_WriteProcessMemory ? Sys_WriteProcessMemory : WriteProcessMemory;

    if (!virtualProtectEx(
            g_process_info.ProcessHandle,
            breakpointAddress,
            sizeof(g_SetDbgBreakPoint.BaseThreadInitThunkOriginalBytes),
            PAGE_EXECUTE_READWRITE,
            &oldProtect))
    {
        logger.Log("恢复启动断点失败 VirtualProtectEx error:%lu", GetLastError());
        return false;
    }

    BOOL wrote = writeProcessMemory(
        g_process_info.ProcessHandle,
        breakpointAddress,
        g_SetDbgBreakPoint.BaseThreadInitThunkOriginalBytes,
        sizeof(g_SetDbgBreakPoint.BaseThreadInitThunkOriginalBytes),
        NULL);
    const DWORD writeError = wrote ? 0 : GetLastError();

    virtualProtectEx(
        g_process_info.ProcessHandle,
        breakpointAddress,
        sizeof(g_SetDbgBreakPoint.BaseThreadInitThunkOriginalBytes),
        oldProtect,
        &oldProtect);

    if (!wrote)
    {
        logger.Log("恢复启动断点失败 WriteProcessMemory error:%lu", writeError);
        return false;
    }

    g_SetDbgBreakPoint.BaseThreadInitThunkPatched = FALSE;
    g_SetDbgBreakPoint.boBaseThreadInitThunk = TRUE;
    return true;
}

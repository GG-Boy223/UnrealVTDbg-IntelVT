#include "DllMainInternal.h"
#include "../../HookCallSet/Hooks/HookInstallRoutines.h"

namespace
{
    constexpr LONG kHookStateNotInstalled = 0;
    constexpr LONG kHookStateInstalled = 1;
    constexpr LONG kHookStateUninstalling = 2;

    typedef bool (*HookInstallRoutine)();
    typedef bool (*HookUninstallRoutine)();

    struct HookStep
    {
        const char* name;
        HookInstallRoutine install;
        HookUninstallRoutine uninstall;
        bool installed;
    };

    HookStep g_HookSteps[] =
    {
        // 旧项目保留的禁用路径：TryHook_DbgUiDebugActiveProcess 是 DbgUiDebugActiveProcess -> NtDebugActiveProcess
        // -> DbgUiIssueRemoteBreakin 的更上层封装入口。
        // 为什么注释：当前安装表从 NtDebugActiveProcess 和 DbgUiIssueRemoteBreakin 两层拦截，已经覆盖关键调试附加与远程
        // break-in 行为；再开启上层 DbgUi hook 会增加重复拦截和递归风险。
        // 放开的好处：可以更早观察 Win32/DbgUi 调试附加入口，便于做 API 层审计。
        // 放开的坏处：和下层 Nt* hook 重叠，异常路径中可能重复记录、重复修改 break-in 线程。
        // 项目影响：会扩大 Hook.dll/Hook64.dll 对 ntdll 调试 API 的干预面，提升兼容性回归风险。
        // 结论：继续注释；只有在明确需要上层 API 观测时再作为单独 HookStep 加入。
        //{ "DbgUiDebugActiveProcess", TryHook_DbgUiDebugActiveProcess, TryUnHook_DbgUiDebugActiveProcess, false },
        { "DebugActiveProcess", TryHook_DebugActiveProcess, TryUnHook_DebugActiveProcess, false }, //注入？
        { "DebugActiveProcessStop", TryHook_DebugActiveProcessStop, TryUnHook_DebugActiveProcessStop, false },
        { "TerminateProcess", TryHook_TerminateProcess, TryUnHook_TerminateProcess, false },
        { "NtDebugActiveProcess", TryHook_NtDebugActiveProcess, TryUnHook_NtDebugActiveProcess, false },
        { "NtRemoveProcessDebug", TryHook_NtRemoveProcessDebug, TryUnHook_NtRemoveProcessDebug, false },
        { "NtTerminateProcess", TryHook_NtTerminateProcess, TryUnHook_NtTerminateProcess, false },
        { "DbgUiIssueRemoteBreakin", TryHook_DbgUiIssueRemoteBreakin, TryUnHook_DbgUiIssueRemoteBreakin, false },
        { "WaitForDebugEvent", TryHook_WaitForDebugEvent, TryUnHook_WaitForDebugEvent, false },
        { "ContinueDebugEvent", TryHook_ContinueDebugEvent, TryUnHook_ContinueDebugEvent, false },
        { "NtDebugContinue", TryHook_NtDebugContinue, TryUnHook_NtDebugContinue, false },
        { "OutputDebugStringA", TryHook_OutputDebugStringA, TryUnHook_OutputDebugStringA, false },
        { "OutputDebugStringW", TryHook_OutputDebugStringW, TryUnHook_OutputDebugStringW, false },
        { "SetThreadContext", TryHook_SetThreadContext, TryUnHook_SetThreadContext, false },
        { "GetThreadContext", TryHook_GetThreadContext, TryUnHook_GetThreadContext, false },
        { "NtWriteVirtualMemory", TryHook_NtWriteVirtualMemory, TryUnHook_NtWriteVirtualMemory, false },
        { "NtReadVirtualMemory", TryHook_NtReadVirtualMemory, TryUnHook_NtReadVirtualMemory, false },
        { "WriteProcessMemory", TryHook_WriteProcessMemory, TryUnHook_WriteProcessMemory, false },
        { "ReadProcessMemory", TryHook_ReadProcessMemory, TryUnHook_ReadProcessMemory, false },
        { "VirtualProtectEx", TryHook_VirtualProtectEx, TryUnHook_VirtualProtectEx, false },
    };

    bool InstallAllHooks()
    {
        bool allInstalled = true;
        for (HookStep& step : g_HookSteps)
        {
            if (!step.install())
            {
                logger.Log("安装%s钩子失败", step.name);
                allInstalled = false;
                continue;
            }

            step.installed = true;
        }

        return allInstalled;
    }

    bool TryUninstallHook(const char* name, HookUninstallRoutine uninstall)
    {
        if (!uninstall())
        {
            logger.Log("卸载%s钩子失败", name);
            return false;
        }

        return true;
    }

    bool UninstallInstalledHooks()
    {
        bool success = true;

        // Keep the legacy teardown order. The original project also attempted to
        // unhook the disabled DbgUiDebugActiveProcess and NtCreateUserProcess paths.
        success = TryUninstallHook("DebugActiveProcess", TryUnHook_DebugActiveProcess) && success;
        success = TryUninstallHook("DebugActiveProcessStop", TryUnHook_DebugActiveProcessStop) && success;
        success = TryUninstallHook("TerminateProcess", TryUnHook_TerminateProcess) && success;
        success = TryUninstallHook("NtDebugActiveProcess", TryUnHook_NtDebugActiveProcess) && success;
        success = TryUninstallHook("NtRemoveProcessDebug", TryUnHook_NtRemoveProcessDebug) && success;
        success = TryUninstallHook("NtTerminateProcess", TryUnHook_NtTerminateProcess) && success;
        success = TryUninstallHook("DbgUiIssueRemoteBreakin", TryUnHook_DbgUiIssueRemoteBreakin) && success;
        success = TryUninstallHook("DbgUiDebugActiveProcess", TryUnHook_DbgUiDebugActiveProcess) && success;
        success = TryUninstallHook("NtCreateUserProcess", TryUnHook_NtCreateUserProcess) && success;
        success = TryUninstallHook("WaitForDebugEvent", TryUnHook_WaitForDebugEvent) && success;
        success = TryUninstallHook("ContinueDebugEvent", TryUnHook_ContinueDebugEvent) && success;
        success = TryUninstallHook("NtDebugContinue", TryUnHook_NtDebugContinue) && success;
        success = TryUninstallHook("OutputDebugStringA", TryUnHook_OutputDebugStringA) && success;
        success = TryUninstallHook("OutputDebugStringW", TryUnHook_OutputDebugStringW) && success;
        success = TryUninstallHook("SetThreadContext", TryUnHook_SetThreadContext) && success;
        success = TryUninstallHook("GetThreadContext", TryUnHook_GetThreadContext) && success;
        success = TryUninstallHook("WriteProcessMemory", TryUnHook_WriteProcessMemory) && success;
        success = TryUninstallHook("ReadProcessMemory", TryUnHook_ReadProcessMemory) && success;
        success = TryUninstallHook("NtWriteVirtualMemory", TryUnHook_NtWriteVirtualMemory) && success;
        success = TryUninstallHook("NtReadVirtualMemory", TryUnHook_NtReadVirtualMemory) && success;
        success = TryUninstallHook("VirtualProtectEx", TryUnHook_VirtualProtectEx) && success;

        for (HookStep& step : g_HookSteps)
        {
            step.installed = false;
        }

        return success;
    }
}

bool SetupHook()
{
    if (HookDllMainInternal::g_HookRuntimeStopping != 0)
    {
        logger.Log("Hook runtime is stopping, skip setup");
        return false;
    }

    if (InterlockedCompareExchange(&HookDllMainInternal::g_HookInstalled, kHookStateInstalled, kHookStateNotInstalled) != kHookStateNotInstalled)
    {
        logger.Log("钩子已经安装，跳过重复初始化");
        return true;
    }

    if (InitializeDevice())
    {
        if (!InstallAllHooks())
        {
            logger.Log("部分钩子安装失败，继续保留已安装项");
        }

        logger.Log("初始化成功，钩子安装完毕");
        return true;
    }
    else
    {
        InterlockedExchange(&HookDllMainInternal::g_HookInstalled, kHookStateNotInstalled);
        logger.Log("初始化设备失败");
        return false;
    }
}

bool UnHook()
{
    if (InterlockedCompareExchange(&HookDllMainInternal::g_HookInstalled, kHookStateUninstalling, kHookStateInstalled) != kHookStateInstalled)
    {
        return true;
    }

    if (!UninstallInstalledHooks())
    {
        logger.Log("部分钩子卸载失败，继续清理资源");
    }

    if (g_hGeneralDriverDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hGeneralDriverDevice);
        g_hGeneralDriverDevice = INVALID_HANDLE_VALUE;
    }

    InterlockedExchange(&HookDllMainInternal::g_HookInstalled, kHookStateNotInstalled);
    return true;
}

#include "HookInternal.h"

NTSTATUS
NTAPI
NewNtCreateUserProcess(
    _Out_ PHANDLE ProcessHandle,
    _Out_ PHANDLE ThreadHandle,
    _In_ ACCESS_MASK ProcessDesiredAccess,
    _In_ ACCESS_MASK ThreadDesiredAccess,
    _In_opt_ POBJECT_ATTRIBUTES ProcessObjectAttributes,
    _In_opt_ POBJECT_ATTRIBUTES ThreadObjectAttributes,
    _In_ ULONG ProcessFlags,
    _In_ ULONG ThreadFlags,
    _In_ PRTL_USER_PROCESS_PARAMETERS ProcessParameters,
    _Inout_ PPS_CREATE_INFO CreateInfo,
    _In_ PPS_ATTRIBUTE_LIST AttributeList
)
{
    NTSTATUS Status = Sys_NtCreateUserProcess(ProcessHandle, ThreadHandle, ProcessDesiredAccess, ThreadDesiredAccess, ProcessObjectAttributes, ThreadObjectAttributes,
        ProcessFlags, ThreadFlags, ProcessParameters, CreateInfo, AttributeList);
    if (NT_SUCCESS(Status))
    {
        //DoDebuggerBreak(ProcessHandle);

        // 与附加路径（NewNtDebugActiveProcess）保持同一目标状态不变量：
        // 硬件断点 DRx→VT watch 与隐藏 INT3 都依赖 g_target_pid / g_target_cr3。
        // 创建路径此前只设置 g_process_info，从不解析这两个全局量，导致
        // x64dbg File->Open / CE 创建调试的目标上：
        //   1) HardwareBreakpointDriver::SetBreakpoint 以“cr3 或 pid为空”直接失败；
        //   2) IsHiddenInt3MemoryIoTarget 恒 false，0xCC 退化为真实写入（断点可见）。
        const DWORD newTargetPid = GetProcessId(*ProcessHandle);
        RING3_PROCESS_INFO proc_info = { 0 };
        RING3_PROCESS_INFO output = { 0 };
        proc_info.ProcessHandle = (ULONG64)(*ProcessHandle);
        DWORD BytesReturned = 0;
        const bool targetResolved =
            newTargetPid != 0 &&
            SendUserDataToDriver(IOCTL_GET_PROCESS_INFO,
                &proc_info,
                sizeof(RING3_PROCESS_INFO),
                &output,
                sizeof(RING3_PROCESS_INFO),
                &BytesReturned) &&
            output.cr3 != 0;

        if (targetResolved && g_target_pid != 0 && g_target_pid != newTargetPid)
        {
            CleanupDebugSessionState("NtCreateUserProcess切换目标");
        }

        g_process_info.ProcessHandle = *ProcessHandle;
        g_process_info.isCreate = TRUE;
        if (targetResolved)
        {
            g_target_pid = newTargetPid;
            g_target_cr3 = output.cr3;
            logger.Log("创建调试进程: pid=%u cr3=0x%llX", g_target_pid, g_target_cr3);
        }
        else
        {
            logger.Log("创建调试进程后无法解析目标CR3，硬件断点/隐藏INT3在本次会话不可用");
        }
    }
    //if (NT_SUCCESS(Status))
    //{
    //    //在目标进程内触发int3事件
    //    Status = DbgUiIssueRemoteBreakin(hProcess);
    //    if (!NT_SUCCESS(Status))
    //        ZwRemoveProcessDebug(hProcess, NtCurrentTeb()->DbgSsReserved[1]);
    //}
    return Status;
}

BOOL
WINAPI
NewTerminateProcess(
    _In_ HANDLE hProcess,
    _In_ UINT uExitCode
)
{
    if (HookInternal::IsTrackedDebugTargetProcessHandle(hProcess))
    {
        CleanupDebugSessionState("TerminateProcess");
    }

    return Sys_TerminateProcess(hProcess, uExitCode);
}

NTSTATUS
NTAPI
NewNtRemoveProcessDebug(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE DebugObjectHandle
)
{
    if (HookInternal::IsTrackedDebugTargetProcessHandle(ProcessHandle))
    {
        CleanupDebugSessionState("NtRemoveProcessDebug");
    }

    return Sys_NtRemoveProcessDebug(ProcessHandle, DebugObjectHandle);
}

NTSTATUS
NTAPI
NewNtTerminateProcess(
    _In_opt_ HANDLE ProcessHandle,
    _In_ NTSTATUS ExitStatus
)
{
    if (HookInternal::IsTrackedDebugTargetProcessHandle(ProcessHandle))
    {
        CleanupDebugSessionState("NtTerminateProcess");
    }

    return Sys_NtTerminateProcess(ProcessHandle, ExitStatus);
}

bool TryHook_TerminateProcess()
{
    Sys_TerminateProcess = (PFN_TERMINATEPROCESS)TerminateProcess;
    assert(Sys_TerminateProcess);
    if (Sys_TerminateProcess)
    {
        return HookOn((PVOID*)&Sys_TerminateProcess, NewTerminateProcess, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("TerminateProcess空指针");
        return false;
    }
}

bool TryUnHook_TerminateProcess()
{
    if (Sys_TerminateProcess)
    {
        return HookOff((PVOID*)&Sys_TerminateProcess, NewTerminateProcess, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_NtRemoveProcessDebug()
{
    Sys_NtRemoveProcessDebug = ResolveNtRemoveProcessDebug();
    assert(Sys_NtRemoveProcessDebug);
    if (Sys_NtRemoveProcessDebug)
    {
        return HookOn((PVOID*)&Sys_NtRemoveProcessDebug, NewNtRemoveProcessDebug, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("NtRemoveProcessDebug空指针");
        return false;
    }
}

bool TryUnHook_NtRemoveProcessDebug()
{
    if (Sys_NtRemoveProcessDebug)
    {
        return HookOff((PVOID*)&Sys_NtRemoveProcessDebug, NewNtRemoveProcessDebug, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_NtTerminateProcess()
{
    Sys_NtTerminateProcess = ResolveNtTerminateProcess();
    assert(Sys_NtTerminateProcess);
    if (Sys_NtTerminateProcess)
    {
        return HookOn((PVOID*)&Sys_NtTerminateProcess, NewNtTerminateProcess, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("NtTerminateProcess空指针");
        return false;
    }
}

bool TryUnHook_NtTerminateProcess()
{
    if (Sys_NtTerminateProcess)
    {
        return HookOff((PVOID*)&Sys_NtTerminateProcess, NewNtTerminateProcess, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_NtCreateUserProcess()
{
    Sys_NtCreateUserProcess = ResolveNtCreateUserProcess();
    assert(Sys_NtCreateUserProcess);
    if (Sys_NtCreateUserProcess)
    {
        return HookOn((PVOID*)&Sys_NtCreateUserProcess, NewNtCreateUserProcess, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("NtCreateUserProcess空指针");
        return false;
    }
}

bool TryUnHook_NtCreateUserProcess()
{
    if (Sys_NtCreateUserProcess)
    {
        return HookOff((PVOID*)&Sys_NtCreateUserProcess, NewNtCreateUserProcess, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

void Hook_TerminateProcess()
{
    (void)TryHook_TerminateProcess();
}

void UnHook_TerminateProcess()
{
    (void)TryUnHook_TerminateProcess();
}

void Hook_NtRemoveProcessDebug()
{
    (void)TryHook_NtRemoveProcessDebug();
}

void UnHook_NtRemoveProcessDebug()
{
    (void)TryUnHook_NtRemoveProcessDebug();
}

void Hook_NtTerminateProcess()
{
    (void)TryHook_NtTerminateProcess();
}

void UnHook_NtTerminateProcess()
{
    (void)TryUnHook_NtTerminateProcess();
}

void Hook_NtCreateUserProcess()
{
    (void)TryHook_NtCreateUserProcess();
}

void UnHook_NtCreateUserProcess()
{
    (void)TryUnHook_NtCreateUserProcess();
}

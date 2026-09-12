#include "HookInternal.h"

NTSTATUS NTAPI NewNtDebugActiveProcess(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE DebugObjectHandle)
{
    NTSTATUS Status;

    DWORD BytesReturned = 0;
    RING3_PROCESS_INFO proc_info = { 0 };
    RING3_PROCESS_INFO output = { 0 };
    proc_info.ProcessHandle = (ULONG64)ProcessHandle;
    if (SendUserDataToDriver(IOCTL_GET_PROCESS_INFO,
        &proc_info,
        sizeof(RING3_PROCESS_INFO),
        &output,
        sizeof(RING3_PROCESS_INFO),
        &BytesReturned))
    {
        const DWORD newTargetPid = GetProcessId(ProcessHandle);
        if (!output.cr3 || !newTargetPid)
        {
            ReportSeriousError("cr3 或 pid为空");
            return STATUS_UNSUCCESSFUL;
        }

        // 先完成真实附加,成功后再切换/建立追踪状态,避免附加失败时残留错误目标。
        Status = Sys_NtDebugActiveProcess(ProcessHandle, DebugObjectHandle);
        if (!NT_SUCCESS(Status))
        {
            return Status;
        }

        if (g_target_pid != 0 && g_target_pid != newTargetPid)
        {
            CleanupDebugSessionState("NtDebugActiveProcess切换目标");
        }

        g_target_pid = newTargetPid;
        g_target_cr3 = output.cr3;
        g_process_info.ProcessHandle = ProcessHandle;
        g_process_info.isCreate = FALSE;

        logger.Log("g_target_cr3: 0x%p", g_target_cr3);
        logger.Log("g_target_pid: %d", g_target_pid);

        //if (NT_SUCCESS(Status))
        //{
        //    //在目标进程内触发int3事件
        //    Status = DbgUiIssueRemoteBreakin(hProcess);
        //    if (!NT_SUCCESS(Status))
        //        ZwRemoveProcessDebug(hProcess, NtCurrentTeb()->DbgSsReserved[1]);
        //}
        return Status;
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        ReportSeriousError("无法获取目标进程cr3");
        return STATUS_UNSUCCESSFUL;
    }
}

NTSTATUS
NTAPI
NewDbgUiIssueRemoteBreakin(IN HANDLE Process)
{
    // 旧项目保留代码：曾计划把 NewDbgUiRemoteBreakin 写入目标进程，由远程线程执行 DbgBreakPoint 后 RtlExitUserThread。
    // 注释原因：该方案需要把代码段复制到目标进程并正确处理重定位/权限/线程入口；当前实现已集中到 InjectCode(Process)。
    // 放开好处：更接近 Windows DbgUiIssueRemoteBreakin 的传统语义，可精确控制远程 break-in 线程执行体。
    // 放开坏处：跨进程写代码和执行自定义入口更容易触发 CFG、EDR 或目标进程保护策略，失败路径也更复杂。
    // 项目影响：继续注释时仅使用 DbgUserBreakPoint 导出入口触发断点；放开会改变附加时远程线程入口和异常时序。
    // 当前建议：继续注释。
    //VOID
    //NTAPI
    //NewDbgUiRemoteBreakin(VOID)
    //{
    //    DbgBreakPoint();
    //    RtlExitUserThread(STATUS_SUCCESS);
    //}

    // 旧项目保留代码：RtlCreateUserThread(Process, ..., DbgUiRemoteBreakin, ...) 远程创建 break-in 线程。
    // 注释原因：重构后统一委托 InjectCode，减少此函数同时负责 CR3 查询、调试对象附加和远程线程创建的职责耦合。
    // 放开好处：可避免 CreateRemoteThread 在部分目标策略下受限。
    // 放开坏处：需要恢复 hThread/ClientId/Status 等上下文和 ntdll 原型，且不同位数目标的调用约定更敏感。
    // 项目影响：继续注释不影响当前附加；放开必须同步验证 x86 Hook.dll 和 x64 Hook64.dll 两套路径。
    // 当前建议：继续注释。
    // Status = RtlCreateUserThread(Process,
    //    NULL,
    //    FALSE,
    //    0,
    //    0,
    //    PAGE_SIZE,
    //    (PUSER_THREAD_START_ROUTINE)&DbgUiRemoteBreakin,
    //    NULL,
    //    &hThread,
    //    &ClientId);
    //if (NT_SUCCESS(Status)) NtClose(hThread);
    //return Status;

    if (InjectCode(Process))
    {
        return STATUS_SUCCESS;
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        return STATUS_UNSUCCESSFUL;
    }
}

NTSTATUS NTAPI NewDbgUiDebugActiveProcess(HANDLE hProcess)
{
    PFN_NTDEBUGACTIVEPROCESS ntDebugActiveProcess = ResolveNtDebugActiveProcess();
    if (ntDebugActiveProcess == nullptr)
    {
        logger.Log("NtDebugActiveProcess resolve failed");
        return STATUS_UNSUCCESSFUL;
    }

    NTSTATUS Status = ntDebugActiveProcess(hProcess, NtCurrentTeb()->DbgSsReserved[1]);
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
APIENTRY
NewDebugActiveProcess(
    _In_ DWORD dwProcessId
)
{
    logger.Log("附加到目标进程");
    return Sys_DebugActiveProcess(dwProcessId);
}

BOOL
WINAPI
NewDebugActiveProcessStop(
    _In_ DWORD dwProcessId
)
{
    if (HookInternal::IsTrackedDebugTargetProcessId(dwProcessId))
    {
        CleanupDebugSessionState("DebugActiveProcessStop");
    }

    return Sys_DebugActiveProcessStop(dwProcessId);
}

bool TryHook_DebugActiveProcess()
{
    Sys_DebugActiveProcess = (PFN_DEBUGACTIVEPROCESS)DebugActiveProcess;
    assert(Sys_DebugActiveProcess);
    if (Sys_DebugActiveProcess)
    {
        return HookOn((PVOID*)&Sys_DebugActiveProcess, NewDebugActiveProcess, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("DebugActiveProcess空指针");
        return false;
    }
}

bool TryUnHook_DebugActiveProcess()
{
    if (Sys_DebugActiveProcess)
    {
        return HookOff((PVOID*)&Sys_DebugActiveProcess, NewDebugActiveProcess, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_DebugActiveProcessStop()
{
    Sys_DebugActiveProcessStop = (PFN_DEBUGACTIVEPROCESSSTOP)DebugActiveProcessStop;
    assert(Sys_DebugActiveProcessStop);
    if (Sys_DebugActiveProcessStop)
    {
        return HookOn((PVOID*)&Sys_DebugActiveProcessStop, NewDebugActiveProcessStop, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("DebugActiveProcessStop空指针");
        return false;
    }
}

bool TryUnHook_DebugActiveProcessStop()
{
    if (Sys_DebugActiveProcessStop)
    {
        return HookOff((PVOID*)&Sys_DebugActiveProcessStop, NewDebugActiveProcessStop, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_NtDebugActiveProcess()
{
    Sys_NtDebugActiveProcess = ResolveNtDebugActiveProcess();
    assert(Sys_NtDebugActiveProcess);
    if (Sys_NtDebugActiveProcess)
    {
        return HookOn((PVOID*)&Sys_NtDebugActiveProcess, NewNtDebugActiveProcess, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("NtDebugActiveProcess空指针");
        return false;
    }
}

bool TryUnHook_NtDebugActiveProcess()
{
    if (Sys_NtDebugActiveProcess)
    {
        return HookOff((PVOID*)&Sys_NtDebugActiveProcess, NewNtDebugActiveProcess, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_DbgUiIssueRemoteBreakin()
{
    Sys_DbgUiIssueRemoteBreakin = ResolveDbgUiIssueRemoteBreakin();
    assert(Sys_DbgUiIssueRemoteBreakin);
    if (Sys_DbgUiIssueRemoteBreakin)
    {
        return HookOn((PVOID*)&Sys_DbgUiIssueRemoteBreakin, NewDbgUiIssueRemoteBreakin, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("DbgUiIssueRemoteBreakin空指针");
        return false;
    }
}

bool TryUnHook_DbgUiIssueRemoteBreakin()
{
    if (Sys_DbgUiIssueRemoteBreakin)
    {
        return HookOff((PVOID*)&Sys_DbgUiIssueRemoteBreakin, NewDbgUiIssueRemoteBreakin, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_DbgUiDebugActiveProcess()
{
    Sys_DbgUiDebugActiveProcess = ResolveDbgUiDebugActiveProcess();
    assert(Sys_DbgUiDebugActiveProcess);
    if (Sys_DbgUiDebugActiveProcess)
    {
        return HookOn((PVOID*)&Sys_DbgUiDebugActiveProcess, NewDbgUiDebugActiveProcess, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("DbgUiDebugActiveProcess空指针");
        return false;
    }
}

bool TryUnHook_DbgUiDebugActiveProcess()
{
    if (Sys_DbgUiDebugActiveProcess)
    {
        return HookOff((PVOID*)&Sys_DbgUiDebugActiveProcess, NewDbgUiDebugActiveProcess, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_WaitForDebugEvent()
{
    Sys_WaitForDebugEvent = (PFN_WAITFORDEBUGEVENT)WaitForDebugEvent;
    assert(Sys_WaitForDebugEvent);
    if (Sys_WaitForDebugEvent)
    {
        return HookOn((PVOID*)&Sys_WaitForDebugEvent, NewWaitForDebugEvent, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("WaitForDebugEvent空指针");
        return false;
    }
}

bool TryUnHook_WaitForDebugEvent()
{
    if (Sys_WaitForDebugEvent)
    {
        return HookOff((PVOID*)&Sys_WaitForDebugEvent, NewWaitForDebugEvent, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

void Hook_DebugActiveProcess()
{
    (void)TryHook_DebugActiveProcess();
}

void UnHook_DebugActiveProcess()
{
    (void)TryUnHook_DebugActiveProcess();
}

void Hook_DebugActiveProcessStop()
{
    (void)TryHook_DebugActiveProcessStop();
}

void UnHook_DebugActiveProcessStop()
{
    (void)TryUnHook_DebugActiveProcessStop();
}

void Hook_NtDebugActiveProcess()
{
    (void)TryHook_NtDebugActiveProcess();
}

void UnHook_NtDebugActiveProcess()
{
    (void)TryUnHook_NtDebugActiveProcess();
}

void Hook_DbgUiIssueRemoteBreakin()
{
    (void)TryHook_DbgUiIssueRemoteBreakin();
}

void UnHook_DbgUiIssueRemoteBreakin()
{
    (void)TryUnHook_DbgUiIssueRemoteBreakin();
}

void Hook_DbgUiDebugActiveProcess()
{
    (void)TryHook_DbgUiDebugActiveProcess();
}

void UnHook_DbgUiDebugActiveProcess()
{
    (void)TryUnHook_DbgUiDebugActiveProcess();
}

void Hook_WaitForDebugEvent()
{
    (void)TryHook_WaitForDebugEvent();
}

void UnHook_WaitForDebugEvent()
{
    (void)TryUnHook_WaitForDebugEvent();
}

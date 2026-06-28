#include "DebugBreakInternal.h"

BOOL
WINAPI
NewSetThreadContext(
    _In_ HANDLE hThread,
    _In_ CONTEXT* lpContext
)
{
    if (!lpContext)
    {
        return FALSE;
    }

    if (DebugBreakInternal::ShouldBypassThreadContextHooks())
    {
        return Sys_SetThreadContext(hThread, lpContext);
    }

    bool hardwareBreakpointsSynced = true;
    if (DebugBreakInternal::HasContextDebugRegisterRequest(lpContext->ContextFlags))
    {
        if (!DebugBreakInternal::IsHardwareBreakpointBridgeEnabled())
        {
            if (DebugBreakInternal::HasEnabledDebugRegister(lpContext))
            {
                DebugBreakInternal::ClearLocalHardwareBreakpointRecords(true, "硬件断点请求被拒绝");
                logger.Log("已拒绝硬件断点请求: tid=%u dr7=0x%llX，请使用软件INT3断点",
                    GetThreadId(hThread),
                    lpContext->Dr7);
                SetLastError(ERROR_NOT_SUPPORTED);
                return FALSE;
            }

            DebugBreakInternal::ClearLocalHardwareBreakpointRecords(false, "硬件断点桥关闭");
        }
        // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
        else
        {
            if (!DebugBreakInternal::IsCurrentHardwareBreakpointState(lpContext))
            {
                hardwareBreakpointsSynced = DebugBreakInternal::SyncHardwareBreakpointsFromContext(lpContext);
            }
        }

        //可能是为了清理dr6
        if (lpContext->Dr6 == 0)
        {
            InterlockedExchange(&g_debug_condition_detected, 0);
            InterlockedExchange(&g_debug_register_status, 0);
            DebugBreakInternal::ClearDebugHitRecord(GetThreadId(hThread));
        }

        if (!hardwareBreakpointsSynced)
        {
            SetLastError(ERROR_GEN_FAILURE);
            return FALSE;
        }
    }

    const DWORD contextFlagsWithoutDebug =
        DebugBreakInternal::StripContextDebugRegisterRequest(lpContext->ContextFlags);
    if (!DebugBreakInternal::HasNonDebugContextRequest(contextFlagsWithoutDebug))
    {
        return TRUE;
    }

    CONTEXT contextToSet = *lpContext;
    contextToSet.ContextFlags = contextFlagsWithoutDebug;
    return Sys_SetThreadContext(hThread, &contextToSet);
}

BOOL WINAPI NewGetThreadContext(
    _In_ HANDLE hThread,
    _Inout_ LPCONTEXT lpContext
)
{
    if (!lpContext)
    {
        return FALSE;
    }

    if (DebugBreakInternal::ShouldBypassThreadContextHooks())
    {
        return Sys_GetThreadContext(hThread, lpContext);
    }

    const DWORD requestedContextFlags = lpContext->ContextFlags;
    //调用原函数
    BOOL boSuccess = Sys_GetThreadContext(hThread, lpContext);

    if (boSuccess && DebugBreakInternal::HasContextDebugRegisterRequest(requestedContextFlags))
    {
        const bool hardwareBridgeEnabled = DebugBreakInternal::IsHardwareBreakpointBridgeEnabled();
        if (!hardwareBridgeEnabled)
        {
            DebugBreakInternal::ClearLocalHardwareBreakpointRecords(false, "硬件断点桥关闭");
        }

        lpContext->Dr0 = 0;
        lpContext->Dr1 = 0;
        lpContext->Dr2 = 0;
        lpContext->Dr3 = 0;
        lpContext->Dr6 = 0;
        lpContext->Dr7 = 0x400;

        const DWORD threadId = GetThreadId(hThread);
        DEBUG_HIT_RECORD hitRecord = { 0 };
        const bool hasThreadHitRecord = DebugBreakInternal::GetDebugHitRecord(threadId, &hitRecord);
        const bool useGlobalHitRecord = !hasThreadHitRecord && threadId == 0;
        const LONG debugCondition = hasThreadHitRecord ?
            hitRecord.Condition :
            (useGlobalHitRecord ? InterlockedCompareExchange(&g_debug_condition_detected, 0, 0) : 0);
        const LONG rawDebugRegisterStatus = (hasThreadHitRecord ?
            hitRecord.RegisterStatus :
            (useGlobalHitRecord ? InterlockedCompareExchange(&g_debug_register_status, 0, 0) : 0)) & 0xF;
        const LONG debugRegisterStatus = hardwareBridgeEnabled ? rawDebugRegisterStatus : 0;
        const bool hasCapturedDr6Status = debugRegisterStatus != 0;
        ULONG64 detectedBreakpointAddress = hasThreadHitRecord ? hitRecord.DetectedAddress : 0;

        if (hardwareBridgeEnabled && debugCondition == 1 && !hasCapturedDr6Status && detectedBreakpointAddress == 0)
        {
            detectedBreakpointAddress = DebugBreakInternal::QueryDetectedBreakpointAddress(hThread);
            if (detectedBreakpointAddress != 0)
            {
                DebugBreakInternal::UpdateDebugHitDetectedAddress(
                    threadId,
                    debugCondition,
                    debugRegisterStatus,
                    detectedBreakpointAddress);
            }
        }

        bool reportedDetectedBreakpoint = false;
        int fallbackDebugRegister = -1;
        int validBreakpointCount = 0;

        BreakpointList.Lock();
        for (const auto& Breakpoint : BreakpointList) //即使如果列表为0，它也会自动跳过，不会进for循环里
        {
            if (Breakpoint.Address &&
                Breakpoint.debugRegister >= 0 &&
                Breakpoint.debugRegister < DebugBreakInternal::kDebugRegisterCount)
            {
                DebugBreakInternal::WriteDebugRegister(lpContext, Breakpoint.debugRegister, Breakpoint.Address);
                lpContext->Dr7 |= (1ull << (Breakpoint.debugRegister * 2));
                lpContext->Dr7 |= (static_cast<ULONG64>(Breakpoint.debugRw & 0x3) <<
                    (16 + Breakpoint.debugRegister * 4));
                lpContext->Dr7 |= (static_cast<ULONG64>(Breakpoint.debugLen & 0x3) <<
                    (18 + Breakpoint.debugRegister * 4));
                fallbackDebugRegister = Breakpoint.debugRegister;
                ++validBreakpointCount;

                if (debugCondition != 0 &&
                    !hasCapturedDr6Status &&
                    detectedBreakpointAddress != 0 &&
                    DebugBreakInternal::IsDetectedAddressInBreakpointRange(Breakpoint, detectedBreakpointAddress))
                {
                    lpContext->Dr6 |= (1ull << Breakpoint.debugRegister);
                    reportedDetectedBreakpoint = true;
                }
            }
        }
        BreakpointList.UnLock();

        if (hardwareBridgeEnabled && debugCondition == 1 && hasCapturedDr6Status)
        {
            lpContext->Dr6 |= debugRegisterStatus;
        }
        else if (hardwareBridgeEnabled &&
            debugCondition == 1 &&
            !hasCapturedDr6Status &&
            detectedBreakpointAddress == 0 &&
            validBreakpointCount == 1 &&
            fallbackDebugRegister >= 0)
        {
            lpContext->Dr6 |= (1ull << fallbackDebugRegister);
        }
        else if (debugCondition == 2 && !reportedDetectedBreakpoint)
        {
            lpContext->Dr6 = 1 << 14; //设置bs位
        }

        // 旧项目保留的禁用路径：GetThreadContext 时通过 vmcall2(VMCALL_GET_BREAKPOINT) 查询当前线程命中的地址，
        // 再和回填到 lpContext 的 DR0-DR3 做相等比较以设置 DR6。
        // 为什么注释：重构后命中信息优先由驱动侧 DEBUG_HIT_RECORD 记录，并支持寄存器状态位、地址范围和单步 BS 位；
        // 旧路径只比较起始地址，不能覆盖 LEN>1 的监视范围，也缺少线程命中缓存。
        // 放开的好处：能直接复现旧版 VT 查询行为，便于定位 vmcall2 返回值是否正确。
        // 放开的坏处：会覆盖当前 DR6 合成结果，遇到范围断点、寄存器状态已捕获或单步异常时可能给调试器错误状态。
        // 项目影响：调试器依赖 DR6 判断哪个硬件断点命中，放开会影响断点继续执行、异常分发和反调试兼容性。
        // 结论：继续注释；需要调试时可以短期开启日志或 QueryDetectedBreakpointAddress，不建议恢复旧分支。
        //if (BreakpointList.size() > 0)
        //{
        //    DWORD dwThreadId = GetThreadId(hThread);
        //    if (dwThreadId == 0)
        //    {
        //        ReportSeriousError("无法获取线程id！");
        //        return boSuccess;
        //    }
        //
        //    BREAKPOINT_DETECTED vmcallinfo = { 0 };
        //    vmcallinfo.command = VMCALL_GET_BREAKPOINT;
        //    vmcallinfo.Cid.UniqueThread = (HANDLE)dwThreadId;
        //
        //    if (vmcall2(&vmcallinfo))
        //    {
        //        logger.Log("breakpoint_detected: %p", vmcallinfo.breakpoint_detected);
        //        if (vmcallinfo.breakpoint_detected == lpContext->Dr0)
        //        {
        //            lpContext->Dr6 = 1 << 0;
        //        }
        //        else if (vmcallinfo.breakpoint_detected == lpContext->Dr1)
        //        {
        //            lpContext->Dr6 = 1 << 1;
        //        }
        //        else if (vmcallinfo.breakpoint_detected == lpContext->Dr2)
        //        {
        //            lpContext->Dr6 = 1 << 2;
        //        }
        //        else if (vmcallinfo.breakpoint_detected == lpContext->Dr3)
        //        {
        //            lpContext->Dr6 = 1 << 3;
        //        }
        //        logger.Log("lpContext->Dr6: %p", lpContext->Dr6);
        //    }
        //}
    }
    //    logger.Log("current_vmcall 失败!  errorCode:%d", vmcallinfo.errorCode);
    //SYSTEM_INFO SysInfo = { 0 };
    //GetSystemInfo(&SysInfo);
    //vmcallinfo.CPUCount = SysInfo.dwNumberOfProcessors;
    //boSuccess = current_vmcall(&vmcallinfo);
    //if (boSuccess)
    //{
    //    logger.Log("返回的监视id: %d", vmcallinfo.watchid);
    //    // Add the breakpoint
    //    BREAKPOINT_RECORD Breakpoint = { 0 };
    //    Breakpoint.Address = vmcallinfo.VirtualAddress;
    //    Breakpoint.length = vmcallinfo.Size;
    //    Breakpoint.cr3 = vmcallinfo.cr3;
    //    Breakpoint.watchid = vmcallinfo.watchid;
    //    BreakpointList.Lock();
    //    BreakpointList.push_back(Breakpoint);
    //    BreakpointList.UnLock();
    //}
    //else
    return boSuccess;
}

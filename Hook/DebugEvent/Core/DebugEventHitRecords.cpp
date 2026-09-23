#include "DebugEventInternal.h"

namespace DebugEventInternal
{
    void StoreDebugHitRecord(DWORD threadId, LONG condition, LONG registerStatus, ULONG64 detectedAddress)
    {
        if (threadId == 0)
        {
            return;
        }

        DEBUG_HIT_RECORD record = { 0 };
        record.ThreadId = threadId;
        record.Condition = condition;
        record.RegisterStatus = registerStatus & 0xF;
        record.DetectedAddress = detectedAddress;

        DebugHitList.Lock();
        for (auto& item : DebugHitList)
        {
            if (item.ThreadId == threadId)
            {
                item.Condition = record.Condition;
                item.RegisterStatus = record.RegisterStatus;
                item.DetectedAddress = record.DetectedAddress;
                DebugHitList.UnLock();
                return;
            }
        }
        DebugHitList.push_back(record);
        DebugHitList.UnLock();
    }

    bool QueryDetectedBreakpoint(DWORD threadId, BREAKPOINT_DETECTED* detectedBreakpoint)
    {
        if (threadId == 0 || detectedBreakpoint == nullptr)
        {
            return false;
        }

        BREAKPOINT_DETECTED vmcallinfo = { 0 };
        vmcallinfo.command = VMCALL_GET_BREAKPOINT;
        vmcallinfo.Cid.UniqueThread = static_cast<ULONG64>(threadId);

        if (!vmcall2(&vmcallinfo))
        {
            return false;
        }

        *detectedBreakpoint = vmcallinfo;
        return true;
    }

    void CaptureSingleStepDebugState(DWORD threadId)
    {
        if (threadId == 0)
        {
            return;
        }

        if (!IsHardwareBreakpointBridgeEnabled())
        {
            InterlockedExchange(&g_debug_condition_detected, 2);
            InterlockedExchange(&g_debug_register_status, 0);
            StoreDebugHitRecord(threadId, 2, 0);
            return;
        }

        HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, threadId);
        if (hThread == nullptr)
        {
            return;
        }

        CONTEXT Context = { 0 };
        Context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        const BOOL boSuccess = Sys_GetThreadContext(hThread, &Context);
        LONG registerStatus = boSuccess ? static_cast<LONG>(Context.Dr6 & 0xF) : 0;
        const bool singleStep = boSuccess && ((Context.Dr6 & (1ULL << 14)) != 0);

        // EPT watch 查询会消费驱动的一次性命中状态，必须在读取真实 DR6 之后执行。
        BREAKPOINT_DETECTED detectedBreakpoint = { 0 };
        ULONG64 detectedAddress = 0;
        if (QueryDetectedBreakpoint(threadId, &detectedBreakpoint))
        {
            detectedAddress = detectedBreakpoint.breakpoint_detected;
            if (detectedAddress != 0 &&
                detectedBreakpoint.debugRegister >= 0 &&
                detectedBreakpoint.debugRegister < 4)
            {
                registerStatus = 1L << detectedBreakpoint.debugRegister;
            }
        }

        if (registerStatus != 0)
        {
            InterlockedExchange(&g_debug_condition_detected, 1);
            InterlockedExchange(&g_debug_register_status, registerStatus);
            StoreDebugHitRecord(threadId, 1, registerStatus, detectedAddress);
        }
        else if (detectedAddress != 0)
        {
            InterlockedExchange(&g_debug_condition_detected, 1);
            InterlockedExchange(&g_debug_register_status, 0);
            StoreDebugHitRecord(threadId, 1, 0, detectedAddress);
        }
        // Intel DR6.BS 只表示 TF 单步；B0-B3 能共存时必须优先按硬件断点处理。
        else if (singleStep)
        {
            InterlockedExchange(&g_debug_condition_detected, 2);
            InterlockedExchange(&g_debug_register_status, 0);
            StoreDebugHitRecord(threadId, 2, 0);
        }
        else
        {
            InterlockedExchange(&g_debug_condition_detected, 2);
            InterlockedExchange(&g_debug_register_status, 0);
            StoreDebugHitRecord(threadId, 2, 0);
            if (!boSuccess)
            {
                logger.Log("获取调试寄存器Context失败! (error: %d)", GetLastError());
            }
        }
        CloseHandle(hThread);
    }

    void ClearDebugHitRecord(DWORD threadId)
    {
        if (threadId == 0)
        {
            return;
        }

        InterlockedExchange(&g_debug_condition_detected, 0);
        InterlockedExchange(&g_debug_register_status, 0);

        DebugHitList.Lock();
        for (auto it = DebugHitList.begin(); it != DebugHitList.end();)
        {
            if (it->ThreadId == threadId)
            {
                it = DebugHitList.erase(it);
                continue;
            }
            ++it;
        }
        DebugHitList.UnLock();
    }
}

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

    ULONG64 QueryDetectedBreakpointAddress(DWORD threadId)
    {
        if (threadId == 0)
        {
            return 0;
        }

        BREAKPOINT_DETECTED vmcallinfo = { 0 };
        vmcallinfo.command = VMCALL_GET_BREAKPOINT;
        vmcallinfo.Cid.UniqueThread = static_cast<ULONG64>(threadId);

        if (!vmcall2(&vmcallinfo))
        {
            return 0;
        }

        return vmcallinfo.breakpoint_detected;
    }

    void CaptureSingleStepDebugState(DWORD threadId)
    {
        if (threadId == 0)
        {
            return;
        }

        if (!HasActiveHardwareBreakpoints())
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
        if (boSuccess)
        {
            Dr6 dr6;
            dr6.flags = Context.Dr6;
            if (dr6.BS)
            {
                InterlockedExchange(&g_debug_condition_detected, 2);
                InterlockedExchange(&g_debug_register_status, 0);
                StoreDebugHitRecord(threadId, 2, 0);
            }
            else
            {
                const LONG registerStatus = static_cast<LONG>(Context.Dr6 & 0xF);
                if (registerStatus != 0)
                {
                    InterlockedExchange(&g_debug_condition_detected, 1);
                    InterlockedExchange(&g_debug_register_status, registerStatus);
                    StoreDebugHitRecord(threadId, 1, registerStatus);
                }
                else
                {
                    const ULONG64 detectedAddress = QueryDetectedBreakpointAddress(threadId);
                    if (detectedAddress != 0)
                    {
                        InterlockedExchange(&g_debug_condition_detected, 1);
                        InterlockedExchange(&g_debug_register_status, 0);
                        StoreDebugHitRecord(threadId, 1, 0, detectedAddress);
                    }
                    else
                    {
                        InterlockedExchange(&g_debug_condition_detected, 2);
                        InterlockedExchange(&g_debug_register_status, 0);
                        StoreDebugHitRecord(threadId, 2, 0);
                    }
                }
            }
        }
        else
        {
            logger.Log("获取调试寄存器Context失败! (error: %d)", GetLastError());
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

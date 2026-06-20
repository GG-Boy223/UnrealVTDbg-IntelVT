#include "DebugBreakInternal.h"

namespace DebugBreakInternal
{
    bool GetDebugHitRecord(DWORD threadId, DEBUG_HIT_RECORD* record)
    {
        if (threadId == 0 || record == nullptr)
        {
            return false;
        }

        bool found = false;
        DebugHitList.Lock();
        for (const auto& item : DebugHitList)
        {
            if (item.ThreadId == threadId)
            {
                *record = item;
                found = true;
                break;
            }
        }
        DebugHitList.UnLock();
        return found;
    }

    void UpdateDebugHitDetectedAddress(
        DWORD threadId,
        LONG condition,
        LONG registerStatus,
        ULONG64 detectedAddress)
    {
        if (threadId == 0 || detectedAddress == 0)
        {
            return;
        }

        DebugHitList.Lock();
        for (auto& item : DebugHitList)
        {
            if (item.ThreadId == threadId)
            {
                item.Condition = condition;
                item.RegisterStatus = registerStatus & 0xF;
                item.DetectedAddress = detectedAddress;
                DebugHitList.UnLock();
                return;
            }
        }

        DEBUG_HIT_RECORD record = { 0 };
        record.ThreadId = threadId;
        record.Condition = condition;
        record.RegisterStatus = registerStatus & 0xF;
        record.DetectedAddress = detectedAddress;
        DebugHitList.push_back(record);
        DebugHitList.UnLock();
    }

    void ClearDebugHitRecord(DWORD threadId)
    {
        if (threadId == 0)
        {
            return;
        }

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

    void ClearAllDebugHitRecords()
    {
        InterlockedExchange(&g_debug_condition_detected, 0);
        InterlockedExchange(&g_debug_register_status, 0);

        DebugHitList.Lock();
        DebugHitList.clear();
        DebugHitList.UnLock();
    }

    ULONG64 QueryDetectedBreakpointAddress(HANDLE hThread)
    {
        const DWORD threadId = GetThreadId(hThread);
        if (threadId == 0)
        {
            logger.Log("无法获取线程id!");
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
}

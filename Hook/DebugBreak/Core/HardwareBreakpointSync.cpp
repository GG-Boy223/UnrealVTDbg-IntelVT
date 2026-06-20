#include "DebugBreakInternal.h"

namespace DebugBreakInternal
{
    bool IsCurrentHardwareBreakpointState(const CONTEXT* context)
    {
        if (context == nullptr)
        {
            return false;
        }

        bool desired[kDebugRegisterCount] = {};
        ULONG64 addresses[kDebugRegisterCount] = {};
        unsigned __int64 commands[kDebugRegisterCount] = {};
        int lengths[kDebugRegisterCount] = {};
        int rwCodes[kDebugRegisterCount] = {};
        int lenCodes[kDebugRegisterCount] = {};

        const ULONG64 dr7 = context->Dr7;
        for (int i = 0; i < kDebugRegisterCount; ++i)
        {
            const ULONG64 address = ReadDebugRegister(context, i);
            if (!address || !IsDebugRegisterEnabled(dr7, i))
            {
                continue;
            }

            const int rwCode = GetDebugRegisterRw(dr7, i);
            unsigned __int64 command = 0;
            if (!DecodeDebugRegisterCommand(rwCode, &command))
            {
                return false;
            }

            const int lenCode = GetDebugRegisterLenCode(dr7, i);
            desired[i] = true;
            addresses[i] = address;
            commands[i] = command;
            lengths[i] = DecodeDebugRegisterLength(lenCode);
            rwCodes[i] = rwCode;
            lenCodes[i] = lenCode;
        }

        bool present[kDebugRegisterCount] = {};
        bool same = true;

        BreakpointList.Lock();
        for (const auto& breakpoint : BreakpointList)
        {
            const int debugRegister = breakpoint.debugRegister;
            if (debugRegister < 0 ||
                debugRegister >= kDebugRegisterCount ||
                present[debugRegister] ||
                !desired[debugRegister] ||
                !SameBreakpointRequest(breakpoint,
                    addresses[debugRegister],
                    commands[debugRegister],
                    lengths[debugRegister],
                    debugRegister,
                    rwCodes[debugRegister],
                    lenCodes[debugRegister]))
            {
                same = false;
                break;
            }

            present[debugRegister] = true;
        }

        if (same)
        {
            for (int i = 0; i < kDebugRegisterCount; ++i)
            {
                if (desired[i] != present[i])
                {
                    same = false;
                    break;
                }
            }
        }

        BreakpointList.UnLock();
        return same;
    }

    bool SyncHardwareBreakpointsFromContext(const CONTEXT* context)
    {
        bool desired[kDebugRegisterCount] = {};
        ULONG64 addresses[kDebugRegisterCount] = {};
        unsigned __int64 commands[kDebugRegisterCount] = {};
        int lengths[kDebugRegisterCount] = {};
        int rwCodes[kDebugRegisterCount] = {};
        int lenCodes[kDebugRegisterCount] = {};
        bool allSynced = true;

        const ULONG64 dr7 = context->Dr7;
        for (int i = 0; i < kDebugRegisterCount; ++i)
        {
            const ULONG64 address = ReadDebugRegister(context, i);
            if (!address || !IsDebugRegisterEnabled(dr7, i))
            {
                continue;
            }

            const int rwCode = GetDebugRegisterRw(dr7, i);
            unsigned __int64 command = 0;
            if (!DecodeDebugRegisterCommand(rwCode, &command))
            {
                if (rwCode != WATCH_EXECUTION_ONLY)
                {
                    logger.Log("不支持的硬件断点类型: dr%d rw=%d dr7=0x%llX", i, rwCode, dr7);
                    allSynced = false;
                }
                continue;
            }

            const int lenCode = GetDebugRegisterLenCode(dr7, i);
            desired[i] = true;
            addresses[i] = address;
            commands[i] = command;
            lengths[i] = DecodeDebugRegisterLength(lenCode);
            rwCodes[i] = rwCode;
            lenCodes[i] = lenCode;
        }

        BreakpointList.Lock();
        for (auto it = BreakpointList.begin(); it != BreakpointList.end();)
        {
            const int debugRegister = it->debugRegister;
            const bool invalidRegister = debugRegister < 0 || debugRegister >= kDebugRegisterCount;
            const bool stale = invalidRegister ||
                !desired[debugRegister] ||
                !SameBreakpointRequest(*it,
                    addresses[debugRegister],
                    commands[debugRegister],
                    lengths[debugRegister],
                    debugRegister,
                    rwCodes[debugRegister],
                    lenCodes[debugRegister]);

            if (!stale)
            {
                ++it;
                continue;
            }

            const BREAKPOINT_RECORD oldBreakpoint = *it;
            if (!DeleteBreakpointFromDriver(oldBreakpoint))
            {
                logger.Log("移除硬件断点驱动返回失败，已丢弃本地记录");
            }
            it = BreakpointList.erase(it);
        }
        UpdateActiveHardwareBreakpointCountLocked();
        BreakpointList.UnLock();

        for (int i = 0; i < kDebugRegisterCount; ++i)
        {
            if (!desired[i])
            {
                continue;
            }

            bool exists = false;
            BreakpointList.Lock();
            for (const auto& breakpoint : BreakpointList)
            {
                if (SameBreakpointRequest(breakpoint, addresses[i], commands[i], lengths[i], i, rwCodes[i], lenCodes[i]))
                {
                    exists = true;
                    break;
                }
            }
            BreakpointList.UnLock();

            if (!exists)
            {
                if (!AddBreakpoint(reinterpret_cast<PVOID>(addresses[i]), commands[i], lengths[i], i, rwCodes[i], lenCodes[i]))
                {
                    allSynced = false;
                }
            }
        }

        return allSynced;
    }
}

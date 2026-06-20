#include "DebugBreakInternal.h"

namespace DebugBreakInternal
{
    void UpdateActiveHardwareBreakpointCountLocked()
    {
        InterlockedExchange(&g_active_hardware_breakpoint_count, static_cast<LONG>(BreakpointList.size()));
    }

    void ClearLocalHardwareBreakpointRecords(bool clearHitRecords, const char* reason)
    {
        bool hadRecords = false;

        BreakpointList.Lock();
        hadRecords = !BreakpointList.empty();
        if (hadRecords)
        {
            BreakpointList.clear();
        }
        UpdateActiveHardwareBreakpointCountLocked();
        BreakpointList.UnLock();

        if (clearHitRecords)
        {
            ClearAllDebugHitRecords();
        }

        if (hadRecords)
        {
            logger.Log("%s: 已清理本地硬件断点记录", reason ? reason : "硬件断点状态清理");
        }
    }

    bool SameBreakpointRequest(const BREAKPOINT_RECORD& breakpoint,
        ULONG64 address,
        unsigned __int64 command,
        int length,
        int debugRegister,
        int debugRw,
        int debugLen)
    {
        return breakpoint.Address == address &&
            breakpoint.command == command &&
            breakpoint.length == length &&
            breakpoint.debugRegister == debugRegister &&
            breakpoint.debugRw == debugRw &&
            breakpoint.debugLen == debugLen;
    }

    bool IsDetectedAddressInBreakpointRange(const BREAKPOINT_RECORD& breakpoint, ULONG64 detectedAddress)
    {
        if (breakpoint.Address == 0 || detectedAddress == 0)
        {
            return false;
        }

        const ULONG64 start = breakpoint.Address;
        const ULONG64 length = breakpoint.length > 0 ? static_cast<ULONG64>(breakpoint.length) : 1;
        return detectedAddress >= start && (detectedAddress - start) < length;
    }
}

bool RemoveBreakpoint()
{
    bool hadRecords = false;
    bool allDriverDeletesSucceeded = true;
    bool shouldClearAllVtState = false;

    BreakpointList.Lock();
    for (auto it = BreakpointList.begin(); it != BreakpointList.end();)
    {
        if (it->Address)
        {
            hadRecords = true;
            if (!DebugBreakInternal::DeleteBreakpointFromDriver(*it))
            {
                allDriverDeletesSucceeded = false;
                logger.Log("移除硬件断点驱动返回失败，已丢弃本地记录");
            }
        }
        it = BreakpointList.erase(it);
    }
    shouldClearAllVtState = hadRecords && BreakpointList.empty();
    DebugBreakInternal::UpdateActiveHardwareBreakpointCountLocked();
    BreakpointList.UnLock();

    if (shouldClearAllVtState)
    {
        clear_vt_breakpoint_state();
    }

    return !hadRecords || allDriverDeletesSucceeded;
}

bool AddBreakpoint(PVOID setAddress, unsigned __int64 command, int length, int debugRegister, int debugRw, int debugLen)
{
    bool boSuccess = false;

    // 旧项目保留的禁用路径：从整份 CONTEXT 中扫描 DR0-DR3，发现新地址后只按地址去重并添加第一个断点。
    // 为什么注释：重构后 SetThreadContext 已经把 DRx、RW/LEN、DR7 解析成显式参数，继续使用旧路径会丢失
    // debugRegister/debugRw/debugLen 等信息，多个寄存器指向同一地址时也无法表达真实状态。
    // 放开的好处：能恢复旧版“直接拿 CONTEXT 扫一遍”的宽松兼容行为，便于对照旧流程排查。
    // 放开的坏处：会绕过当前按寄存器编号去重和长度/访问类型同步的逻辑，可能导致 VT 断点与调试器看到的 DR7 不一致。
    // 项目影响：硬件断点命中回填、删除 watchid、DR6 判断都依赖当前 BREAKPOINT_RECORD 字段完整性。
    // 结论：继续注释，只作为旧实现对照；需要兼容旧输入时应扩展 SyncHardwareBreakpointsFromContext，而不是放开本块。
    //bool AddBreakpoint(_In_ CONST CONTEXT* lpContext, unsigned __int64 command)
    //{
    //    bool boSuccess = false;
    //
    //    BreakpointList.Lock();
    //    for (int i = 0; i < 4; ++i)
    //    {
    //        PVOID setAddress = nullptr;
    //
    //        switch (i)
    //        {
    //        case 0:
    //            setAddress = (PVOID)lpContext->Dr0;
    //            break;
    //        case 1:
    //            setAddress = (PVOID)lpContext->Dr1;
    //            break;
    //        case 2:
    //            setAddress = (PVOID)lpContext->Dr2;
    //            break;
    //        case 3:
    //            setAddress = (PVOID)lpContext->Dr3;
    //            break;
    //        default:
    //            break;
    //        }
    //
    //        if (setAddress && std::find_if(BreakpointList.begin(), BreakpointList.end(), [setAddress](auto& bp) {
    //            return bp.Address == (ULONG64)setAddress;
    //        }) == BreakpointList.end())
    //        {
    //            logger.Log("添加断点: %p", setAddress);
    //            if (BreakpointList.size() < 4)
    //            {
    //                BREAKPOINT_RECORD Breakpoint = { 0 };
    //                Breakpoint.Address = (ULONG64)setAddress;
    //                BreakpointList.push_back(Breakpoint);
    //
    //                boSuccess = SetBreakpoint(setAddress, command);
    //                if (!boSuccess)
    //                {
    //                    ReportSeriousError("设置断点失败");
    //                }
    //            }
    //            break;
    //        }
    //    }
    //    BreakpointList.UnLock();
    //
    //    return boSuccess;
    //}

    if (!setAddress ||
        debugRegister < 0 ||
        debugRegister >= DebugBreakInternal::kDebugRegisterCount)
    {
        return false;
    }

    bool exists = false;
    size_t breakpointCount = 0;
    BreakpointList.Lock();
    breakpointCount = BreakpointList.size();
    exists = std::find_if(BreakpointList.begin(), BreakpointList.end(), [setAddress, debugRegister](auto& bp)
    {
        return bp.Address == (ULONG64)setAddress && bp.debugRegister == debugRegister;
    }) != BreakpointList.end();
    BreakpointList.UnLock();

    if (exists)
    {
        return true;
    }

    if (breakpointCount < BREAKPOINT_COUNT)
    {
        boSuccess = SetBreakpoint(setAddress, command, length, debugRegister, debugRw, debugLen);
        if (!boSuccess)
        {
            ReportSeriousError("设置硬件断点失败");
        }
    }
    else
    {
        ReportSeriousError("设置断点过多，目前仅支持4个debugreg断点");
    }

    return boSuccess;
}

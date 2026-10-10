#include "../HookInternal.h"

namespace HookInternal
{
    int FindInt3BreakpointIndexLocked(const void* address)
    {
        for (int i = static_cast<int>(INT3BreakpointList.size()) - 1; i >= 0; --i)
        {
            if (IsSameTargetInt3Breakpoint(INT3BreakpointList.at(i), address))
            {
                return i;
            }
        }

        return -1;
    }

    int FindInt3BreakpointIndexLocked(const VT_BREAK_POINT& breakpoint)
    {
        for (int i = static_cast<int>(INT3BreakpointList.size()) - 1; i >= 0; --i)
        {
            const VT_BREAK_POINT& item = INT3BreakpointList.at(i);
            if (item.VirtualAddress == breakpoint.VirtualAddress &&
                item.watchid == breakpoint.watchid &&
                item.OriginalBytes == breakpoint.OriginalBytes &&
                IsSameTargetProcess(item))
            {
                return i;
            }
        }

        return -1;
    }

    int FindInt3BreakpointMetaIndexLocked(const VT_BREAK_POINT& breakpoint)
    {
        for (int i = static_cast<int>(Int3BreakpointMetaList.size()) - 1; i >= 0; --i)
        {
            const INT3_BREAKPOINT_META& item = Int3BreakpointMetaList.at(i);
            if (item.VirtualAddress == breakpoint.VirtualAddress &&
                item.watchid == breakpoint.watchid &&
                (item.pid == 0 || breakpoint.pid == 0 || item.pid == breakpoint.pid))
            {
                return i;
            }
        }

        return -1;
    }

    void TrackInt3BreakpointLocked(const VT_BREAK_POINT& breakpoint)
    {
        const int index = FindInt3BreakpointMetaIndexLocked(breakpoint);
        if (index >= 0)
        {
            Int3BreakpointMetaList.at(index).CreatedTick = GetTickCount64();
            return;
        }

        INT3_BREAKPOINT_META meta = {};
        meta.VirtualAddress = breakpoint.VirtualAddress;
        meta.watchid = breakpoint.watchid;
        meta.pid = breakpoint.pid;
        meta.CreatedTick = GetTickCount64();
        Int3BreakpointMetaList.push_back(meta);
    }

    void RemoveInt3BreakpointMetaLocked(const VT_BREAK_POINT& breakpoint)
    {
        const int index = FindInt3BreakpointMetaIndexLocked(breakpoint);
        if (index >= 0)
        {
            Int3BreakpointMetaList.erase(Int3BreakpointMetaList.begin() + index);
        }
    }

    ULONGLONG GetInt3BreakpointAgeMsLocked(const VT_BREAK_POINT& breakpoint)
    {
        const int index = FindInt3BreakpointMetaIndexLocked(breakpoint);
        if (index < 0)
        {
            return kInt3DuplicateRefreshDelayMs;
        }

        const ULONGLONG now = GetTickCount64();
        const ULONGLONG created = Int3BreakpointMetaList.at(index).CreatedTick;
        return now >= created ? now - created : 0;
    }

    bool RemoveInt3BreakpointRecord(const VT_BREAK_POINT& breakpoint)
    {
        bool removed = false;

        INT3BreakpointList.Lock();
        const int index = FindInt3BreakpointIndexLocked(breakpoint);
        if (index >= 0)
        {
            INT3BreakpointList.erase(INT3BreakpointList.begin() + index);
            RemoveInt3BreakpointMetaLocked(breakpoint);
            DecrementActiveHiddenInt3Count();
            removed = true;
        }
        INT3BreakpointList.UnLock();

        return removed;
    }

    bool DeleteInt3BreakpointFromDriver(const VT_BREAK_POINT& breakpoint)
    {
        const ULONG64 dbgUserBreakPoint = ResolveDbgUserBreakPointAddress();
        if (dbgUserBreakPoint == 0)
        {
            ReportSeriousError("DbgUserBreakPoint空指针");
            return false;
        }

        VT_BREAK_POINT vmcallinfo = { 0 };
        vmcallinfo.cr3 = breakpoint.cr3;
        vmcallinfo.VirtualAddress = breakpoint.VirtualAddress;
        vmcallinfo.PhysicalAddress = breakpoint.PhysicalAddress;
        vmcallinfo.Size = breakpoint.Size;
        vmcallinfo.command = VMCALL_WATCH_DELETE;
        vmcallinfo.LoopUserMode = dbgUserBreakPoint;
        vmcallinfo.OriginalBytes = breakpoint.OriginalBytes;
        vmcallinfo.watchid = breakpoint.watchid;
        vmcallinfo.CPUCount = g_dwNumberOfProcessors;
        vmcallinfo.pid = g_target_pid;

        DWORD bytesReturned = 0;
        HOOK_DEBUG_RESULT output = { 520, 0 };
        if (!SendUserDataToDriver(IOCTL_DEL_SOFTWARE_BREAKPOINT,
            &vmcallinfo,
            sizeof(VT_BREAK_POINT),
            &output,
            sizeof(HOOK_DEBUG_RESULT),
            &bytesReturned))
        {
            logger.Log("删除隐藏 INT3 断点失败: ioctl=0x%08lX, win32Error=%lu, watchid=%d, addr=0x%llX, cr3=0x%llX, pid=%u",
                IOCTL_DEL_SOFTWARE_BREAKPOINT,
                GetLastError(),
                breakpoint.watchid,
                breakpoint.VirtualAddress,
                breakpoint.cr3,
                breakpoint.pid);
            ReportSeriousError("IOCTL_DEL_SOFTWARE_BREAKPOINT 请求失败!");
            return false;
        }

        const bool success = output.status == 1998;
        if (success)
        {
            logger.Log("删除隐藏 INT3 断点成功: status=%lu, errorCode=%lu, watchid=%d, addr=0x%llX, cr3=0x%llX, pid=%u",
                output.status,
                output.errorCode,
                breakpoint.watchid,
                breakpoint.VirtualAddress,
                breakpoint.cr3,
                breakpoint.pid);
            clear_vt_breakpoint_state(0, breakpoint.VirtualAddress, breakpoint.cr3, breakpoint.watchid);
        }
        else
        {
            const char* errorText = "未知错误";
            switch (output.errorCode)
            {
            case 90: errorText = "CR3 解析/捕获失败"; break;
            case 91: errorText = "VMCALL 失败且无具体码"; break;
            case 99: errorText = "删除请求参数非法/watchid 无效"; break;
            case 520: errorText = "IOCTL/解密失败"; break;
            case 600: errorText = "watch ID 非法"; break;
            case 602: errorText = "vCPU/EPT/页记录缺失"; break;
            case 603: errorText = "CPUCount 非法"; break;
            case 604: errorText = "watch group 状态异常"; break;
            default: break;
            }

            logger.Log("删除隐藏 INT3 断点失败: status=%lu, errorCode=%lu(%s), watchid=%d, addr=0x%llX, cr3=0x%llX, pid=%u, bytes=%lu",
                output.status,
                output.errorCode,
                errorText,
                breakpoint.watchid,
                breakpoint.VirtualAddress,
                breakpoint.cr3,
                breakpoint.pid,
                bytesReturned);
        }
        return success;
    }
}

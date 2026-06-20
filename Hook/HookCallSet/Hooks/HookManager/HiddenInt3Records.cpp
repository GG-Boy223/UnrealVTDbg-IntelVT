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
        DWORD output = 520;
        if (!SendUserDataToDriver(IOCTL_DEL_SOFTWARE_BREAKPOINT,
            &vmcallinfo,
            sizeof(VT_BREAK_POINT),
            &output,
            sizeof(DWORD),
            &bytesReturned))
        {
            ReportSeriousError("IOCTL_DEL_SOFTWARE_BREAKPOINT 请求失败!");
            clear_vt_breakpoint_state(0, breakpoint.VirtualAddress, breakpoint.cr3, breakpoint.watchid);
            return false;
        }

        const bool success = output == 1998;
        clear_vt_breakpoint_state(0, breakpoint.VirtualAddress, breakpoint.cr3, breakpoint.watchid);
        return success;
    }
}

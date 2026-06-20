#include "DebugBreakInternal.h"

namespace DebugBreakInternal
{
    bool DeleteBreakpointFromDriver(const BREAKPOINT_RECORD& breakpoint)
    {
        DWORD bytesReturned = 0;
        DWORD output = 520;
        BREAKPOINT_RECORD request = breakpoint;
        if (!SendUserDataToDriver(IOCTL_DEL_HARDWARE_BREAKPOINT,
            &request,
            sizeof(BREAKPOINT_RECORD),
            &output,
            sizeof(DWORD),
            &bytesReturned))
        {
            logger.Log("IOCTL失败!");
            clear_vt_breakpoint_state(0, breakpoint.Address, breakpoint.cr3, breakpoint.watchid);
            return false;
        }

        if (output != 1998)
        {
            logger.Log("移除硬件断点失败");
            clear_vt_breakpoint_state(0, breakpoint.Address, breakpoint.cr3, breakpoint.watchid);
            return false;
        }

        ClearAllDebugHitRecords();
        clear_vt_breakpoint_state(0, breakpoint.Address, breakpoint.cr3, breakpoint.watchid);
        return true;
    }
}

bool SetBreakpoint(PVOID lpBaseAddress, unsigned __int64 command, int length, int debugRegister, int debugRw, int debugLen)
{
    bool boSuccess = false;
    DWORD BytesReturned = 0;

    if (!g_target_cr3 || !g_target_pid)
    {
        logger.Log("[%s] cr3 或 pid为空", __func__);
        return false;
    }

    const ULONG64 dbgUserBreakPoint = ResolveDbgUserBreakPointAddress();
    if (dbgUserBreakPoint == 0)
    {
        logger.Log("[%s] DbgUserBreakPoint空指针", __func__);
        return false;
    }

    SYSTEM_INFO SysInfo = { 0 };
    GetSystemInfo(&SysInfo);
    BREAKPOINT_RECORD Breakpoint = { 0 };
    Breakpoint.Address = (unsigned __int64)lpBaseAddress;
    Breakpoint.length = length;
    Breakpoint.cr3 = g_target_cr3;
    Breakpoint.command = command;
    Breakpoint.CPUCount = SysInfo.dwNumberOfProcessors;
    Breakpoint.LoopUserMode = dbgUserBreakPoint;
    Breakpoint.pid = g_target_pid;
    Breakpoint.watchid = -1;
    Breakpoint.debugRegister = debugRegister;
    Breakpoint.debugRw = debugRw;
    Breakpoint.debugLen = debugLen;

    BREAKPOINT_RECORD output = { 0 };
    if (SendUserDataToDriver(IOCTL_SET_HARDWARE_BREAKPOINT,
        &Breakpoint,
        sizeof(BREAKPOINT_RECORD),
        &output,
        sizeof(BREAKPOINT_RECORD),
        &BytesReturned))
    {
        if (output.watchid != -1)
        {
            boSuccess = true;

            Breakpoint.watchid = output.watchid;
            BreakpointList.Lock();
            BreakpointList.push_back(Breakpoint);
            DebugBreakInternal::UpdateActiveHardwareBreakpointCountLocked();
            BreakpointList.UnLock();
        }
        else
        {
            logger.Log("设置断点 失败!");
        }
    }
    else
    {
        logger.Log("通信 请求失败!");
    }

    // 旧项目保留的禁用路径：用户态 Hook 直接构造 VT_BREAK_POINT，并通过 current_vmcall 进入 VT 层设置硬件断点。
    // 为什么注释：重构后断点设置统一走 IOCTL_SET_HARDWARE_BREAKPOINT，由驱动负责参数校验、pid/cr3/watchid
    // 生命周期和跨 CPU 状态同步；用户态直接 vmcall 会绕过这些边界检查。
    // 放开的好处：少一层 IOCTL 转发，能复现旧版用户态直连 VT 的调试链路。
    // 放开的坏处：缺少 g_target_pid、debugRegister、debugRw/debugLen 等新字段，且用户态直接 vmcall 的失败路径更难回收本地记录。
    // 项目影响：会让 Hook64/Hook 的断点状态与 Dbgk 驱动维护的记录脱节，删除断点和 DR6 回填可能出现残留。
    // 结论：继续注释；如果需要恢复直连 VT，只能作为单独兼容模式实现，并补齐 BREAKPOINT_RECORD 的完整状态。
    //if (!g_target_cr3)
    //{
    //    logger.Log("[%s] g_target_cr3 空指针", __func__);
    //    return false;
    //}
    //VT_BREAK_POINT vmcallinfo = { 0 };
    //vmcallinfo.cr3 = g_target_cr3;
    //vmcallinfo.VirtualAddress = (unsigned __int64)lpBaseAddress;
    //vmcallinfo.Size = length;
    //vmcallinfo.command = command;
    //vmcallinfo.LoopUserMode = dbgUserBreakPoint;
    //
    //SYSTEM_INFO SysInfo = { 0 };
    //GetSystemInfo(&SysInfo);
    //vmcallinfo.CPUCount = SysInfo.dwNumberOfProcessors;
    //boSuccess = current_vmcall(&vmcallinfo);
    //if (boSuccess)
    //{
    //    logger.Log("返回的监视id: %d", vmcallinfo.watchid);
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
    //{
    //    logger.Log("current_vmcall 失败!  errorCode:%d", vmcallinfo.errorCode);
    //}

    return boSuccess;
}

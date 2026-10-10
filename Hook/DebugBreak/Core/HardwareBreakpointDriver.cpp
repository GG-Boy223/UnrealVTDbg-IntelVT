#include "DebugBreakInternal.h"

namespace DebugBreakInternal
{
    bool DeleteBreakpointFromDriver(const BREAKPOINT_RECORD& breakpoint)
    {
        DWORD bytesReturned = 0;
        HOOK_DEBUG_RESULT output = { 520, 0 };
        BREAKPOINT_RECORD request = breakpoint;
        if (!SendUserDataToDriver(IOCTL_DEL_HARDWARE_BREAKPOINT,
            &request,
            sizeof(BREAKPOINT_RECORD),
            &output,
            sizeof(HOOK_DEBUG_RESULT),
            &bytesReturned))
        {
            logger.Log("删除硬件断点失败: ioctl=0x%08lX, win32Error=%lu, dr=%d, watchid=%d, addr=0x%llX, cr3=0x%llX, pid=%u",
                IOCTL_DEL_HARDWARE_BREAKPOINT,
                GetLastError(),
                breakpoint.debugRegister,
                breakpoint.watchid,
                breakpoint.Address,
                breakpoint.cr3,
                breakpoint.pid);
            return false;
        }

        if (output.status != 1998)
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

            logger.Log("移除硬件断点失败: status=%lu, errorCode=%lu(%s), dr=%d, watchid=%d, addr=0x%llX, cr3=0x%llX, pid=%u, bytes=%lu",
                output.status,
                output.errorCode,
                errorText,
                breakpoint.debugRegister,
                breakpoint.watchid,
                breakpoint.Address,
                breakpoint.cr3,
                breakpoint.pid,
                bytesReturned);
            return false;
        }

        logger.Log("删除硬件断点成功: watchid=%d, addr=0x%llX, cr3=0x%llX, pid=%u, dr=%d",
            breakpoint.watchid,
            breakpoint.Address,
            breakpoint.cr3,
            breakpoint.pid,
            breakpoint.debugRegister);

        // 删除断点后不要继续复用旧的 #DB/EPT 命中缓存；CE 多次步过再删除时，
        // 过期 DR6 或命中地址会让调试器误以为断点仍处于暂停状态。
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
        logger.Log("[%s] cr3 或 pid 为空: cr3=0x%llX, pid=%u", __func__, g_target_cr3, g_target_pid);
        return false;
    }

    const ULONG64 dbgUserBreakPoint = ResolveDbgUserBreakPointAddress();
    if (dbgUserBreakPoint == 0)
    {
        logger.Log("[%s] DbgUserBreakPoint 为空: addr=0x%llX, cr3=0x%llX, pid=%u",
            __func__,
            reinterpret_cast<unsigned __int64>(lpBaseAddress),
            g_target_cr3,
            g_target_pid);
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
        // 旧日志包含 watchid，避免在日志文件暴露内部断点编号。
        // logger.Log("返回的监视id: %d", output.watchid);
        if (output.watchid >= 0 &&
            output.Address == Breakpoint.Address &&
            output.cr3 == Breakpoint.cr3)
        {
            boSuccess = true;

            logger.Log("设置硬件断点成功: watchid=%d, addr=0x%llX, cr3=0x%llX, pid=%u, dr=%d, rw=%d, len=%d, size=%d, command=0x%llX",
                output.watchid,
                Breakpoint.Address,
                Breakpoint.cr3,
                Breakpoint.pid,
                Breakpoint.debugRegister,
                Breakpoint.debugRw,
                Breakpoint.debugLen,
                Breakpoint.length,
                Breakpoint.command);

            Breakpoint.watchid = output.watchid;
            BreakpointList.Lock();
            BreakpointList.push_back(Breakpoint);
            DebugBreakInternal::UpdateActiveHardwareBreakpointCountLocked();
            BreakpointList.UnLock();
        }
        // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
        else
        {
            const char* errorText = "未知错误";
            if (output.errorCode >= 121 && output.errorCode <= 129)
            {
                errorText = "EPT watch 细分失败";
            }
            else if (output.errorCode >= 141 && output.errorCode <= 144)
            {
                errorText = "CR3 tracker 细分失败";
            }
            else
            {
                switch (output.errorCode)
                {
                case 90: errorText = "CR3 解析/捕获失败"; break;
                case 91: errorText = "VMCALL 失败且无具体码"; break;
                case 99: errorText = "断点请求参数非法"; break;
                case 100: errorText = "虚拟地址无法转换为物理地址"; break;
                case 101: errorText = "EPT watch 槽位耗尽"; break;
                case 102: errorText = "EPT watch 激活/隐藏失败"; break;
                case 103: errorText = "watch 跨越条目过多"; break;
                case 104: errorText = "CR3 tracker 分配失败"; break;
                case 105: errorText = "回写断点结果失败"; break;
                case 520: errorText = "IOCTL/驱动通信失败"; break;
                default: break;
                }
            }

            logger.Log("设置硬件断点失败: errorCode=%d(%s), watchid=%d, addr=0x%llX, cr3=0x%llX, pid=%u, dr=%d, rw=%d, len=%d, size=%d, command=0x%llX, bytes=%lu",
                output.errorCode,
                errorText,
                output.watchid,
                Breakpoint.Address,
                Breakpoint.cr3,
                Breakpoint.pid,
                Breakpoint.debugRegister,
                Breakpoint.debugRw,
                Breakpoint.debugLen,
                Breakpoint.length,
                Breakpoint.command,
                BytesReturned);
        }
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("设置硬件断点通信失败: ioctl=0x%08lX, win32Error=%lu, addr=0x%llX, cr3=0x%llX, pid=%u, dr=%d, rw=%d, len=%d, size=%d, command=0x%llX",
            IOCTL_SET_HARDWARE_BREAKPOINT,
            GetLastError(),
            Breakpoint.Address,
            Breakpoint.cr3,
            Breakpoint.pid,
            Breakpoint.debugRegister,
            Breakpoint.debugRw,
            Breakpoint.debugLen,
            Breakpoint.length,
            Breakpoint.command);
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
    //    logger.Log("current_vmcall 失败!  errorCode:%d", vmcallinfo.errorCode);
    return boSuccess;
}

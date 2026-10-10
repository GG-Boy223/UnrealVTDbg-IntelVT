#include "../HookInternal.h"

namespace HookInternal
{
    bool ReadTargetMemoryByte(
        HANDLE process,
        PVOID address,
        BYTE* value,
        NTSTATUS* ntStatus,
        DWORD* win32Error)
    {
        if (ntStatus != nullptr)
        {
            *ntStatus = 0;
        }
        if (win32Error != nullptr)
        {
            *win32Error = 0;
        }

        if (value == nullptr)
        {
            // 处理打开进程失败的情况
            return false;
        }

        SIZE_T bytesRead = 0;
        if (Sys_NtReadVirtualMemory)
        {
            NTSTATUS status = Sys_NtReadVirtualMemory(process, address, value, 1, &bytesRead);
            if (NT_SUCCESS(status) && bytesRead == 1)
            {
                return true;
            }
            if (ntStatus != nullptr)
            {
                *ntStatus = NT_SUCCESS(status) ? STATUS_UNSUCCESSFUL : status;
            }
        }

        if (Sys_ReadProcessMemory)
        {
            SetLastError(ERROR_SUCCESS);
            if (Sys_ReadProcessMemory(process, address, value, 1, &bytesRead) && bytesRead == 1)
            {
                return true;
            }
            if (win32Error != nullptr)
            {
                *win32Error = GetLastError();
            }
        }

        return false;
    }

    bool WriteTargetMemoryByte(
        HANDLE process,
        PVOID address,
        BYTE value,
        NTSTATUS* ntStatus,
        DWORD* win32Error,
        DWORD* protectWin32Error)
    {
        if (ntStatus != nullptr)
        {
            *ntStatus = 0;
        }
        if (win32Error != nullptr)
        {
            *win32Error = 0;
        }
        if (protectWin32Error != nullptr)
        {
            *protectWin32Error = 0;
        }

        // 目标地址通常是代码页（PAGE_EXECUTE_READ/EXECUTE_READWRITE）。
        // 直接 NtWriteVirtualMemory/WriteProcessMemory 可能因写保护失败，
        // 必须先临时改成可写，写完/失败后恢复原保护。
        DWORD oldProtect = 0;
        bool protectionChanged = false;
        const PFN_VIRTUALPROTECTEX virtualProtectEx =
            Sys_VirtualProtectEx ? Sys_VirtualProtectEx : VirtualProtectEx;
        if (virtualProtectEx != nullptr &&
            virtualProtectEx(process, address, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            protectionChanged = true;
        }
        else if (protectWin32Error != nullptr)
        {
            *protectWin32Error = GetLastError();
        }

        SIZE_T bytesWritten = 0;
        bool success = false;
        if (Sys_NtWriteVirtualMemory)
        {
            NTSTATUS status = Sys_NtWriteVirtualMemory(process, address, &value, 1, &bytesWritten);
            if (NT_SUCCESS(status) && bytesWritten == 1)
            {
                success = true;
            }
            else if (ntStatus != nullptr)
            {
                *ntStatus = NT_SUCCESS(status) ? STATUS_UNSUCCESSFUL : status;
            }
        }

        if (!success && Sys_WriteProcessMemory)
        {
            SetLastError(ERROR_SUCCESS);
            if (Sys_WriteProcessMemory(process, address, &value, 1, &bytesWritten) && bytesWritten == 1)
            {
                success = true;
            }
            else if (win32Error != nullptr)
            {
                *win32Error = GetLastError();
            }
        }

        if (protectionChanged)
        {
            DWORD restoreOldProtect = 0;
            virtualProtectEx(process, address, 1, oldProtect, &restoreOldProtect);
        }

        return success;
    }

    bool IsSingleByteInt3Write(const BYTE* writeBytes, SIZE_T size)
    {
        BYTE writeByte = 0;
        return size == 1 &&
            TryReadWriteBufferByte(writeBytes, 0, &writeByte) &&
            writeByte == 0xCC;
    }

    HiddenInt3WriteResult HandleHiddenInt3Write(
        HANDLE process,
        PVOID baseAddress,
        LPCVOID buffer,
        SIZE_T size,
        PSIZE_T bytesWritten)
    {
        HiddenInt3WriteResult result = {};
        if (baseAddress == nullptr ||
            buffer == nullptr ||
            size == 0 ||
            !IsHiddenInt3MemoryIoTarget(process))
        {
            return result;
        }

        const BYTE* writeBytes = static_cast<const BYTE*>(buffer);
        const bool setHiddenInt3 = IsSingleByteInt3Write(writeBytes, size);
        const bool hasHiddenInt3 = HasActiveHiddenInt3Breakpoints();
        if (!hasHiddenInt3 && !setHiddenInt3)
        {
            return result;
        }

        if (hasHiddenInt3)
        {
            const Int3WriteRemovalResult removalResult =
                RemoveHiddenInt3BreakpointsForWrite(baseAddress, writeBytes, size);
            if (removalResult.failed)
            {
                ReportSeriousError("删除cc断点失败");
                result.handled = true;
                result.success = false;
                return result;
            }

            if (removalResult.removedCount > 0)
            {
                if (removalResult.exactRestoreOnly)
                {
                    if (bytesWritten)
                    {
                        *bytesWritten = size;
                    }
                    result.handled = true;
                    result.success = true;
                }
                return result;
            }
        }

        if (!setHiddenInt3)
        {
            return result;
        }

        INT3BreakpointList.Lock();
        const int existingIndex = FindInt3BreakpointIndexLocked(baseAddress);
        if (existingIndex >= 0)
        {
            const VT_BREAK_POINT existing = INT3BreakpointList.at(existingIndex);
            const ULONGLONG existingAgeMs = GetInt3BreakpointAgeMsLocked(existing);
            INT3BreakpointList.UnLock();

            if (existingAgeMs < kInt3DuplicateRefreshDelayMs)
            {
                if (bytesWritten)
                {
                    *bytesWritten = size;
                }
                result.handled = true;
                result.success = true;
                return result;
            }

            if (!DeleteInt3BreakpointFromDriver(existing))
            {
                ReportSeriousError("刷新旧int3断点失败");
                result.handled = true;
                result.success = false;
                return result;
            }
            RemoveInt3BreakpointRecord(existing);
        }
        // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
        else
        {
            INT3BreakpointList.UnLock();
        }

        const ULONG64 dbgUserBreakPoint = ResolveDbgUserBreakPointAddress();
        if (dbgUserBreakPoint == 0)
        {
            ReportSeriousError("DbgUserBreakPoint空指针");
            result.handled = true;
            result.success = false;
            return result;
        }

        BYTE originalByte = 0;
        NTSTATUS readNtStatus = 0;
        DWORD readWin32Error = 0;
        if (!ReadTargetMemoryByte(
                process,
                baseAddress,
                &originalByte,
                &readNtStatus,
                &readWin32Error))
        {
            logger.Log("读取int3原字节失败: errorCode=1011, ntStatus=0x%08X, win32Error=%lu, addr=0x%llX, cr3=0x%llX, pid=%u, size=%zu, command=0x%llX",
                static_cast<unsigned int>(readNtStatus),
                readWin32Error,
                reinterpret_cast<unsigned __int64>(baseAddress),
                g_target_cr3,
                g_target_pid,
                size,
                static_cast<unsigned long long>(VMCALL_HIDE_SOFTWARE_BREAKPOINT));
            ReportSeriousError("读取int3原字节失败");
            result.handled = true;
            result.success = false;
            return result;
        }

        if (originalByte == 0xCC)
        {
            logger.Log("目标原字节已经是0xCC，跳过隐藏int3断点 地址:%p", baseAddress);
            if (bytesWritten)
            {
                *bytesWritten = size;
            }
            result.handled = true;
            result.success = true;
            return result;
        }

        MEMORY_BASIC_INFORMATION preWriteInfo = {};
        const SIZE_T preWriteQuerySize = VirtualQueryEx(
            process,
            baseAddress,
            &preWriteInfo,
            sizeof(preWriteInfo));
        bool skipWriteBack = false;
        if (preWriteQuerySize == sizeof(preWriteInfo))
        {
            const DWORD writableProtections =
                PAGE_READWRITE |
                PAGE_WRITECOPY |
                PAGE_EXECUTE_READWRITE |
                PAGE_EXECUTE_WRITECOPY;
            if ((preWriteInfo.Protect & writableProtections) == 0)
            {
                skipWriteBack = true;
                logger.Log("跳过int3原字节写回: 只读页原字节无需写回, addr=0x%llX, protect=0x%08lX, state=0x%08lX, type=0x%08lX, cr3=0x%llX, pid=%u, originalByte=0x%02X, size=%zu, command=0x%llX",
                    reinterpret_cast<unsigned __int64>(baseAddress),
                    preWriteInfo.Protect,
                    preWriteInfo.State,
                    preWriteInfo.Type,
                    g_target_cr3,
                    g_target_pid,
                    static_cast<unsigned int>(originalByte),
                    size,
                    static_cast<unsigned long long>(VMCALL_HIDE_SOFTWARE_BREAKPOINT));
            }
        }

        if (!skipWriteBack)
        {
        NTSTATUS writeNtStatus = 0;
        DWORD writeWin32Error = 0;
        DWORD writeProtectError = 0;
        if (!WriteTargetMemoryByte(
                process,
                baseAddress,
                originalByte,
                &writeNtStatus,
                &writeWin32Error,
                &writeProtectError))
        {
            logger.Log("写回int3原字节失败: errorCode=1011, ntStatus=0x%08X, win32Error=%lu, protectError=%lu, addr=0x%llX, cr3=0x%llX, pid=%u, originalByte=0x%02X, size=%zu, command=0x%llX",
                static_cast<unsigned int>(writeNtStatus),
                writeWin32Error,
                writeProtectError,
                reinterpret_cast<unsigned __int64>(baseAddress),
                g_target_cr3,
                g_target_pid,
                static_cast<unsigned int>(originalByte),
                size,
                static_cast<unsigned long long>(VMCALL_HIDE_SOFTWARE_BREAKPOINT));

            MEMORY_BASIC_INFORMATION memoryInfo = {};
            const SIZE_T querySize = VirtualQueryEx(
                process,
                baseAddress,
                &memoryInfo,
                sizeof(memoryInfo));
            if (querySize == sizeof(memoryInfo))
            {
                logger.Log("写回int3原字节失败页信息: base=0x%llX, regionBase=0x%llX, regionSize=%zu, protect=0x%08lX, state=0x%08lX, type=0x%08lX",
                    reinterpret_cast<unsigned __int64>(baseAddress),
                    reinterpret_cast<unsigned __int64>(memoryInfo.BaseAddress),
                    memoryInfo.RegionSize,
                    memoryInfo.Protect,
                    memoryInfo.State,
                    memoryInfo.Type);
            }
            else
            {
                logger.Log("写回int3原字节失败页信息: VirtualQueryEx 失败 win32Error=%lu", GetLastError());
            }

            // 这里读到的 originalByte 已经来自真实页；写回只是防御性同步。
            // 若目标进程仍是当前调试目标，允许继续安装隐藏 INT3，由后续
            // VMCALL 返回真正的 EPT/CR3 错误码；只有进程已退出/句柄失配才终止。
            const DWORD currentPid = GetProcessIdNoThrow(process);
            if (currentPid == 0 || currentPid != g_target_pid)
            {
                logger.Log("写回int3原字节失败且目标进程已失配: currentPid=%lu, targetPid=%u, addr=0x%llX",
                    currentPid,
                    g_target_pid,
                    reinterpret_cast<unsigned __int64>(baseAddress));
                ReportSeriousError("写回int3原字节失败");
                result.handled = true;
                result.success = false;
                return result;
            }

            logger.Log("写回int3原字节失败但原字节已在真实页中，继续安装隐藏 INT3: addr=0x%llX, cr3=0x%llX, pid=%u",
                reinterpret_cast<unsigned __int64>(baseAddress),
                g_target_cr3,
                g_target_pid);
        }
        }

        VT_BREAK_POINT vmcallinfo = { 0 };
        vmcallinfo.cr3 = g_target_cr3;
        vmcallinfo.VirtualAddress = reinterpret_cast<unsigned __int64>(baseAddress);
        vmcallinfo.Size = 1;
        vmcallinfo.command = VMCALL_HIDE_SOFTWARE_BREAKPOINT;
        vmcallinfo.LoopUserMode = dbgUserBreakPoint;
        vmcallinfo.OriginalBytes = originalByte;
        vmcallinfo.CPUCount = g_dwNumberOfProcessors;
        vmcallinfo.pid = g_target_pid;

        DWORD driverBytesReturned = 0;
        VT_BREAK_POINT output = { 0 };
        if (!SendUserDataToDriver(IOCTL_SET_SOFTWARE_BREAKPOINT,
            &vmcallinfo,
            sizeof(VT_BREAK_POINT),
            &output,
            sizeof(VT_BREAK_POINT),
            &driverBytesReturned))
        {
            logger.Log("设置隐藏 INT3 断点失败: ioctl=0x%08lX, win32Error=%lu, addr=0x%llX, cr3=0x%llX, pid=%u, size=%d, command=0x%llX",
                IOCTL_SET_SOFTWARE_BREAKPOINT,
                GetLastError(),
                vmcallinfo.VirtualAddress,
                vmcallinfo.cr3,
                vmcallinfo.pid,
                vmcallinfo.Size,
                vmcallinfo.command);
            ReportSeriousError("IOCTL_SET_SOFTWARE_BREAKPOINT 请求失败!");
            result.handled = true;
            result.success = false;
            return result;
        }

        if (!output.PhysicalAddress)
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

            logger.Log("设置隐藏 INT3 断点失败: errorCode=%d(%s), watchid=%d, addr=0x%llX, cr3=0x%llX, pid=%u, size=%d, command=0x%llX, originalByte=0x%02X",
                output.errorCode,
                errorText,
                output.watchid,
                vmcallinfo.VirtualAddress,
                vmcallinfo.cr3,
                vmcallinfo.pid,
                vmcallinfo.Size,
                vmcallinfo.command,
                static_cast<unsigned int>(originalByte));
            ReportSeriousError("设置int3断点失败");
            result.handled = true;
            result.success = false;
            return result;
        }

        logger.Log("设置隐藏 INT3 断点成功: watchid=%d, addr=0x%llX, phys=0x%llX, cr3=0x%llX, pid=%u, size=%d, command=0x%llX, originalByte=0x%02X",
            output.watchid,
            vmcallinfo.VirtualAddress,
            output.PhysicalAddress,
            vmcallinfo.cr3,
            vmcallinfo.pid,
            vmcallinfo.Size,
            vmcallinfo.command,
            static_cast<unsigned int>(originalByte));

        INT3BreakpointList.Lock();
        INT3BreakpointList.push_back(output);
        TrackInt3BreakpointLocked(output);
        IncrementActiveHiddenInt3Count();
        INT3BreakpointList.UnLock();

        if (bytesWritten)
        {
            *bytesWritten = size;
        }
        result.handled = true;
        result.success = true;
        return result;
    }
}

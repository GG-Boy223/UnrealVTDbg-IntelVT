#include "../HookInternal.h"

namespace HookInternal
{
    bool ReadTargetMemoryByte(HANDLE process, PVOID address, BYTE* value)
    {
        if (value == nullptr)
        {
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
        }

        return Sys_ReadProcessMemory &&
            Sys_ReadProcessMemory(process, address, value, 1, &bytesRead) &&
            bytesRead == 1;
    }

    bool WriteTargetMemoryByte(HANDLE process, PVOID address, BYTE value)
    {
        SIZE_T bytesWritten = 0;
        if (Sys_NtWriteVirtualMemory)
        {
            NTSTATUS status = Sys_NtWriteVirtualMemory(process, address, &value, 1, &bytesWritten);
            if (NT_SUCCESS(status) && bytesWritten == 1)
            {
                return true;
            }
        }

        return Sys_WriteProcessMemory &&
            Sys_WriteProcessMemory(process, address, &value, 1, &bytesWritten) &&
            bytesWritten == 1;
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
        if (!ReadTargetMemoryByte(process, baseAddress, &originalByte))
        {
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

        WriteTargetMemoryByte(process, baseAddress, originalByte);

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
            ReportSeriousError("IOCTL_SET_SOFTWARE_BREAKPOINT 请求失败!");
            result.handled = true;
            result.success = false;
            return result;
        }

        if (!output.PhysicalAddress)
        {
            ReportSeriousError("设置int3断点失败");
            result.handled = true;
            result.success = false;
            return result;
        }

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

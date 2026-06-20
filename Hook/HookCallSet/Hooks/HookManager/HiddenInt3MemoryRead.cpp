#include "../HookInternal.h"

namespace HookInternal
{
    bool HandleHiddenInt3Read(
        HANDLE process,
        LPCVOID baseAddress,
        LPVOID buffer,
        SIZE_T size,
        PSIZE_T bytesRead)
    {
        if (process == nullptr ||
            baseAddress == nullptr ||
            buffer == nullptr ||
            size == 0 ||
            !IsHiddenInt3MemoryIoTarget(process) ||
            !HasActiveHiddenInt3Breakpoints())
        {
            return false;
        }

        std::vector<VT_BREAK_POINT> overlappedBreakpoints;

        INT3BreakpointList.Lock();
        for (const auto& breakpoint : INT3BreakpointList)
        {
            if (!IsSameTargetProcess(breakpoint))
            {
                continue;
            }

            SIZE_T offset = 0;
            if (TryGetWriteOffsetForAddress(baseAddress, size, breakpoint.VirtualAddress, &offset))
            {
                if (overlappedBreakpoints.empty())
                {
                    overlappedBreakpoints.reserve(4);
                }
                overlappedBreakpoints.push_back(breakpoint);
            }
        }
        INT3BreakpointList.UnLock();

        if (overlappedBreakpoints.empty())
        {
            return false;
        }

        SIZE_T actualBytesRead = 0;
        bool readOk = false;
        if (Sys_NtReadVirtualMemory)
        {
            NTSTATUS status = Sys_NtReadVirtualMemory(
                process,
                const_cast<PVOID>(baseAddress),
                buffer,
                size,
                &actualBytesRead);
            readOk = NT_SUCCESS(status);
        }

        if (!readOk && Sys_ReadProcessMemory)
        {
            actualBytesRead = 0;
            readOk = Sys_ReadProcessMemory(
                process,
                baseAddress,
                buffer,
                size,
                &actualBytesRead) != FALSE;
        }

        if (!readOk)
        {
            return false;
        }

        BYTE* readBytes = static_cast<BYTE*>(buffer);
        for (const auto& breakpoint : overlappedBreakpoints)
        {
            SIZE_T offset = 0;
            if (!TryGetWriteOffsetForAddress(baseAddress, actualBytesRead, breakpoint.VirtualAddress, &offset))
            {
                continue;
            }

            readBytes[offset] = breakpoint.OriginalBytes;
        }

        if (bytesRead)
        {
            *bytesRead = actualBytesRead;
        }

        return true;
    }
}

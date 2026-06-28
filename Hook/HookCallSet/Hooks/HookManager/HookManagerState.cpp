#include "../HookInternal.h"

namespace HookInternal
{
    LONG DebugSessionCleanupInProgress = 0;
    LONG ActiveHiddenInt3Count = 0;
    std::vector<INT3_BREAKPOINT_META> Int3BreakpointMetaList;

    bool HasActiveHiddenInt3Breakpoints()
    {
        return InterlockedCompareExchange(&ActiveHiddenInt3Count, 0, 0) > 0;
    }

    void IncrementActiveHiddenInt3Count()
    {
        InterlockedIncrement(&ActiveHiddenInt3Count);
    }

    void DecrementActiveHiddenInt3Count()
    {
        LONG value = InterlockedDecrement(&ActiveHiddenInt3Count);
        if (value < 0)
        {
            InterlockedExchange(&ActiveHiddenInt3Count, 0);
        }
    }

    void ResetActiveHiddenInt3Count()
    {
        InterlockedExchange(&ActiveHiddenInt3Count, 0);
    }

    unsigned __int64 ToAddressValue(const void* address)
    {
        return static_cast<unsigned __int64>(reinterpret_cast<ULONG_PTR>(address));
    }

    bool IsSameTargetInt3Breakpoint(const VT_BREAK_POINT& breakpoint, const void* address)
    {
        if (breakpoint.VirtualAddress != ToAddressValue(address))
        {
            // 处理打开进程失败的情况
            return false;
        }

        return breakpoint.pid == 0 || g_target_pid == 0 || breakpoint.pid == g_target_pid;
    }

    bool IsSameTargetProcess(const VT_BREAK_POINT& breakpoint)
    {
        return breakpoint.pid == 0 || g_target_pid == 0 || breakpoint.pid == g_target_pid;
    }

    bool TryGetWriteOffsetForAddress(
        const void* baseAddress,
        SIZE_T size,
        unsigned __int64 targetAddress,
        SIZE_T* offset)
    {
        if (baseAddress == nullptr || size == 0)
        {
            // 处理打开进程失败的情况
            return false;
        }

        const unsigned __int64 base = ToAddressValue(baseAddress);
        if (targetAddress < base)
        {
            // 处理打开进程失败的情况
            return false;
        }

        const unsigned __int64 delta = targetAddress - base;
        if (delta >= size)
        {
            // 处理打开进程失败的情况
            return false;
        }

        if (offset)
        {
            *offset = static_cast<SIZE_T>(delta);
        }
        // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
        return true;
    }

    bool TryReadWriteBufferByte(const BYTE* writeBytes, SIZE_T offset, BYTE* value)
    {
        if (writeBytes == nullptr || value == nullptr)
        {
            // 处理打开进程失败的情况
            return false;
        }

        __try
        {
            *value = writeBytes[offset];
            // *value = writeBytes[offset];
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // 处理打开进程失败的情况
            return false;
        }
    }

    DWORD GetProcessIdNoThrow(HANDLE process)
    {
        if (process == nullptr || process == GetCurrentProcess())
        {
            return GetCurrentProcessId();
        }

        DWORD processId = 0;
        __try
        {
            processId = GetProcessId(process);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            processId = 0;
        }
        return processId;
    }

    bool IsTrackedDebugTargetProcessId(DWORD processId)
    {
        if (processId == 0)
        {
            // 处理打开进程失败的情况
            return false;
        }

        if (g_target_pid != 0 && processId == g_target_pid)
        {
            return true;
        }

        if (g_process_info.ProcessHandle != nullptr)
        {
            const DWORD trackedProcessId = GetProcessIdNoThrow(g_process_info.ProcessHandle);
            return trackedProcessId != 0 && trackedProcessId == processId;
        }

        return false;
    }

    bool IsTrackedDebugTargetProcessHandle(HANDLE process)
    {
        if (process != nullptr &&
            g_process_info.ProcessHandle != nullptr &&
            process == g_process_info.ProcessHandle)
        {
            return true;
        }

        return IsTrackedDebugTargetProcessId(GetProcessIdNoThrow(process));
    }

    bool IsHiddenInt3MemoryIoTarget(HANDLE process)
    {
        if (process == nullptr || g_target_pid == 0 || g_target_cr3 == 0)
        {
            // 处理打开进程失败的情况
            return false;
        }

        return IsTrackedDebugTargetProcessHandle(process);
    }
}

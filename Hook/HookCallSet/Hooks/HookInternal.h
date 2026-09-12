#pragma once

#include "../../dllmain.h"
#include "../../Globals.h"
#include "../../DebugEvent/DebugEvent.h"
#include "../../Inject/ShellCode/InjectCode.h"
#include "../../Inject/ApcInject/ApcInject.h"
#include "../../DebugBreak/DebugBreak.h"
#include "../../Channels/DispatchData.h"
#include "../../Log/Log.h"
#include "../../vmx/vmx.h"
#include "../../NativeApi/NtdllResolver.h"
#include <vector>

#define PAGE_ALIGN(Va) ((PVOID)((ULONG_PTR)(Va) & ~(PAGE_SIZE - 1)))

    void CleanupDebugSessionState(const char* reason);
bool RestoreBaseThreadInitThunkStartupBreakpoint();

namespace HookInternal
{
    constexpr ULONGLONG kInt3DuplicateRefreshDelayMs = 1500;
    extern LONG DebugSessionCleanupInProgress;
    extern LONG ActiveHiddenInt3Count;

    struct INT3_BREAKPOINT_META
    {
        unsigned __int64 VirtualAddress; //断点地址
        int watchid; //监视id
        unsigned long pid; //进程pid
        ULONGLONG CreatedTick;
    };

    extern std::vector<INT3_BREAKPOINT_META> Int3BreakpointMetaList;

    bool DeleteInt3BreakpointFromDriver(const VT_BREAK_POINT& breakpoint);
    bool HasActiveHiddenInt3Breakpoints();
    void IncrementActiveHiddenInt3Count();
    void DecrementActiveHiddenInt3Count();
    void ResetActiveHiddenInt3Count();
    unsigned __int64 ToAddressValue(const void* address);
    bool IsSameTargetInt3Breakpoint(const VT_BREAK_POINT& breakpoint, const void* address);
    bool IsSameTargetProcess(const VT_BREAK_POINT& breakpoint);
    bool TryGetWriteOffsetForAddress(const void* baseAddress, SIZE_T size, unsigned __int64 targetAddress, SIZE_T* offset);
    bool TryReadWriteBufferByte(const BYTE* writeBytes, SIZE_T offset, BYTE* value);
    int FindInt3BreakpointIndexLocked(const void* address);
    int FindInt3BreakpointIndexLocked(const VT_BREAK_POINT& breakpoint);
    int FindInt3BreakpointMetaIndexLocked(const VT_BREAK_POINT& breakpoint);
    void TrackInt3BreakpointLocked(const VT_BREAK_POINT& breakpoint);
    void RemoveInt3BreakpointMetaLocked(const VT_BREAK_POINT& breakpoint);
    ULONGLONG GetInt3BreakpointAgeMsLocked(const VT_BREAK_POINT& breakpoint);
    bool RemoveInt3BreakpointRecord(const VT_BREAK_POINT& breakpoint);
    bool ReadTargetMemoryByte(HANDLE process, PVOID address, BYTE* value);
    bool WriteTargetMemoryByte(HANDLE process, PVOID address, BYTE value);
    DWORD GetProcessIdNoThrow(HANDLE process);
    bool IsTrackedDebugTargetProcessId(DWORD processId);
    bool IsTrackedDebugTargetProcessHandle(HANDLE process);
    bool IsHiddenInt3MemoryIoTarget(HANDLE process);
    bool IsSingleByteInt3Write(const BYTE* writeBytes, SIZE_T size);

    struct Int3WriteRemovalResult
    {
        bool attempted = false;
        bool failed = false;
        bool exactRestoreOnly = false;
        int removedCount = 0;
    };

    Int3WriteRemovalResult RemoveHiddenInt3BreakpointsForWrite(const void* baseAddress, const BYTE* writeBytes, SIZE_T size);

    struct HiddenInt3WriteResult
    {
        bool handled = false;
        bool success = false;
    };

    HiddenInt3WriteResult HandleHiddenInt3Write(HANDLE process, PVOID baseAddress, LPCVOID buffer, SIZE_T size, PSIZE_T bytesWritten);
    bool HandleHiddenInt3Read(HANDLE process, LPCVOID baseAddress, LPVOID buffer, SIZE_T size, PSIZE_T bytesRead);
}

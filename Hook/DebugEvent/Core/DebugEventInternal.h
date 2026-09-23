#pragma once

#include "../../dllmain.h"
#include "../../HookCallSet/functionSet.h"
#include "../../Globals.h"
#include "../../Log/Log.h"
#include "../../vmx/vmx.h"
#include "../DebugEvent.h"

namespace DebugEventInternal
{
    bool IsEnvironmentFlagEnabled(const wchar_t* name);
    bool IsHardwareBreakpointBridgeEnabled();
    bool IsSingleStepExceptionCode(DWORD exceptionCode);
    void StoreDebugHitRecord(DWORD threadId, LONG condition, LONG registerStatus, ULONG64 detectedAddress = 0);
    bool QueryDetectedBreakpoint(DWORD threadId, BREAKPOINT_DETECTED* detectedBreakpoint);
    void CaptureSingleStepDebugState(DWORD threadId);
    void ClearDebugHitRecord(DWORD threadId);
}

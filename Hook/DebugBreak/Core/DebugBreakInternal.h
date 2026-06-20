#pragma once

#ifndef _DEBUG_BREAK_INTERNAL_H
#define _DEBUG_BREAK_INTERNAL_H

#include "../../dllmain.h"
#include "../../Globals.h"
#include "../../Channels/DispatchData.h"
#include "../../vmx/vmx.h"
#include "../../Log/Log.h"
#include "../../NativeApi/NtdllResolver.h"
#include "../DebugBreak.h"
#include <algorithm>

namespace DebugBreakInternal
{
    constexpr int kDebugRegisterCount = 4;
    constexpr DWORD kContextDebugRegistersLowBit = 0x00000010;
    constexpr DWORD kContextFeatureBitsMask = 0x0000FFFF;
    constexpr DWORD kContextNonDebugFeatureBitsMask =
        kContextFeatureBitsMask & ~kContextDebugRegistersLowBit;

    bool HasContextDebugRegisterRequest(DWORD contextFlags);
    DWORD StripContextDebugRegisterRequest(DWORD contextFlags);
    bool HasNonDebugContextRequest(DWORD contextFlags);
    bool IsEnvironmentFlagEnabled(const wchar_t* name);
    bool ShouldBypassThreadContextHooks();
    bool IsHardwareBreakpointBridgeEnabled();

    ULONG64 ReadDebugRegister(const CONTEXT* context, int debugRegister);
    void WriteDebugRegister(CONTEXT* context, int debugRegister, ULONG64 value);
    bool IsDebugRegisterEnabled(ULONG64 dr7, int debugRegister);
    int GetDebugRegisterRw(ULONG64 dr7, int debugRegister);
    int GetDebugRegisterLenCode(ULONG64 dr7, int debugRegister);
    bool HasEnabledDebugRegister(const CONTEXT* context);
    int DecodeDebugRegisterLength(int lenCode);
    bool DecodeDebugRegisterCommand(int rwCode, unsigned __int64* command);

    void UpdateActiveHardwareBreakpointCountLocked();
    void ClearLocalHardwareBreakpointRecords(bool clearHitRecords, const char* reason);
    bool DeleteBreakpointFromDriver(const BREAKPOINT_RECORD& breakpoint);
    bool SameBreakpointRequest(const BREAKPOINT_RECORD& breakpoint,
        ULONG64 address,
        unsigned __int64 command,
        int length,
        int debugRegister,
        int debugRw,
        int debugLen);
    bool IsDetectedAddressInBreakpointRange(const BREAKPOINT_RECORD& breakpoint, ULONG64 detectedAddress);
    bool IsCurrentHardwareBreakpointState(const CONTEXT* context);
    bool SyncHardwareBreakpointsFromContext(const CONTEXT* context);

    bool GetDebugHitRecord(DWORD threadId, DEBUG_HIT_RECORD* record);
    void UpdateDebugHitDetectedAddress(
        DWORD threadId,
        LONG condition,
        LONG registerStatus,
        ULONG64 detectedAddress);
    void ClearDebugHitRecord(DWORD threadId);
    void ClearAllDebugHitRecords();
    ULONG64 QueryDetectedBreakpointAddress(HANDLE hThread);
}

#endif

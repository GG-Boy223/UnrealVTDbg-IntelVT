#pragma once

#include "../../dllmain.h"
#include "../../HookCallSet/functionSet.h"
#include "../../Channels/DispatchData.h"
#include "../../Globals.h"
#include "../../Log/Log.h"
#include <new>
#include "../../../Common/VMProtect/vmp.h"

namespace HookDllMainInternal
{
    extern volatile LONG g_HookInstalled;
    extern volatile LONG g_HookRuntimeStarted;
    extern volatile LONG g_HookRuntimeStopping;
    extern volatile LONG g_HookRuntimeCompleted;
    extern HINSTANCE g_HookModule;
    extern HMODULE g_HookRuntimeModuleReference;
    extern HANDLE g_HookRuntimeThreadHandle;
    extern DWORD g_HookRuntimeThreadId;

    void EarlyHookLog(const wchar_t* message);
    void InitializeHookLogger(HINSTANCE moduleHandle);
    bool VerifyDriverHandshake();
    DWORD WINAPI HookRuntimeThread(LPVOID parameter);
    bool StartHookRuntime(HINSTANCE moduleHandle);
    void RequestHookRuntimeShutdown();
    void CloseHookRuntimeThreadHandle();
}

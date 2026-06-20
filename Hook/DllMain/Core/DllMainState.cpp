#include "DllMainInternal.h"

Logger logger;

namespace HookDllMainInternal
{
    volatile LONG g_HookInstalled = 0;
    volatile LONG g_HookRuntimeStarted = 0;
    volatile LONG g_HookRuntimeStopping = 0;
    volatile LONG g_HookRuntimeCompleted = 0;
    HINSTANCE g_HookModule = nullptr;
    HMODULE g_HookRuntimeModuleReference = nullptr;
    HANDLE g_HookRuntimeThreadHandle = nullptr;
    DWORD g_HookRuntimeThreadId = 0;
}

#include "DllMainInternal.h"

namespace HookDllMainInternal
{
    namespace
    {
        bool SignalHookReadyEvent()
        {
            wchar_t eventName[96] = {};
            if (_snwprintf_s(
                eventName,
                _countof(eventName),
                _TRUNCATE,
                L"%s%lu",
                HOOK_READY_EVENT_NAME_PREFIX,
                GetCurrentProcessId()) < 0)
            {
                return false;
            }

            HANDLE readyEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName);
            if (readyEvent == nullptr)
            {
                return false;
            }

            bool const signaled = SetEvent(readyEvent) != FALSE;
            CloseHandle(readyEvent);
            return signaled;
        }

        void CompleteRuntimeThread()
        {
            InterlockedExchange(&g_HookRuntimeCompleted, 1);
            CloseHookRuntimeThreadHandle();
        }

        void ReleaseRuntimeModuleReferenceAndExitThread(DWORD exitCode)
        {
            HMODULE moduleReference = g_HookRuntimeModuleReference;
            g_HookRuntimeModuleReference = nullptr;
            InterlockedExchange(&g_HookRuntimeStarted, 0);

            if (moduleReference != nullptr)
            {
                FreeLibraryAndExitThread(moduleReference, exitCode);
            }

            ExitThread(exitCode);
        }
    }

    DWORD WINAPI HookRuntimeThread(LPVOID parameter)
    {
        HINSTANCE moduleHandle = reinterpret_cast<HINSTANCE>(parameter);
        DWORD exitCode = 0;
        bool keepModuleReference = false;

        __try
        {
            EarlyHookLog(L"runtime begin");
            InitializeHookLogger(moduleHandle);
            logger.Log("Hook runtime begin");
            InitGlobalVariables();
            InitFunction();
            keepModuleReference = SetupHook();
            if (!keepModuleReference)
            {
                exitCode = 1;
            }
            else if (!VerifyDriverHandshake())
            {
                EarlyHookLog(L"runtime driver handshake failed");
                keepModuleReference = !UnHook();
                exitCode = 1;
            }
            else if (!SignalHookReadyEvent())
            {
                EarlyHookLog(L"runtime ready event failed");
                keepModuleReference = !UnHook();
                exitCode = 1;
            }
            logger.Log("Hook runtime end");
            EarlyHookLog(L"runtime end");
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            EarlyHookLog(L"runtime fault");
            keepModuleReference = !UnHook();
            exitCode = 1;
        }

        CompleteRuntimeThread();
        if (!keepModuleReference)
        {
            ReleaseRuntimeModuleReferenceAndExitThread(exitCode);
        }

        return exitCode;
    }

    bool StartHookRuntime(HINSTANCE moduleHandle)
    {
        if (InterlockedCompareExchange(&g_HookRuntimeStarted, 1, 0) != 0)
        {
            return true;
        }

        g_HookModule = moduleHandle;
        InterlockedExchange(&g_HookRuntimeStopping, 0);
        InterlockedExchange(&g_HookRuntimeCompleted, 0);

        HMODULE moduleReference = nullptr;
        if (!GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCTSTR>(&HookRuntimeThread),
            &moduleReference))
        {
            InterlockedExchange(&g_HookRuntimeStarted, 0);
            EarlyHookLog(L"runtime module reference failed");
            return false;
        }

        g_HookRuntimeModuleReference = moduleReference;

        DWORD threadId = 0;
        HANDLE threadHandle = CreateThread(nullptr, 0, HookRuntimeThread, moduleHandle, CREATE_SUSPENDED, &threadId);
        if (threadHandle == nullptr)
        {
            FreeLibrary(moduleReference);
            g_HookRuntimeModuleReference = nullptr;
            InterlockedExchange(&g_HookRuntimeStarted, 0);
            EarlyHookLog(L"runtime thread create failed");
            return false;
        }

        // Keep the module referenced for process lifetime while Detours targets point into it.
        g_HookRuntimeThreadHandle = threadHandle;
        g_HookRuntimeThreadId = threadId;

        if (ResumeThread(threadHandle) == static_cast<DWORD>(-1))
        {
            CloseHookRuntimeThreadHandle();
            FreeLibrary(moduleReference);
            g_HookRuntimeModuleReference = nullptr;
            InterlockedExchange(&g_HookRuntimeStarted, 0);
            EarlyHookLog(L"runtime thread resume failed");
            return false;
        }

        // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
        return true;
    }

    void RequestHookRuntimeShutdown()
    {
        InterlockedExchange(&g_HookRuntimeStopping, 1);
    }

    void CloseHookRuntimeThreadHandle()
    {
        HANDLE threadHandle = g_HookRuntimeThreadHandle;
        if (threadHandle == nullptr)
        {
            return;
        }

        CloseHandle(threadHandle);
        g_HookRuntimeThreadHandle = nullptr;
        g_HookRuntimeThreadId = 0;
    }
}

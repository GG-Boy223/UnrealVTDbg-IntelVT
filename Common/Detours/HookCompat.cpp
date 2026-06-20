#include <Windows.h>
#include "detours.h"
#include "HookCompat.h"

#ifdef _WIN64
#pragma comment(lib, "../Common/Detours/x64/detours.lib")
#else
#pragma comment(lib, "../Common/Detours/x86/detours.lib")
#endif

namespace
{
    void LogDetourFailure(const char* action, const char* stage, LONG status)
    {
        char message[160] = {};
        wsprintfA(message, "%s %s failed: %ld\n", action, stage, status);
        OutputDebugStringA(message);
    }

    bool BeginDetourTransaction(const char* action, HANDLE hThread)
    {
        LONG status = DetourTransactionBegin();
        if (status != NO_ERROR)
        {
            LogDetourFailure(action, "begin", status);
            return false;
        }

        status = DetourUpdateThread(hThread);
        if (status != NO_ERROR)
        {
            LogDetourFailure(action, "update-thread", status);
            DetourTransactionAbort();
            return false;
        }

        return true;
    }
}

bool HookOn(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread)
{
    if (pfun == nullptr || *pfun == nullptr || proxy_fun == nullptr || hThread == nullptr)
    {
        return false;
    }

    if (!BeginDetourTransaction("HookOn", hThread))
    {
        return false;
    }

    LONG status = DetourAttach(pfun, proxy_fun);
    if (status != NO_ERROR)
    {
        LogDetourFailure("HookOn", "attach", status);
        DetourTransactionAbort();
        return false;
    }

    status = DetourTransactionCommit();
    if (status != NO_ERROR)
    {
        LogDetourFailure("HookOn", "commit", status);
        return false;
    }

    return true;
}

bool HookOff(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread)
{
    if (pfun == nullptr || *pfun == nullptr || proxy_fun == nullptr || hThread == nullptr)
    {
        return false;
    }

    if (!BeginDetourTransaction("HookOff", hThread))
    {
        return false;
    }

    LONG status = DetourDetach(pfun, proxy_fun);
    if (status != NO_ERROR)
    {
        LogDetourFailure("HookOff", "detach", status);
        DetourTransactionAbort();
        return false;
    }

    status = DetourTransactionCommit();
    if (status != NO_ERROR)
    {
        LogDetourFailure("HookOff", "commit", status);
        return false;
    }

    return true;
}

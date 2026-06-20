#include "HookInternal.h"

NTSTATUS
NTAPI
NewNtDebugContinue(
    _In_ HANDLE DebugObjectHandle,
    _In_ PCLIENT_ID ClientId,
    _In_ NTSTATUS ContinueStatus
)
{
    return Sys_NtDebugContinue(DebugObjectHandle, ClientId, ContinueStatus);
}

bool TryHook_ContinueDebugEvent()
{
    Sys_ContinueDebugEvent = (PFN_CONTINUEDEBUGEVENT)ContinueDebugEvent;
    assert(Sys_ContinueDebugEvent);
    if (Sys_ContinueDebugEvent)
    {
        return HookOn((PVOID*)&Sys_ContinueDebugEvent, NewContinueDebugEvent, GetCurrentThread());
    }
    else
    {
        logger.Log("ContinueDebugEvent空指针");
        return false;
    }
}

bool TryUnHook_ContinueDebugEvent()
{
    if (Sys_ContinueDebugEvent)
    {
        return HookOff((PVOID*)&Sys_ContinueDebugEvent, NewContinueDebugEvent, GetCurrentThread());
    }

    return true;
}

bool TryHook_NtDebugContinue()
{
    Sys_NtDebugContinue = ResolveNtDebugContinue();
    assert(Sys_NtDebugContinue);
    if (Sys_NtDebugContinue)
    {
        return HookOn((PVOID*)&Sys_NtDebugContinue, NewNtDebugContinue, GetCurrentThread());
    }
    else
    {
        logger.Log("Sys_NtDebugContinue空指针");
        return false;
    }
}

bool TryUnHook_NtDebugContinue()
{
    if (Sys_NtDebugContinue)
    {
        return HookOff((PVOID*)&Sys_NtDebugContinue, NewNtDebugContinue, GetCurrentThread());
    }

    return true;
}

void Hook_ContinueDebugEvent()
{
    (void)TryHook_ContinueDebugEvent();
}

void UnHook_ContinueDebugEvent()
{
    (void)TryUnHook_ContinueDebugEvent();
}

void Hook_NtDebugContinue()
{
    (void)TryHook_NtDebugContinue();
}

void UnHook_NtDebugContinue()
{
    (void)TryUnHook_NtDebugContinue();
}

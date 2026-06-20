#include "HookInternal.h"

bool TryHook_SetThreadContext()
{
    Sys_SetThreadContext = (PFN_SETTHREADCONTEXT)SetThreadContext;
    assert(Sys_SetThreadContext);
    if (Sys_SetThreadContext)
    {
        return HookOn((PVOID*)&Sys_SetThreadContext, NewSetThreadContext, GetCurrentThread());
    }
    else
    {
        logger.Log("SetThreadContext空指针");
        return false;
    }
}

bool TryUnHook_SetThreadContext()
{
    if (Sys_SetThreadContext)
    {
        return HookOff((PVOID*)&Sys_SetThreadContext, NewSetThreadContext, GetCurrentThread());
    }

    return true;
}

bool TryHook_GetThreadContext()
{
    Sys_GetThreadContext = (PFN_GETTHREADCONTEXT)GetThreadContext;
    assert(Sys_GetThreadContext);
    if (Sys_GetThreadContext)
    {
        return HookOn((PVOID*)&Sys_GetThreadContext, NewGetThreadContext, GetCurrentThread());
    }
    else
    {
        logger.Log("Sys_GetThreadContext空指针");
        return false;
    }
}

bool TryUnHook_GetThreadContext()
{
    if (Sys_GetThreadContext)
    {
        return HookOff((PVOID*)&Sys_GetThreadContext, NewGetThreadContext, GetCurrentThread());
    }

    return true;
}

void Hook_SetThreadContext()
{
    (void)TryHook_SetThreadContext();
}

void UnHook_SetThreadContext()
{
    (void)TryUnHook_SetThreadContext();
}

void Hook_GetThreadContext()
{
    (void)TryHook_GetThreadContext();
}

void UnHook_GetThreadContext()
{
    (void)TryUnHook_GetThreadContext();
}

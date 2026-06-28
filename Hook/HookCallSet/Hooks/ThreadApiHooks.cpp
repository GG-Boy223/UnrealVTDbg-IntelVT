#include "HookInternal.h"

bool TryHook_SetThreadContext()
{
    Sys_SetThreadContext = (PFN_SETTHREADCONTEXT)SetThreadContext;
    assert(Sys_SetThreadContext);
    if (Sys_SetThreadContext)
    {
        return HookOn((PVOID*)&Sys_SetThreadContext, NewSetThreadContext, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
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

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
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
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
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

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
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

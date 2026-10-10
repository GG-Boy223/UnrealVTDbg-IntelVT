#include "EptHookCommon.h"

EXTERN_C
VOID Hook_NtUserFindWindowEx()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtUserFindWindowEx, NewNtUserFindWindowEx, (PVOID*)&Original_NtUserFindWindowEx, "NtUserFindWindowEx", "NtUserFindWindowEx");
    }
}

EXTERN_C
VOID Hook_ValidateHwnd()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_ValidateHwnd, NewValidateHwnd, (PVOID*)&Original_ValidateHwnd, "ValidateHwnd", "ValidateHwnd");
    }
}

EXTERN_C
VOID Hook_NtUserWindowFromPoint()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtUserWindowFromPoint, NewNtUserWindowFromPoint, (PVOID*)&Original_NtUserWindowFromPoint, "NtUserWindowFromPoint", "NtUserWindowFromPoint");
    }
}

EXTERN_C
VOID Hook_NtUserSetWindowsHookEx()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtUserSetWindowsHookEx, NewNtUserSetWindowsHookEx, (PVOID*)&Original_NtUserSetWindowsHookEx, "NtUserSetWindowsHookEx", "NtUserSetWindowsHookEx");
    }
}

EXTERN_C
VOID Hook_NtUserGetClassName()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtUserGetClassName, NewNtUserGetClassName, (PVOID*)&Original_NtUserGetClassName, "NtUserGetClassName", "NtUserGetClassName");
    }
}

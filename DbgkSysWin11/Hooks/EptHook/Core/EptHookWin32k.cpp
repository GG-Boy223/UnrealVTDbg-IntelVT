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

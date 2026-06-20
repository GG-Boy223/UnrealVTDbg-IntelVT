#include "DebugEventInternal.h"

namespace DebugEventInternal
{
    bool IsEnvironmentFlagEnabled(const wchar_t* name)
    {
        if (name == nullptr || name[0] == L'\0')
        {
            return false;
        }

        wchar_t value[16] = {};
        const DWORD length = GetEnvironmentVariableW(name, value, _countof(value));
        if (length == 0)
        {
            return false;
        }

        value[_countof(value) - 1] = L'\0';
        return lstrcmpiW(value, L"1") == 0 ||
            lstrcmpiW(value, L"true") == 0 ||
            lstrcmpiW(value, L"yes") == 0 ||
            lstrcmpiW(value, L"on") == 0;
    }

    bool IsHardwareBreakpointBridgeEnabled()
    {
        return !IsEnvironmentFlagEnabled(L"UNREALVTDBG_DISABLE_HARDWARE_BREAKPOINTS");
    }

    bool HasActiveHardwareBreakpoints()
    {
        if (!IsHardwareBreakpointBridgeEnabled())
        {
            return false;
        }

        return InterlockedCompareExchange(&g_active_hardware_breakpoint_count, 0, 0) > 0;
    }

    bool IsSingleStepExceptionCode(DWORD exceptionCode)
    {
        return exceptionCode == EXCEPTION_SINGLE_STEP;
    }
}

#include "DebugEventInternal.h"

namespace DebugEventInternal
{
    bool IsEnvironmentFlagEnabled(const wchar_t* name)
    {
        if (name == nullptr || name[0] == L'\0')
        {
            // 处理打开进程失败的情况
            return false;
        }

        wchar_t value[16] = {};
        const DWORD length = GetEnvironmentVariableW(name, value, _countof(value));
        if (length == 0)
        {
            // 处理打开进程失败的情况
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
        // 保持原版设计：CE/x64dbg/x32dbg 的 DRx 断点默认桥接为 VT watch。
        return !IsEnvironmentFlagEnabled(L"UNREALVTDBG_DISABLE_HARDWARE_BREAKPOINTS");
    }

    bool IsSingleStepExceptionCode(DWORD exceptionCode)
    {
        return exceptionCode == EXCEPTION_SINGLE_STEP;
    }
}

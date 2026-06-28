#include "HookInternal.h"

/// <summary>
/// 测试用的
/// </summary>
void NewLdrInitializeThunk(PCONTEXT ContextRecord, PVOID SystemArgument1)
{
    UNREFERENCED_PARAMETER(ContextRecord);
    UNREFERENCED_PARAMETER(SystemArgument1);

    // The original project kept this hook as a diagnostic stub and did not
    // forward to LdrInitializeThunk from the replacement routine.
}

void Hook_LdrInitializeThunk()
{
    LdrInitializeThunk = reinterpret_cast<PFN_LDRINITIALIZETHUNK>(
        GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "LdrInitializeThunk"));
    assert(LdrInitializeThunk);
    if (LdrInitializeThunk)
    {
        HookOn(reinterpret_cast<PVOID*>(&LdrInitializeThunk), NewLdrInitializeThunk, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("LdrInitializeThunk空指针");
    }
}

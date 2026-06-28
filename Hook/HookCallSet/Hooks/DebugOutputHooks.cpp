#include "HookInternal.h"

VOID
WINAPI
NewOutputDebugStringA(
    _In_opt_ LPCSTR lpOutputString
)
{
    // 旧项目保留代码：return Sys_OutputDebugStringA(lpOutputString);
    // 注释原因：当前 Hook 目标是吞掉 OutputDebugStringA/W，避免目标程序或反调试逻辑通过调试输出通道探测调试器。
    // 放开好处：恢复目标进程原始调试输出，便于排查目标自身日志。
    // 放开坏处：会重新暴露调试输出行为，可能让反调试样本利用 OutputDebugString 检测调试器状态。
    // 项目影响：继续注释会静默丢弃 ANSI 调试输出；放开会改变反调试对抗策略。
    // 当前建议：继续注释，排障时可临时放开。
    //return Sys_OutputDebugStringA(lpOutputString);
    return;
}

VOID
WINAPI
NewOutputDebugStringW(
    _In_opt_ LPCWSTR lpOutputString
)
{
    // 旧项目没有显式调用 Sys_OutputDebugStringW，但与 A 版本策略一致：默认吞掉宽字符调试输出。
    // 注释原因：保持 A/W 行为一致，避免目标程序通过 W 版本绕过输出抑制。
    // 放开好处：恢复 Unicode 调试日志，便于定位目标内部状态。
    // 放开坏处：增加被 OutputDebugStringW 反调试检测命中的概率。
    // 项目影响：继续注释时目标 W 日志不会外发；放开会恢复系统默认输出。
    // 当前建议：继续注释。
    //return Sys_OutputDebugStringW(lpOutputString);
    return;
}

bool TryHook_OutputDebugStringA()
{
    Sys_OutputDebugStringA = (PFN_OUTPUTDEBUGSTRINGA)OutputDebugStringA;
    assert(Sys_OutputDebugStringA);
    if (Sys_OutputDebugStringA)
    {
        return HookOn((PVOID*)&Sys_OutputDebugStringA, NewOutputDebugStringA, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("OutputDebugStringA空指针");
        return false;
    }
}

bool TryUnHook_OutputDebugStringA()
{
    if (Sys_OutputDebugStringA)
    {
        return HookOff((PVOID*)&Sys_OutputDebugStringA, NewOutputDebugStringA, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_OutputDebugStringW()
{
    Sys_OutputDebugStringW = (PFN_OUTPUTDEBUGSTRINGW)OutputDebugStringW;
    assert(Sys_OutputDebugStringW);
    if (Sys_OutputDebugStringW)
    {
        return HookOn((PVOID*)&Sys_OutputDebugStringW, NewOutputDebugStringW, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("OutputDebugStringW空指针");
        return false;
    }
}

bool TryUnHook_OutputDebugStringW()
{
    if (Sys_OutputDebugStringW)
    {
        return HookOff((PVOID*)&Sys_OutputDebugStringW, NewOutputDebugStringW, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

//Hook OutputDebugStringA/W处理敏感日志检测
void Hook_OutputDebugStringA()
{
    (void)TryHook_OutputDebugStringA();
}

void UnHook_OutputDebugStringA()
{
    (void)TryUnHook_OutputDebugStringA();
}

void Hook_OutputDebugStringW()
{
    (void)TryHook_OutputDebugStringW();
}

void UnHook_OutputDebugStringW()
{
    (void)TryUnHook_OutputDebugStringW();
}

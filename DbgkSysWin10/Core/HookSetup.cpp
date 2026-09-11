#include "../Driver.h"
#include "../ntos/inc/ntosdef.h"
#include "../ntos/inc/pstypes.h"
#include "../Globals.h"
#include "../DbgkApi/DbgkApi.h"
#include "../Hvm/hypervisor_gateway.h"
#include "../Asm/AsmVar.h"
#include "../Log/log.h"

extern "C"
{
    unsigned __int64 DbgkCreateThread_jcc_instruction = 0; // 保存 DbgkCreateThread 中被替换 cmp 后的下一条指令地址，供汇编 trampoline 返回。
    unsigned __int64 PspExitThread_jcc_instruction = 0; // 保存 PspExitThread 中被替换 cmp 后的下一条指令地址，供汇编 trampoline 返回。
}

EXTERN_C
PVOID DbgkCreateThread_CMP_Debugport_01() // EPT hook 代理：替换 DbgkCreateThread 读取 EPROCESS.DebugPort 的结果。
{
    PDEBUG_OBJECT Port = NULL; // 返回给原始比较逻辑的 DebugObject 指针。
    PDEBUG_PROCESS DebugProcess; // 接收自定义调试目标记录。
    _EPROCESS* Process = (_EPROCESS*)PsGetCurrentProcess(); // 当前线程所属进程即 DbgkCreateThread 正在处理的进程上下文。
    DbgkRuntimeCallGuard runtimeGuard; // 标记进入 Dbgk runtime，避免卸载流程与 hook 回调交叉。

    if (DbgkIsDriverRuntimeUnloading())
    {
        if (eprocess_offset::DebugPort == 0xFFFFFFFF)
        {
            ////判断当前调用者是否是调试器进程
            //if (!IsDebugger(PsGetCurrentProcess()))
            //{
            //    //根据窗口句柄获得窗口对象
            //    PWND pwnd = Sys_ValidateHwnd(hwnd);
            //    if (pwnd)
            //    {
            //        //根据线程对象获得进程对象
            //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
            //    }
            //}
            //NtUserQueryWindow(hwnd, WindowInfo);
            return NULL; // 卸载阶段偏移无效时保守返回空，避免解引用错误 EPROCESS 偏移。
        }

        return *(PDEBUG_OBJECT*)((size_t)Process + eprocess_offset::DebugPort); // 卸载期间不走自定义表，直接回退系统 DebugPort。
    }

    //DbgBreakPoint();
    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        Port = DebugProcess->DebugObject; // 目标进程命中自定义调试对象时返回受控 DebugObject。
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort; // 未命中自定义表时计算原始 EPROCESS.DebugPort 地址。
        Port = *(PDEBUG_OBJECT*)ptr_DebugPort; // 保持系统原始语义，降低 hook 对非目标进程的回归风险。
    }
    return Port; // 返回给汇编 hook，继续原始 cmp/test 路径。
}

//VOID SetupHook_DbgkCreateThread_CMP_Debugport()
//{
//    BYTE code[] = { 0x48, 0x83,'?','?','?','?' };  //cmp指令
//
//    BYTE* startaddr = (BYTE*)Sys_DbgkCreateThread;
//    for (;;)
//    {
//        BYTE* hookAddr = (BYTE*)SearchCode(code, sizeof(code), startaddr, 0x100);
//        //判断指令是否是cmp
//        if ((hookAddr[0] == 0x48) &&
//            (hookAddr[1] == 0x83) &&
//            (*(WORD*)&hookAddr[3] == eprocess_offset::DebugPort))
//        {
//            //得到cmp的下一条指令
//            DbgkCreateThread_jcc_instruction = (unsigned __int64)((unsigned __int64)hookAddr + (unsigned __int64)(LDE((unsigned __int8*)hookAddr, 64)));
//            if (hvgt::hook_function(hookAddr, Asm_DbgkCreateThread_CMP_Debugport_01, NULL))
//            {
//                outLog("hook DbgkCreateThread_CMP_Debugport_01成功.");
//            }
//            else
//                outLog("hook DbgkCreateThread_CMP_Debugport_01失败.");
//            break;
//        }
//        else
//            //outLog("没有匹配到 cmp qword ptr [rdi+578h]");
//            startaddr = (BYTE*)((unsigned __int64)hookAddr + (unsigned __int64)(LDE((unsigned __int8*)hookAddr, 64)));
//    }
//}
VOID SetupHook_DbgkCreateThread_CMP_Debugport() // 在 DbgkCreateThread 内查找 cmp [Process+DebugPort] 并安装 EPT hook。
{
    BYTE code[] = { 0x48, 0x83,'?','?','?','?' }; // Win10 目标指令模板，? 表示通配立即数/位移字节。

    BYTE* startaddr = (BYTE*)Sys_DbgkCreateThread; // 从符号解析到的 DbgkCreateThread 入口开始扫描。
    const ULONG_PTR searchLimit = 0x100; // 只扫描函数开头附近，避免跨函数误匹配。
    ULONG_PTR searched = 0; // 已扫描字节数，用于保证循环可前进。
    while (searched < searchLimit)
    {
        const ULONG_PTR remaining = searchLimit - searched; // 剩余扫描窗口，防止 SearchCode 越界。
        BYTE* hookAddr = (BYTE*)SearchCode(code, sizeof(code), startaddr, remaining); // 在剩余窗口内查找候选 cmp 指令。
        if (hookAddr == NULL)
        {
            outLog("hook DbgkCreateThread_CMP_Debugport_01 target cmp not found.");
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // 未找到目标指令，不安装 hook，避免错误 patch。
        }

        if (!MmIsAddressValid(hookAddr) || !MmIsAddressValid(hookAddr + sizeof(code) - 1))
        {
            outLog("hook DbgkCreateThread_CMP_Debugport_01 target cmp address invalid.");
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // 候选地址不可读时停止，避免内核页错误。
        }

        const ULONG_PTR instructionLength = LDE((unsigned __int8*)hookAddr, 64); // 解析候选指令长度，用于计算 trampoline 返回点。
        if (instructionLength == 0)
        {
            outLog("hook DbgkCreateThread_CMP_Debugport_01 scan stopped on invalid instruction length.");
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // LDE 失败说明无法安全推进扫描，必须停止防死循环。
        }

        //判断指令是否是cmp
        if ((hookAddr[0] == 0x48) &&
            (hookAddr[1] == 0x83) &&
            (*(WORD*)&hookAddr[3] == eprocess_offset::DebugPort))
        {
            //得到cmp的下一条指令
            DbgkCreateThread_jcc_instruction = (unsigned __int64)((unsigned __int64)hookAddr + (unsigned __int64)instructionLength); // 保存原 cmp 后继地址，汇编代理执行完后跳回这里。
            if (hvgt::hook_function(hookAddr, Asm_DbgkCreateThread_CMP_Debugport_01, NULL))
            {
                outLog("\x68\x6F\x6F\x6B\x20\x44\x62\x67\x6B\x43\x72\x65\x61\x74\x65\x54\x68\x72\x65\x61\x64\x5F\x43\x4D\x50\x5F\x44\x65\x62\x75\x67\x70\x6F\x72\x74\x5F\x30\x31\xB3\xC9\xB9\xA6\x2E");
            }
            else
            {
                outLog("\x68\x6F\x6F\x6B\x20\x44\x62\x67\x6B\x43\x72\x65\x61\x74\x65\x54\x68\x72\x65\x61\x64\x5F\x43\x4D\x50\x5F\x44\x65\x62\x75\x67\x70\x6F\x72\x74\x5F\x30\x31\xCA\xA7\xB0\xDC\x2E");
            }
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // 成功匹配目标 DebugPort cmp 后不再继续扫描。
        }

        BYTE* nextStart = hookAddr + instructionLength; // 未命中 DebugPort 偏移时跳到下一条指令继续扫描。
        if (nextStart <= startaddr)
        {
            outLog("hook DbgkCreateThread_CMP_Debugport_01 scan cannot advance.");
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // 防御异常 LDE 结果导致扫描指针不前进。
        }

        searched += static_cast<ULONG_PTR>(nextStart - startaddr); // 累加实际推进距离，保证不超过 searchLimit。
        startaddr = nextStart; // 更新扫描起点。
    }

    outLog("hook DbgkCreateThread_CMP_Debugport_01 target cmp not found.");
}

VOID SetupHook_PspExitThread_CMP_Debugport() // 在 PspExitThread 内查找 DebugPort 比较并安装 EPT hook。
{
    BYTE code[] = { 0x49, 0x39,'?','?','?','?' }; // Win10 PspExitThread 目标 cmp 指令模板。
    BYTE* hookAddr = (BYTE*)SearchCode(code, sizeof(code), (BYTE*)Sys_PspExitThread, 0x200); // 在函数前 0x200 字节内查找候选指令。
    if (hookAddr == NULL)
    {
        outLog("hook PspExitThread_CMP_Debugport_01 target cmp not found.");
        return;
    }

    if (!MmIsAddressValid(hookAddr) || !MmIsAddressValid(hookAddr + sizeof(code) - 1))
    {
        outLog("hook PspExitThread_CMP_Debugport_01 target cmp address invalid.");
        return;
    }

    if ((hookAddr[0] == 0x49) &&
        (hookAddr[1] == 0x39) &&
        (*(WORD*)&hookAddr[3] == eprocess_offset::DebugPort))
    {
        PspExitThread_jcc_instruction = (unsigned __int64)((unsigned __int64)hookAddr + (unsigned __int64)(LDE((unsigned __int8*)hookAddr, 64)));
        if (PspExitThread_jcc_instruction == 0)
        {
            outLog("hook PspExitThread_CMP_Debugport_01 jcc instruction is NULL after LDE.");
            return;
        }
        if (hvgt::hook_function(hookAddr, Asm_PspExitThread_CMP_Debugport_01, NULL))
        {
            outLog("\x68\x6F\x6F\x6B\x20\x50\x73\x70\x45\x78\x69\x74\x54\x68\x72\x65\x61\x64\x5F\x43\x4D\x50\x5F\x44\x65\x62\x75\x67\x70\x6F\x72\x74\x5F\x30\x31\xB3\xC9\xB9\xA6\x2E");
        }
        else
        {
            outLog("\x68\x6F\x6F\x6B\x20\x50\x73\x70\x45\x78\x69\x74\x54\x68\x72\x65\x61\x64\x5F\x43\x4D\x50\x5F\x44\x65\x62\x75\x67\x70\x6F\x72\x74\x5F\x30\x31\xCA\xA7\xB0\xDC\x2E");
        }
    }
}

EXTERN_C
ULONG_PTR SearchCode(unsigned char* code, ULONG_PTR codeLenth, unsigned char* startaddr, ULONG_PTR addrlenth) // 带通配符的内核指令扫描，遇到无效地址必须推进索引。
{
    ULONG_PTR Ret = 0; // 返回匹配地址，0 表示未找到。
    ULONG_PTR Index = 0; // 当前扫描偏移。
    unsigned char x = '?'; // 通配符字节。
    BOOLEAN Isfind = 0; // 当前候选位置是否匹配。
    if (!code || codeLenth == 0 || !startaddr || addrlenth == 0 || addrlenth < codeLenth)
    {
        //DbgBreakPoint();
        return 0; // 参数非法或扫描范围短于模板时直接失败，避免无符号下溢。
    }

    while (Index < (addrlenth - codeLenth + 1))
    {
        if (!MmIsAddressValid((PVOID)&startaddr[Index]))
        {
            //if (!MmIsAddressValid((PVOID)&startaddr[Index]))continue;
            Index++; // 无效地址也必须推进，否则会在同一地址死循环。
            // *ppThread = NULL;
            continue; // 当前字节不可读，跳过该候选位置。
        }

        if ((code[0] == x) || (code[0] == startaddr[Index]))
        {
            Isfind = 1; // 首字节匹配后进入完整模板比较。
            for (ULONG_PTR i = 0; i < codeLenth; i++)
            {
                if (!MmIsAddressValid((PVOID)&startaddr[Index + i]))
                {
                    Isfind = 0; // 模板范围内任意字节不可读，都不能视为匹配。
                    break; // 退出内层比较，外层继续推进。
                }

                if ((code[i] != x) && (code[i] != startaddr[Index + i]))
                {
                    Isfind = 0; // 非通配字节不一致，当前候选失败。
                    break; // 退出内层比较。
                }
            }
            if (Isfind)
            {
                Ret = (ULONG_PTR)&startaddr[Index]; // 保存匹配起始地址。
                break; // 找到第一处匹配即可返回。
            }
        }
        //if (!MmIsAddressValid((PVOID)&startaddr[Index]))continue;
        Index++; // 扫描下一个候选字节。
    }
    return Ret; // 返回匹配地址或 0。
}

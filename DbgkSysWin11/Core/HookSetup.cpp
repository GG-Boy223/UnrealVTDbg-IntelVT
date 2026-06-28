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
    unsigned __int64 DbgkCreateThread_jcc_instruction = 0; // 保存 DbgkCreateThread cmp 后继地址，供汇编 hook 返回原流程。
    unsigned __int64 PspExitThread_jcc_instruction = 0; // 保存 PspExitThread cmp 后继地址，供汇编 hook 返回原流程。
}

// /*找特征码  注意找到的特征码是给定的第0个特征码的实际地址
// 特征码形式: CHAR EtwHostStateShellcode[] = { 0xB8,0x08,0x00,0x00,0xC0,0xE9,'?','?','?','?',0x48,0x8B,0x15 };
// 参数:
// code: 表示特征码 如 EtwHostStateShellcode  UCHAR 数组
// codeLenth: 表示特征码长度
// startaddr: 找特征码的起始地址;
// addrlenth: 用来确定找特征码结束地址,即找起始地址偏移长度后结束
// 返回值: 如果返回0 表示未找到,其它值表示找到第一个特征码的实际地址 注意是实际地址 即startaddr 到(startaddr+addrlenth)这范围中间的某个地址,不是偏移值,是实际地址
// */
//EXTERN_C
//ULONG_PTR SearchCode(unsigned char* code, ULONG_PTR codeLenth, unsigned char* startaddr, ULONG_PTR addrlenth)
//{
//    ULONG_PTR Ret = 0;
//    ULONG_PTR Index = 0;
//    unsigned char x = '?';
//    BOOLEAN Isfind = 0;
//    if ((!code) | (!codeLenth) | (!startaddr) | (!addrlenth))return 0;
//    while (Index < (addrlenth - codeLenth + 1))
//    {
//        if (!MmIsAddressValid((PVOID)&startaddr[Index]))continue;
//
//        if (code[0] == startaddr[Index])
//        {
//            Isfind = 1;
//            for (ULONG_PTR i = 0; i < codeLenth; i++)
//            {
//                if (!MmIsAddressValid((PVOID)&startaddr[Index + i]))continue;
//                if ((code[i] != x) && (code[i] != startaddr[Index + i]))
//                {
//                    Isfind = 0;
//                    break;
//                }
//            }
//            if (Isfind)
//                Ret = (ULONG_PTR)&startaddr[Index];
//                break;
//        }
//        Index++;
//    }
//    return Ret;
//}
EXTERN_C
ULONG_PTR SearchCode(unsigned char* code, ULONG_PTR codeLenth, unsigned char* startaddr, ULONG_PTR addrlenth) // 带 ? 通配符的内核代码扫描函数。
{
    ULONG_PTR Ret = 0; // 匹配地址，0 表示未找到。
    ULONG_PTR Index = 0; // 当前扫描偏移。
    unsigned char x = '?'; // 模板中的通配字节。
    BOOLEAN Isfind = 0; // 当前候选是否完整匹配。
    if (!code || codeLenth == 0 || !startaddr || addrlenth == 0 || addrlenth < codeLenth)
    {
        //DbgBreakPoint();
        return 0; // 防止空指针、零长度和 addrlenth-codeLenth 无符号下溢。
    }

    while (Index < (addrlenth - codeLenth + 1))
    {
        if (!MmIsAddressValid((PVOID)&startaddr[Index]))
        {
            //if (!MmIsAddressValid((PVOID)&startaddr[Index]))continue;
            Index++; // 无效地址也要推进，避免旧逻辑在不可读地址上死循环。
            // *ppThread = NULL;
            continue; // 当前候选不可读，跳过。
        }

        if ((code[0] == x) || (code[0] == startaddr[Index]))
        {
            Isfind = 1; // 首字节匹配，继续校验完整模板。
            for (ULONG_PTR i = 0; i < codeLenth; i++)
            {
                if (!MmIsAddressValid((PVOID)&startaddr[Index + i]))
                {
                    Isfind = 0; // 模板覆盖范围内存在不可读字节，当前候选失败。
                    break; // 退出内层比较。
                }

                if ((code[i] != x) && (code[i] != startaddr[Index + i]))
                {
                    Isfind = 0; // 非通配字节不同，当前候选失败。
                    break; // 退出内层比较。
                }
            }
            if (Isfind)
            {
                Ret = (ULONG_PTR)&startaddr[Index]; // 保存第一处匹配地址。
                break; // 找到目标后结束扫描。
            }
        }
        //if (!MmIsAddressValid((PVOID)&startaddr[Index]))continue;
        Index++; // 推进到下一个候选偏移。
    }
    return Ret; // 返回匹配地址或 0。
}

EXTERN_C
//PVOID DbgkCreateThread_CMP_Debugport_01()
//{
//    PDEBUG_OBJECT Port = NULL;
//    PDEBUG_PROCESS DebugProcess;
//    _EPROCESS* Process = (_EPROCESS*)PsGetCurrentProcess();
//    DbgkRuntimeCallGuard runtimeGuard;
//
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        if (eprocess_offset::DebugPort == 0xFFFFFFFF)
//        {
//            return NULL;
//        }
//        return *(PDEBUG_OBJECT*)((size_t)Process + eprocess_offset::DebugPort);
//    }
//    DbgBreakPoint();
//    if (IsDebugTargetProcess(Process, &DebugProcess))
//        //DbgBreakPoint();
//        Port = DebugProcess->DebugObject;
//    else
//        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
//        Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
//    return Port;
//}
PVOID DbgkCreateThread_CMP_Debugport_01() // EPT hook 代理：控制 DbgkCreateThread 读取到的 DebugPort。
{
    PDEBUG_OBJECT Port = NULL; // 返回给原始比较逻辑的 DebugObject。
    PDEBUG_PROCESS DebugProcess; // 接收自定义调试进程记录。
    _EPROCESS* Process = (_EPROCESS*)PsGetCurrentProcess(); // 当前进程即创建线程路径中的目标进程。
    DbgkRuntimeCallGuard runtimeGuard; // 防止卸载流程与 hook 回调并发破坏全局状态。

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
            return NULL; // 偏移未初始化时不访问 EPROCESS，避免非法内核地址。
        }

        return *(PDEBUG_OBJECT*)((size_t)Process + eprocess_offset::DebugPort); // 卸载阶段回退原始 DebugPort。
    }

    //DbgBreakPoint();
    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        Port = DebugProcess->DebugObject; // 命中受控调试目标时返回自定义 DebugObject。
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort; // 计算系统原始 DebugPort 字段地址。
        Port = *(PDEBUG_OBJECT*)ptr_DebugPort; // 非目标进程保持系统原始行为。
    }
    return Port; // 返回给汇编 hook。
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
VOID SetupHook_DbgkCreateThread_CMP_Debugport() // Win11 DbgkCreateThread DebugPort cmp hook 安装。
{
    BYTE code[] = { 0x48, 0x83,'?','?','?','?' };  // cmp 指令模板，? 用于跳过版本相关字节。

    BYTE* startaddr = (BYTE*)Sys_DbgkCreateThread; // 从符号解析入口开始扫描。
    const ULONG_PTR searchLimit = 0x100; // 限制扫描窗口，降低误匹配其他逻辑的风险。
    ULONG_PTR searched = 0; // 累计扫描长度，保证循环有界。
    while (searched < searchLimit)
    {
        const ULONG_PTR remaining = searchLimit - searched; // 剩余扫描范围。
        BYTE* hookAddr = (BYTE*)SearchCode(code, sizeof(code), startaddr, remaining); // 查找下一个候选 cmp。
        if (hookAddr == NULL)
        {
            outLog("hook DbgkCreateThread_CMP_Debugport_01 target cmp not found.");
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // 未找到时不安装 hook。
        }

        if (!MmIsAddressValid(hookAddr) || !MmIsAddressValid(hookAddr + sizeof(code) - 1))
        {
            outLog("hook DbgkCreateThread_CMP_Debugport_01 target cmp address invalid.");
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // 候选地址不可读时退出，避免内核异常。
        }

        const ULONG_PTR instructionLength = LDE((unsigned __int8*)hookAddr, 64); // 解析真实指令长度，不能假定模板长度等于指令长度。
        if (instructionLength == 0)
        {
            outLog("hook DbgkCreateThread_CMP_Debugport_01 scan stopped on invalid instruction length.");
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // LDE 失败时停止，避免无法推进导致死循环。
        }

        //判断指令是否是cmp
        if ((hookAddr[0] == 0x48) &&
            (hookAddr[1] == 0x83) &&
            (*(WORD*)&hookAddr[3] == eprocess_offset::DebugPort))
        {
            //得到cmp的下一条指令
            DbgkCreateThread_jcc_instruction = (unsigned __int64)((unsigned __int64)hookAddr + (unsigned __int64)instructionLength); // 保存原 cmp 后继地址。
            if (hvgt::hook_function(hookAddr, Asm_DbgkCreateThread_CMP_Debugport_01, NULL))
            {
                outLog("hook DbgkCreateThread_CMP_Debugport_01成功.");
            }
            else
            {
                outLog("hook DbgkCreateThread_CMP_Debugport_01失败.");
            }
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // 成功定位目标 cmp 后结束安装。
        }

        BYTE* nextStart = hookAddr + instructionLength; // 当前候选不是 DebugPort 偏移时，跳到下一条指令继续。
        if (nextStart <= startaddr)
        {
            outLog("hook DbgkCreateThread_CMP_Debugport_01 scan cannot advance.");
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return; // 防御异常长度导致扫描无法前进。
        }

        searched += static_cast<ULONG_PTR>(nextStart - startaddr); // 更新已扫描长度。
        startaddr = nextStart; // 更新扫描起点。
    }

    outLog("hook DbgkCreateThread_CMP_Debugport_01 target cmp not found.");
}

//VOID SetupHook_PspExitThread_CMP_Debugport()
//{
//    BYTE code[] = { 0x49, 0x39,'?','?','?','?' };  //cmp指令
//    BYTE* hookAddr = (BYTE*)SearchCode(code, sizeof(code), (BYTE*)Sys_PspExitThread, 0x200);
//
//    //判断指令是否是cmp
//    if ((hookAddr[0] == 0x49) &&
//        (hookAddr[1] == 0x39) &&
//        (*(WORD*)&hookAddr[3] == eprocess_offset::DebugPort))  /*if ( Process->DebugPort )*/
//    {
//        //得到cmp的下一条指令
//        PspExitThread_jcc_instruction = (unsigned __int64)((unsigned __int64)hookAddr + (unsigned __int64)(LDE((unsigned __int8*)hookAddr, 64)));
//        if (hvgt::hook_function(hookAddr, Asm_PspExitThread_CMP_Debugport_01, NULL))
//        {
//            outLog("hook PspExitThread_CMP_Debugport_01成功.");
//        }
//        else
//            outLog("hook PspExitThread_CMP_Debugport_01失败.");
//    }
//    else
//        outLog("没有匹配到 cmp [r14+578h]");
//}
VOID SetupHook_PspExitThread_CMP_Debugport() // Win11 PspExitThread DebugPort cmp hook 安装。
{
    BYTE code[] = { 0x49, 0x39,'?','?','?','?' };  // cmp 指令模板。
    BYTE* hookAddr = (BYTE*)SearchCode(code, sizeof(code), (BYTE*)Sys_PspExitThread, 0x200); // 在函数前 0x200 字节内查找目标 cmp。
    if (hookAddr == NULL)
    {
        outLog("hook PspExitThread_CMP_Debugport_01 target cmp not found.");
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return; // 未找到候选指令时跳过安装。
    }

    if (!MmIsAddressValid(hookAddr) || !MmIsAddressValid(hookAddr + sizeof(code) - 1))
    {
        outLog("hook PspExitThread_CMP_Debugport_01 target cmp address invalid.");
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return; // 防止不可读候选地址触发页错误。
    }

    //判断指令是否是cmp
    if ((hookAddr[0] == 0x49) &&
        (hookAddr[1] == 0x39) &&
        (*(WORD*)&hookAddr[3] == eprocess_offset::DebugPort))  /*if ( Process->DebugPort )*/
    {
        //得到cmp的下一条指令
        PspExitThread_jcc_instruction = (unsigned __int64)((unsigned __int64)hookAddr + (unsigned __int64)(LDE((unsigned __int8*)hookAddr, 64))); // 保存原 cmp 后继地址，供汇编代理跳回。
        if (hvgt::hook_function(hookAddr, Asm_PspExitThread_CMP_Debugport_01, NULL))
        {
            outLog("hook PspExitThread_CMP_Debugport_01成功.");
        }
        else
        {
            outLog("hook PspExitThread_CMP_Debugport_01失败.");
        }
    }
    else
    {
        outLog("没有匹配到 cmp [r14+578h]"); // 候选模板存在但偏移不是 DebugPort，说明当前系统版本指令形态不符合预期。
    }
}

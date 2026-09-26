#include "DebugBreakInternal.h"

namespace DebugBreakInternal
{
    ULONG64 ReadDebugRegister(const CONTEXT* context, int debugRegister)
    {
        switch (debugRegister)
        {
        case 0: return context->Dr0;
        case 1: return context->Dr1;
        case 2: return context->Dr2;
        case 3: return context->Dr3;
        default: return 0;
        }
    }

    void WriteDebugRegister(CONTEXT* context, int debugRegister, ULONG64 value)
    {
        switch (debugRegister)
        {
        case 0: context->Dr0 = static_cast<DWORD_PTR>(value); break;
        case 1: context->Dr1 = static_cast<DWORD_PTR>(value); break;
        case 2: context->Dr2 = static_cast<DWORD_PTR>(value); break;
        case 3: context->Dr3 = static_cast<DWORD_PTR>(value); break;
        default: break;
        }
    }

    bool IsDebugRegisterEnabled(ULONG64 dr7, int debugRegister)
    {
        return ((dr7 >> (debugRegister * 2)) & 0x3) != 0;
    }

    int GetDebugRegisterRw(ULONG64 dr7, int debugRegister)
    {
        return static_cast<int>((dr7 >> (16 + debugRegister * 4)) & 0x3);
    }

    int GetDebugRegisterLenCode(ULONG64 dr7, int debugRegister)
    {
        return static_cast<int>((dr7 >> (18 + debugRegister * 4)) & 0x3);
    }

    bool HasEnabledDebugRegister(const CONTEXT* context)
    {
        if (context == nullptr)
        {
            // 处理打开进程失败的情况
            return false;
        }

        const ULONG64 dr7 = context->Dr7;
        for (int i = 0; i < kDebugRegisterCount; ++i)
        {
            if (IsDebugRegisterEnabled(dr7, i))
            {
                return true;
            }
        }
        return false;
    }

    int DecodeDebugRegisterLength(int lenCode)
    {
        switch (lenCode)
        {
        case BYTE_1: return 1;
        case BYTE_2: return 2;
        case BYTE_4: return 4;
        case BYTE_8: return 8;
        default: return 1;
        }
    }

    bool DecodeDebugRegisterCommand(int rwCode, unsigned __int64* command)
    {
        switch (rwCode)
        {
        case WATCH_EXECUTION_ONLY:
            // DR7.RWn = 00（执行型硬件断点）必须桥接为 EPT 执行监视，否则调试器设置的
            // 执行型硬件断点会被静默丢弃，并连带清掉同槽位已有的断点记录。
            //
            // 依据（已核对 x64dbg / CE 真实实现）：
            //   1) x64dbg 硬件断点默认类型就是执行型：src/dbg/commands/cmd-breakpoint-control.cpp
            //      `TitanHardwareBreakpointType type = UE_HARDWARE_EXECUTE;`，只有显式传 'r'/'w'
            //      才改为 UE_HARDWARE_READWRITE / UE_HARDWARE_WRITE；'x' 落到 default 分支。
            //   2) CE 的 bptExecute 同样映射为 RW=00：Cheat Engine/debughelper.pas
            //      `case bptWrite: Debugregistermask := $1; case bptAccess: Debugregistermask := $3;`
            //      bptExecute 不加任何 RW 位，即 RW=00。
            //   3) 单步/步过不使用执行型 DRx：TitanEngine.Debugger.Control.cpp 的 StepInto 用
            //      `myDBGContext.EFlags |= UE_TRAP_FLAG`（TF），StepOver 对 CALL/REP/PUSHF 用
            //      `SetBPX(..., UE_SINGLESHOOT)` 下临时 INT3，其余情况退化为 StepInto。
            //      因此"x64dbg 用执行型 DRx 做内部步过"的说法与现行实现不符。
            //   4) VMM 侧执行监视已完整接线：VmcallDispatch.cpp `case VMCALL_WATCH_EXECUTES:
            //      return SetBreakpoint(..., EPTW_EXECUTE);`，EptWatchPageInstall.cpp
            //      `set_ept_watch()` 对 EPTW_EXECUTE 清 EPT 条目的 execute 位。
            // 注：执行监视要求 DR7.LENn = 00（SDM Vol3B 17.2.4），TitanEngine 与 CE 均已强制
            // 长度为 1 字节，与本项目 DecodeDebugRegisterLength(BYTE_1) == 1 一致。
            *command = VMCALL_WATCH_EXECUTES;
            return true;
        case WATCH_WRITE:
            *command = VMCALL_WATCH_WRITES;
            // *command = VMCALL_WATCH_WRITES;
            return true;
        case WATCH_READWRITE:
            *command = VMCALL_WATCH_READS;
            // *command = VMCALL_WATCH_READS;
            return true;
        default:
            return false;
        }
    }
}

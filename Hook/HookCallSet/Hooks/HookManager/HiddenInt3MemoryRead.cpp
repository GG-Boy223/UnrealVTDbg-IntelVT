#include "../HookInternal.h"

namespace HookInternal
{
    bool HandleHiddenInt3Read(
        HANDLE process,
        LPCVOID baseAddress,
        LPVOID buffer,
        SIZE_T size,
        PSIZE_T bytesRead)
    {
        if (process == nullptr ||
            baseAddress == nullptr ||
            buffer == nullptr ||
            size == 0 ||
            !IsHiddenInt3MemoryIoTarget(process) ||
            !HasActiveHiddenInt3Breakpoints())
        {
            // 处理打开进程失败的情况
            return false;
        }

        std::vector<VT_BREAK_POINT> overlappedBreakpoints;

        INT3BreakpointList.Lock();
        for (const auto& breakpoint : INT3BreakpointList)
        {
            if (!IsSameTargetProcess(breakpoint))
            {
                continue;
            }

            SIZE_T offset = 0;
            if (TryGetWriteOffsetForAddress(baseAddress, size, breakpoint.VirtualAddress, &offset))
            {
                if (overlappedBreakpoints.empty())
                {
                    overlappedBreakpoints.reserve(4);
                }
                overlappedBreakpoints.push_back(breakpoint);
            }
        }
        INT3BreakpointList.UnLock();

        if (overlappedBreakpoints.empty())
        {
            // 处理打开进程失败的情况
            return false;
        }

        SIZE_T actualBytesRead = 0;
        bool readOk = false;
        if (Sys_NtReadVirtualMemory)
        {
            NTSTATUS status = Sys_NtReadVirtualMemory(
                process,
                const_cast<PVOID>(baseAddress),
                buffer,
                size,
                &actualBytesRead);
            readOk = NT_SUCCESS(status);
        }

        if (!readOk && Sys_ReadProcessMemory)
        {
            actualBytesRead = 0;
            readOk = Sys_ReadProcessMemory(
                process,
                baseAddress,
                buffer,
                size,
                &actualBytesRead) != FALSE;
        }

        if (!readOk)
        {
            // 处理打开进程失败的情况
            return false;
        }

        BYTE* readBytes = static_cast<BYTE*>(buffer);
        for (const auto& breakpoint : overlappedBreakpoints)
        {
            SIZE_T offset = 0;
            if (!TryGetWriteOffsetForAddress(baseAddress, actualBytesRead, breakpoint.VirtualAddress, &offset))
            {
                continue;
            }

            // ---------------------------------------------------------------------
            // 关键适配点：断点存在性校验读取必须返回 0xCC，其余读取返回原字节。
            //
            // x64dbg / x32dbg 删除软件断点的真实流程（TitanEngine.Breakpoints.cpp）：
            //   DeleteBPX(addr) {                                   // :277
            //       if (IsBPXEnabled(addr))                         // :295
            //           WriteProcessMemory(..., OriginalByte, 2);   // :297 只有这里才写回原字节
            //       BreakPointBuffer.erase(...);                    // :305 本地记录照样被删除
            //   }
            //   IsBPXEnabled(addr) {                                // :27
            //       ReadProcessMemory(..., ReadData, UE_MAX_BREAKPOINT_SIZE, ...);  // :41
            //       if (ReadData[0] == INT3BreakPoint /*0xCC*/) return true;         // :43
            //       else return false;                                              // :50
            //   }
            // 其中 UE_MAX_BREAKPOINT_SIZE == 2（TitanEngine/stdafx.h:263）。
            //
            // 若这里把 2 字节读取也回填成原字节，IsBPXEnabled 恒为 false → DeleteBPX 跳过写回
            // → 本 Hook 的隐藏 INT3 记录与 VT 侧 EPT watch 永远不会被删除，而 x64dbg 本地记录已
            // 被 erase → 被调试进程反复命中残留的 0xCC、无法继续执行，表现为"删除所有断点后被调试
            // 进程卡死（调试器本身正常）"。同理 x64dbg 的 BpSetSingleshoot（breakpoint.cpp:576）
            // 也依赖 IsBPXEnabled 为真才能重新武装步过用的临时断点。
            //
            // 因此：仅当读取"恰好从该断点地址开始、且长度为 2"（即 IsBPXEnabled 的探测）时返回
            // 0xCC —— 与普通调试器在真实内存上看到的字节完全一致；其余读取一律保持原字节：
            //   - 反汇编：MemRead 读 16/64 字节（disasm_fast.cpp:103/111）→ 仍显示原始指令；
            //   - 内存操作数：disasm_helper.cpp 读 1/4/6/8 字节 → 仍为原字节；
            //   - oldbytes 捕获（cmd-breakpoint-control.cpp:122）：发生在 IsBPXEnabled 返回 false
            //     之后，此刻不存在隐藏 INT3，因此不受本分支影响；
            //   - BpEnable 的 oldbytes 重读（breakpoint.cpp:408）：此时断点已删除，同样不受影响。
            // 被调试进程的读取不经过本 Hook（本 DLL 只注入调试器进程），它由 EPT 伪页隐藏：
            // 伪页 r/w=0，任何读取都会触发 EPT violation 并由 VMM 换回原页，隐藏效果不受影响。
            // ---------------------------------------------------------------------
            constexpr SIZE_T kBreakpointVerifyReadSize = 2; // == TitanEngine UE_MAX_BREAKPOINT_SIZE
            constexpr BYTE kInt3Opcode = 0xCC;
            const bool is_breakpoint_verify_read =
                actualBytesRead == kBreakpointVerifyReadSize &&
                ToAddressValue(baseAddress) == breakpoint.VirtualAddress;

            readBytes[offset] = is_breakpoint_verify_read
                ? kInt3Opcode
                : breakpoint.OriginalBytes;
        }

        if (bytesRead)
        {
            *bytesRead = actualBytesRead;
        }

        // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
        return true;
    }
}

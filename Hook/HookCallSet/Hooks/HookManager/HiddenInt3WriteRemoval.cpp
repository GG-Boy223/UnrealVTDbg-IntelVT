#include "../HookInternal.h"

namespace HookInternal
{
    Int3WriteRemovalResult RemoveHiddenInt3BreakpointsForWrite(
        const void* baseAddress,
        const BYTE* writeBytes,
        SIZE_T size)
    {
        Int3WriteRemovalResult result = {};
        if (writeBytes == nullptr || size == 0)
        {
            return result;
        }

        struct RemovalCandidate
        {
            VT_BREAK_POINT breakpoint;
            BYTE writeByte;
            bool exactRestore;
        };

        std::vector<RemovalCandidate> candidates;
        candidates.reserve(4);

        INT3BreakpointList.Lock();
        for (const auto& breakpoint : INT3BreakpointList)
        {
            if (!IsSameTargetProcess(breakpoint))
            {
                continue;
            }

            SIZE_T offset = 0;
            if (!TryGetWriteOffsetForAddress(baseAddress, size, breakpoint.VirtualAddress, &offset))
            {
                continue;
            }

            BYTE writeByte = 0;
            if (!TryReadWriteBufferByte(writeBytes, offset, &writeByte))
            {
                continue;
            }

            // 写入非 0xCC：CE 正在恢复原字节，或者用户正在覆盖这段代码，隐藏断点必须先解除。
            // 原始字节本身为 0xCC 时，删除操作写回的也是 0xCC，不能把它误判成重复设置。
            if (writeByte != 0xCC || breakpoint.OriginalBytes == 0xCC)
            {
                RemovalCandidate candidate = {};
                candidate.breakpoint = breakpoint;
                candidate.writeByte = writeByte;
                // 恢复写入的长度与读侧共用同一常量：x64dbg / TitanEngine 的 DeleteBPX 写回
                // BreakPointSize == UE_MAX_BREAKPOINT_SIZE == 2 字节（TitanEngine.Breakpoints.cpp:297），
                // 而断点存在性校验 IsBPXEnabled 读的也是 2 字节。原判据写死 size == 1，
                // 于是删除时的 2 字节写回永远被判成「非精确恢复」，HandleHiddenInt3Write 会把
                // 写入透传给真实 WriteProcessMemory，多走一次 EPT violation。
                candidate.exactRestore =
                    size <= kBreakpointVerifyIoSize &&
                    ToAddressValue(baseAddress) == breakpoint.VirtualAddress &&
                    writeByte == breakpoint.OriginalBytes;
                candidates.push_back(candidate);
            }
        }
        INT3BreakpointList.UnLock();

        if (candidates.empty())
        {
            return result;
        }

        result.attempted = true;
        result.exactRestoreOnly = true;
        for (const auto& candidate : candidates)
        {
            if (!candidate.exactRestore)
            {
                result.exactRestoreOnly = false;
            }

            if (!DeleteInt3BreakpointFromDriver(candidate.breakpoint))
            {
                result.failed = true;
                // 旧日志包含 watchid，避免在日志文件暴露内部断点编号。
                // logger.Log("删除cc断点失败 地址:%p watchid:%d 写入:%02X 原字节:%02X", ...);
                logger.Log("删除cc断点失败 地址:%p 写入:%02X 原字节:%02X",
                    reinterpret_cast<PVOID>(candidate.breakpoint.VirtualAddress),
                    static_cast<unsigned int>(candidate.writeByte),
                    static_cast<unsigned int>(candidate.breakpoint.OriginalBytes));
                continue;
            }

            RemoveInt3BreakpointRecord(candidate.breakpoint);
            ++result.removedCount;
        }

        return result;
    }
}

#include "DebugBreakInternal.h"

namespace
{
    SRWLOCK g_hardware_breakpoint_transaction_lock = SRWLOCK_INIT;
}

namespace DebugBreakInternal
{
    HardwareBreakpointTransactionGuard::HardwareBreakpointTransactionGuard() noexcept
    {
        AcquireSRWLockExclusive(&g_hardware_breakpoint_transaction_lock);
    }

    HardwareBreakpointTransactionGuard::~HardwareBreakpointTransactionGuard()
    {
        ReleaseSRWLockExclusive(&g_hardware_breakpoint_transaction_lock);
    }

    namespace
    {
        bool SameBreakpointIdentity(
            const BREAKPOINT_RECORD& left,
            const BREAKPOINT_RECORD& right)
        {
            if (left.watchid >= 0 && right.watchid >= 0)
            {
                return left.watchid == right.watchid;
            }

            return left.Address == right.Address &&
                left.command == right.command &&
                left.length == right.length &&
                left.debugRegister == right.debugRegister &&
                left.debugRw == right.debugRw &&
                left.debugLen == right.debugLen;
        }

        bool EraseLocalBreakpoint(const BREAKPOINT_RECORD& record)
        {
            bool erased = false;

            BreakpointList.Lock();
            for (auto it = BreakpointList.begin(); it != BreakpointList.end(); ++it)
            {
                if (!SameBreakpointIdentity(*it, record))
                {
                    continue;
                }

                BreakpointList.erase(it);
                erased = true;
                break;
            }
            UpdateActiveHardwareBreakpointCountLocked();
            BreakpointList.UnLock();

            return erased;
        }

        bool FindLocalBreakpoint(
            ULONG64 address,
            int debugRegister,
            unsigned __int64 command,
            int length,
            int debugRw,
            int debugLen,
            BREAKPOINT_RECORD* record)
        {
            if (record == nullptr)
            {
                return false;
            }

            bool found = false;
            BreakpointList.Lock();
            for (const auto& breakpoint : BreakpointList)
            {
                if (SameBreakpointRequest(
                    breakpoint,
                    address,
                    command,
                    length,
                    debugRegister,
                    debugRw,
                    debugLen))
                {
                    *record = breakpoint;
                    found = true;
                    break;
                }
            }
            BreakpointList.UnLock();
            return found;
        }

        void RollbackRemovedBreakpoints(
            const BREAKPOINT_RECORD* removed,
            int removedCount)
        {
            if (removed == nullptr)
            {
                return;
            }

            for (int i = removedCount - 1; i >= 0; --i)
            {
                const BREAKPOINT_RECORD& oldBreakpoint = removed[i];
                if (!SetBreakpoint(
                    reinterpret_cast<PVOID>(oldBreakpoint.Address),
                    oldBreakpoint.command,
                    oldBreakpoint.length,
                    oldBreakpoint.debugRegister,
                    oldBreakpoint.debugRw,
                    oldBreakpoint.debugLen))
                {
                    logger.Log("硬件断点事务回滚失败: errorCode=1004, dr=%d, addr=0x%llX, cr3=0x%llX, pid=%u, watchid=%d, rw=%d, len=%d",
                        oldBreakpoint.debugRegister,
                        oldBreakpoint.Address,
                        oldBreakpoint.cr3,
                        oldBreakpoint.pid,
                        oldBreakpoint.watchid,
                        oldBreakpoint.debugRw,
                        oldBreakpoint.debugLen);
                }
            }
        }
    }

    bool IsCurrentHardwareBreakpointState(const CONTEXT* context)
    {
        if (context == nullptr)
        {
            // 处理打开进程失败的情况
            return false;
        }

        HardwareBreakpointTransactionGuard transactionGuard;

        bool desired[kDebugRegisterCount] = {};
        ULONG64 addresses[kDebugRegisterCount] = {};
        unsigned __int64 commands[kDebugRegisterCount] = {};
        int lengths[kDebugRegisterCount] = {};
        int rwCodes[kDebugRegisterCount] = {};
        int lenCodes[kDebugRegisterCount] = {};

        const ULONG64 dr7 = context->Dr7;
        for (int i = 0; i < kDebugRegisterCount; ++i)
        {
            const ULONG64 address = ReadDebugRegister(context, i);
            if (!address || !IsDebugRegisterEnabled(dr7, i))
            {
                continue;
            }

            const int rwCode = GetDebugRegisterRw(dr7, i);
            unsigned __int64 command = 0;
            if (!DecodeDebugRegisterCommand(rwCode, &command))
            {
                // 处理打开进程失败的情况
                return false;
            }

            const int lenCode = GetDebugRegisterLenCode(dr7, i);
            desired[i] = true;
            addresses[i] = address;
            commands[i] = command;
            lengths[i] = DecodeDebugRegisterLength(lenCode);
            rwCodes[i] = rwCode;
            lenCodes[i] = lenCode;
        }

        bool present[kDebugRegisterCount] = {};
        bool same = true;

        BreakpointList.Lock();
        for (const auto& breakpoint : BreakpointList)
        {
            const int debugRegister = breakpoint.debugRegister;
            if (debugRegister < 0 ||
                debugRegister >= kDebugRegisterCount ||
                present[debugRegister] ||
                !desired[debugRegister] ||
                !SameBreakpointRequest(breakpoint,
                    addresses[debugRegister],
                    commands[debugRegister],
                    lengths[debugRegister],
                    debugRegister,
                    rwCodes[debugRegister],
                    lenCodes[debugRegister]))
            {
                same = false;
                break;
            }

            present[debugRegister] = true;
        }

        if (same)
        {
            for (int i = 0; i < kDebugRegisterCount; ++i)
            {
                if (desired[i] != present[i])
                {
                    same = false;
                    break;
                }
            }
        }

        BreakpointList.UnLock();
        return same;
    }

    bool SyncHardwareBreakpointsFromContext(const CONTEXT* context)
    {
        if (context == nullptr)
        {
            return false;
        }

        HardwareBreakpointTransactionGuard transactionGuard;

        bool desired[kDebugRegisterCount] = {};
        ULONG64 addresses[kDebugRegisterCount] = {};
        unsigned __int64 commands[kDebugRegisterCount] = {};
        int lengths[kDebugRegisterCount] = {};
        int rwCodes[kDebugRegisterCount] = {};
        int lenCodes[kDebugRegisterCount] = {};

        const ULONG64 dr7 = context->Dr7;
        for (int i = 0; i < kDebugRegisterCount; ++i)
        {
            const ULONG64 address = ReadDebugRegister(context, i);
            if (!address || !IsDebugRegisterEnabled(dr7, i))
            {
                continue;
            }

            const int rwCode = GetDebugRegisterRw(dr7, i);
            unsigned __int64 command = 0;
            if (!DecodeDebugRegisterCommand(rwCode, &command))
            {
                // 执行型(00)/写(01)/读写(11) 均已桥接为 EPT 监视；这里只剩 RW=10
                // （I/O 读写断点，仅 CR4.DE=1 的旧平台上有效，SDM Vol3B 17.2.4 已标记为
                // 保留/废弃）。此类断点无法用 EPT 表达，明确记为不同步而不是静默忽略。
                if (rwCode != WATCH_EXECUTION_ONLY)
                {
                    logger.Log("不支持的硬件断点类型: errorCode=1002, dr=%d, rw=%d, dr7=0x%llX",
                        i,
                        rwCode,
                        dr7);
                    return false;
                }
                continue;
            }

            const int lenCode = GetDebugRegisterLenCode(dr7, i);
            desired[i] = true;
            addresses[i] = address;
            commands[i] = command;
            lengths[i] = DecodeDebugRegisterLength(lenCode);
            rwCodes[i] = rwCode;
            lenCodes[i] = lenCode;
        }

        BREAKPOINT_RECORD original[kDebugRegisterCount] = {};
        int originalCount = 0;
        BreakpointList.Lock();
        for (const auto& breakpoint : BreakpointList)
        {
            if (originalCount >= kDebugRegisterCount)
            {
                BreakpointList.UnLock();
                logger.Log("硬件断点本地记录超过 DR0-DR3 容量，拒绝执行非事务同步: errorCode=1003");
                return false;
            }

            original[originalCount++] = breakpoint;
        }
        BreakpointList.UnLock();

        BREAKPOINT_RECORD stale[kDebugRegisterCount] = {};
        int staleCount = 0;
        bool desiredPresent[kDebugRegisterCount] = {};

        for (int index = 0; index < originalCount; ++index)
        {
            const BREAKPOINT_RECORD& breakpoint = original[index];
            const int debugRegister = breakpoint.debugRegister;
            const bool invalidRegister =
                debugRegister < 0 || debugRegister >= kDebugRegisterCount;

            if (!invalidRegister &&
                desired[debugRegister] &&
                SameBreakpointRequest(
                    breakpoint,
                    addresses[debugRegister],
                    commands[debugRegister],
                    lengths[debugRegister],
                    debugRegister,
                    rwCodes[debugRegister],
                    lenCodes[debugRegister]))
            {
                desiredPresent[debugRegister] = true;
                continue;
            }

            stale[staleCount++] = breakpoint;
        }

        BREAKPOINT_RECORD removed[kDebugRegisterCount] = {};
        int removedCount = 0;
        for (int index = 0; index < staleCount; ++index)
        {
            const BREAKPOINT_RECORD& oldBreakpoint = stale[index];
            if (!DeleteBreakpointFromDriver(oldBreakpoint))
            {
                logger.Log("移除硬件断点驱动返回失败，事务中止并恢复已有变更: errorCode=1004, dr=%d, addr=0x%llX, cr3=0x%llX, pid=%u, watchid=%d",
                    oldBreakpoint.debugRegister,
                    oldBreakpoint.Address,
                    oldBreakpoint.cr3,
                    oldBreakpoint.pid,
                    oldBreakpoint.watchid);
                RollbackRemovedBreakpoints(removed, removedCount);
                return false;
            }

            if (!EraseLocalBreakpoint(oldBreakpoint))
            {
                logger.Log("硬件断点驱动已删除但本地记录未找到: errorCode=1001, dr=%d, addr=0x%llX, cr3=0x%llX, pid=%u, watchid=%d",
                    oldBreakpoint.debugRegister,
                    oldBreakpoint.Address,
                    oldBreakpoint.cr3,
                    oldBreakpoint.pid,
                    oldBreakpoint.watchid);
            }
            removed[removedCount++] = oldBreakpoint;
        }

        BREAKPOINT_RECORD added[kDebugRegisterCount] = {};
        int addedCount = 0;
        for (int i = 0; i < kDebugRegisterCount; ++i)
        {
            if (!desired[i] || desiredPresent[i])
            {
                continue;
            }

            if (!SetBreakpoint(
                reinterpret_cast<PVOID>(addresses[i]),
                commands[i],
                lengths[i],
                i,
                rwCodes[i],
                lenCodes[i]))
            {
                logger.Log("添加硬件断点失败，事务回滚: errorCode=1004, dr=%d, addr=0x%llX, command=0x%llX, len=%d",
                    i,
                    addresses[i],
                    commands[i],
                    lengths[i]);

                for (int rollback = addedCount - 1; rollback >= 0; --rollback)
                {
                    if (DeleteBreakpointFromDriver(added[rollback]))
                    {
                        EraseLocalBreakpoint(added[rollback]);
                    }
                    else
                    {
                        logger.Log("回滚新硬件断点失败: errorCode=1004, dr=%d, addr=0x%llX, cr3=0x%llX, pid=%u, watchid=%d",
                            added[rollback].debugRegister,
                            added[rollback].Address,
                            added[rollback].cr3,
                            added[rollback].pid,
                            added[rollback].watchid);
                    }
                }

                RollbackRemovedBreakpoints(removed, removedCount);
                return false;
            }

            BREAKPOINT_RECORD installed = {};
            if (!FindLocalBreakpoint(
                addresses[i],
                i,
                commands[i],
                lengths[i],
                rwCodes[i],
                lenCodes[i],
                &installed))
            {
                logger.Log("硬件断点安装成功但本地记录未找到: errorCode=1001, dr=%d, addr=0x%llX, command=0x%llX, len=%d",
                    i,
                    addresses[i],
                    commands[i],
                    lengths[i]);

                for (int rollback = addedCount - 1; rollback >= 0; --rollback)
                {
                    if (DeleteBreakpointFromDriver(added[rollback]))
                    {
                        EraseLocalBreakpoint(added[rollback]);
                    }
                }

                RollbackRemovedBreakpoints(removed, removedCount);
                return false;
            }

            added[addedCount++] = installed;
        }

        return true;
    }
}

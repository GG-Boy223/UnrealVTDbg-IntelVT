#pragma once

#include <ntifs.h>

namespace cr3_tracker_internal
{
    // 每个 watch 页现在持有独立的 probe 身份（pid + probe VA/PFN + CR3 root），
    // tracker 容量必须能覆盖整个 EPT watch 表，避免多页 watch 在 128 处提前失败。
    constexpr int kTrackerCount = 256;

    struct TRACKER_ENTRY
    {
        unsigned __int64 cr3;
        unsigned __int64 pid; // 目标进程 ID；CR3 更新必须与 PID 身份一致
        unsigned __int64 probeVirtualAddress;
        unsigned __int64 probePfn;
        unsigned __int64 pml4Fingerprint;
        unsigned __int64 generation;
        LONG referenceCount;
        BOOLEAN active;
    };

    struct TRACKER_SNAPSHOT
    {
        int trackerId;
        unsigned __int64 probeVirtualAddress;
        unsigned __int64 probePfn;
        unsigned __int64 pml4Fingerprint;
        unsigned __int64 generation;
    };

    extern volatile LONG g_ActiveTrackerCount;
    // 保护 g_Trackers 表条目与引用计数的自旋锁：
    // Acquire/Release 会在 VMCALL 事务上下文改写表项，而 Observe 在任意 CPU 的
    // EPT violation（INT3 命中）热路径并发读取同一张表；两者必须使用同一把锁，
    // CommitObservedCr3 之前依赖的 generation 校验才有意义（否则表项可能被在
    // 校验与写入之间复用/清零，形成 ABA 与丢失更新）。
    extern volatile LONG g_Cr3TrackerLock;
    extern TRACKER_ENTRY g_Trackers[kTrackerCount];
    extern volatile LONG64 g_AcquireCount;
    extern volatile LONG64 g_AcquireFailureCount;
    extern volatile LONG64 g_ObserveCount;
    extern volatile LONG64 g_FingerprintRejectCount;
    extern volatile LONG64 g_TranslationRejectCount;
    extern volatile LONG64 g_MatchCount;
    extern volatile LONG64 g_UpdateCount;
    extern volatile LONG64 g_PolicyEnableCount;
    extern volatile LONG64 g_PolicyDisableCount;
    extern volatile LONG64 g_TargetUpdateCount;

    unsigned __int64 NormalizeCr3(unsigned __int64 value);
    unsigned __int64 ReadPml4Fingerprint(
        unsigned __int64 cr3,
        unsigned __int64 virtualAddress);
    unsigned __int64 TranslateProbe(
        unsigned __int64 cr3,
        unsigned __int64 virtualAddress);
}

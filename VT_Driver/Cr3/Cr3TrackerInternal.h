#pragma once

#include <ntifs.h>

namespace cr3_tracker_internal
{
    // ✅ P1-002 修复：从 16 扩大到 128，避免在多进程环境下候选池溢出
    constexpr int kTrackerCount = 128;

    struct TRACKER_ENTRY
    {
        unsigned __int64 cr3;
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

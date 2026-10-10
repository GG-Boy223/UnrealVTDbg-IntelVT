#include "Cr3Tracker.h"
#include "Cr3TrackerInternal.h"

#include "../Driver.h"
#include "../Globals.h"
#include "../spinlock.h"

namespace cr3_tracker_internal
{
    volatile LONG g_ActiveTrackerCount = 0;
    volatile LONG g_Cr3TrackerLock = 0;
    TRACKER_ENTRY g_Trackers[kTrackerCount] = { 0 };
    volatile LONG64 g_AcquireCount = 0;
    volatile LONG64 g_AcquireFailureCount = 0;
    volatile LONG64 g_ObserveCount = 0;
    volatile LONG64 g_FingerprintRejectCount = 0;
    volatile LONG64 g_TranslationRejectCount = 0;
    volatile LONG64 g_MatchCount = 0;
    volatile LONG64 g_UpdateCount = 0;
    volatile LONG64 g_PolicyEnableCount = 0;
    volatile LONG64 g_PolicyDisableCount = 0;
    volatile LONG64 g_TargetUpdateCount = 0;
}

namespace
{
    using namespace cr3_tracker_internal;

    int SnapshotTrackers(TRACKER_SNAPSHOT* snapshots)
    {
        int snapshotCount = 0;
        for (int index = 0; index < kTrackerCount; ++index)
        {
            const TRACKER_ENTRY& entry = g_Trackers[index];
            if (!entry.active)
            {
                continue;
            }

            TRACKER_SNAPSHOT& snapshot = snapshots[snapshotCount++];
            snapshot.trackerId = index;
            snapshot.probeVirtualAddress = entry.probeVirtualAddress;
            snapshot.probePfn = entry.probePfn;
            snapshot.pml4Fingerprint = entry.pml4Fingerprint;
            snapshot.generation = entry.generation;
        }
        return snapshotCount;
    }

    bool CommitObservedCr3(
        const TRACKER_SNAPSHOT& snapshot,
        unsigned __int64 root,
        unsigned __int64 current_pid)
    {
        bool matched = false;
        // 锁序：g_Cr3TrackerLock -> eptWatchList_lock。
        // Acquire/Release 只取第一把锁；调用方都在释放 eptWatchList_lock 之后才调用
        // Cr3TrackerRelease，因此不存在反向获取路径。
        spinlock::lock(&g_Cr3TrackerLock);
        spinlock::lock(&eptWatchList_lock);
        TRACKER_ENTRY& entry = g_Trackers[snapshot.trackerId];
        if (entry.active &&
            // CR3 更新代表“同一目标进程换了一个 root”。PML4E 指纹 + probe PFN
            // 只能证明 VA→PA 映射一致，无法证明进程身份；共享内核高半区/共享页表
            // 会让不同进程满足该条件。PID 不可用或不一致时禁止更新，调用方回退
            // 到 CR3 匹配，避免把别的地址空间写进 watch 记录。
            entry.pid != 0 &&
            current_pid != 0 &&
            entry.pid == current_pid &&
            entry.generation == snapshot.generation &&
            entry.probeVirtualAddress == snapshot.probeVirtualAddress &&
            entry.probePfn == snapshot.probePfn &&
            entry.pml4Fingerprint == snapshot.pml4Fingerprint)
        {
            matched = true;
            InterlockedIncrement64(&g_MatchCount);
            if (entry.cr3 != root)
            {
                entry.cr3 = root;
                ++entry.generation;

                for (int index = 0; index < EPTWATCHLISTSIZE; ++index)
                {
                    if (ept_watch_entry_is_reserved(eptWatchList[index]) &&
                        eptWatchList[index].Cr3TrackerId == snapshot.trackerId)
                    {
                        InterlockedExchange64(
                            reinterpret_cast<volatile LONG64*>(
                                &eptWatchList[index].cr3),
                            static_cast<LONG64>(root));
                    }
                }
                InterlockedIncrement64(&g_UpdateCount);
            }
        }
        spinlock::unlock(&eptWatchList_lock);
        spinlock::unlock(&g_Cr3TrackerLock);
        return matched;
    }
}

int Cr3TrackerAcquire(
    unsigned __int64 initialCr3,
    unsigned __int64 probeVirtualAddress,
    unsigned __int64 probePhysicalAddress,
    unsigned __int64 pid,
    int* failureDetail)
{
    using namespace cr3_tracker_internal;

    if (failureDetail != nullptr)
    {
        *failureDetail = kCr3TrackerFailureNone;
    }

    InterlockedIncrement64(&g_AcquireCount);
    const unsigned __int64 root = NormalizeCr3(initialCr3);
    const unsigned __int64 probePfn = probePhysicalAddress >> PAGE_SHIFT;
    const unsigned __int64 fingerprint =
        ReadPml4Fingerprint(root, probeVirtualAddress);
    if (root == 0)
    {
        if (failureDetail != nullptr)
        {
            *failureDetail = kCr3TrackerFailureInvalidRoot;
        }
        InterlockedIncrement64(&g_AcquireFailureCount);
        return kInvalidCr3TrackerId;
    }
    if (probePfn == 0)
    {
        if (failureDetail != nullptr)
        {
            *failureDetail = kCr3TrackerFailureInvalidPfn;
        }
        InterlockedIncrement64(&g_AcquireFailureCount);
        return kInvalidCr3TrackerId;
    }
    if (fingerprint == 0)
    {
        if (failureDetail != nullptr)
        {
            *failureDetail = kCr3TrackerFailureFingerprint;
        }
        InterlockedIncrement64(&g_AcquireFailureCount);
        return kInvalidCr3TrackerId;
    }

    int freeIndex = kInvalidCr3TrackerId;
    spinlock::lock(&g_Cr3TrackerLock);
    for (int index = 0; index < kTrackerCount; ++index)
    {
        TRACKER_ENTRY& entry = g_Trackers[index];
        if (!entry.active)
        {
            if (freeIndex == kInvalidCr3TrackerId)
            {
                freeIndex = index;
            }
            continue;
        }

        // 同一 tracker 只在同进程内复用；PID 已知且不一致时必须分配独立条目，
        // 否则两个共享同一 VA→PA 映射的进程会共用 cr3 状态。
        const bool same_process =
            (entry.pid != 0 && pid != 0)
            ? (entry.pid == pid)
            : true;
        // probe VA 也必须是同一张 watch 页：否则同一 CR3 下不同页会共用
        // tracker 的指纹/PFN，进程 root 变化时无法证明该 watch 页的映射未变。
        const bool same_probe =
            entry.probeVirtualAddress == probeVirtualAddress;
        if (same_process && same_probe &&
            entry.pml4Fingerprint == fingerprint &&
            entry.probePfn == probePfn)
        {
            if (entry.cr3 != root)
            {
                entry.cr3 = root;
                ++entry.generation;
            }
            ++entry.referenceCount;
            spinlock::unlock(&g_Cr3TrackerLock);
            return index;
        }
    }

    if (freeIndex != kInvalidCr3TrackerId)
    {
        TRACKER_ENTRY& entry = g_Trackers[freeIndex];
        entry.cr3 = root;
        entry.pid = pid;
        entry.probeVirtualAddress = probeVirtualAddress;
        entry.probePfn = probePfn;
        entry.pml4Fingerprint = fingerprint;
        entry.generation = 1;
        entry.referenceCount = 1;
        entry.active = TRUE;
        InterlockedIncrement(&g_ActiveTrackerCount);
    }
    spinlock::unlock(&g_Cr3TrackerLock);

    if (freeIndex == kInvalidCr3TrackerId)
    {
        if (failureDetail != nullptr)
        {
            *failureDetail = kCr3TrackerFailurePoolFull;
        }
        InterlockedIncrement64(&g_AcquireFailureCount);
    }
    return freeIndex;
}

void Cr3TrackerRelease(int trackerId)
{
    using namespace cr3_tracker_internal;

    if (trackerId < 0 || trackerId >= kTrackerCount)
    {
        return;
    }

    TRACKER_ENTRY& entry = g_Trackers[trackerId];
    spinlock::lock(&g_Cr3TrackerLock);
    if (entry.active && --entry.referenceCount <= 0)
    {
        entry = { 0 };
        InterlockedDecrement(&g_ActiveTrackerCount);
    }
    spinlock::unlock(&g_Cr3TrackerLock);
}

void Cr3TrackerObserve(unsigned __int64 candidateCr3, unsigned __int64 current_pid)
{
    using namespace cr3_tracker_internal;

    if (InterlockedCompareExchange(&g_ActiveTrackerCount, 0, 0) == 0)
    {
        return;
    }

    InterlockedIncrement64(&g_ObserveCount);
    const unsigned __int64 root = NormalizeCr3(candidateCr3);
    if (root == 0)
    {
        return;
    }

    TRACKER_SNAPSHOT snapshots[kTrackerCount] = { 0 };
    spinlock::lock(&g_Cr3TrackerLock);
    const int snapshotCount = SnapshotTrackers(snapshots);
    spinlock::unlock(&g_Cr3TrackerLock);
    for (int index = 0; index < snapshotCount; ++index)
    {
        const TRACKER_SNAPSHOT& snapshot = snapshots[index];
        if (ReadPml4Fingerprint(root, snapshot.probeVirtualAddress) !=
            snapshot.pml4Fingerprint)
        {
            InterlockedIncrement64(&g_FingerprintRejectCount);
            continue;
        }

        const unsigned __int64 physicalAddress =
            TranslateProbe(root, snapshot.probeVirtualAddress);
        if (physicalAddress == 0 ||
            physicalAddress >> PAGE_SHIFT != snapshot.probePfn)
        {
            InterlockedIncrement64(&g_TranslationRejectCount);
            continue;
        }

        if (CommitObservedCr3(snapshot, root, current_pid))
        {
            break;
        }
    }
}

void Cr3TrackerReset()
{
    using namespace cr3_tracker_internal;

    spinlock::lock(&g_Cr3TrackerLock);
    RtlZeroMemory(g_Trackers, sizeof(g_Trackers));
    InterlockedExchange(&g_ActiveTrackerCount, 0);
    spinlock::unlock(&g_Cr3TrackerLock);

    InterlockedExchange64(&g_AcquireCount, 0);
    InterlockedExchange64(&g_AcquireFailureCount, 0);
    InterlockedExchange64(&g_ObserveCount, 0);
    InterlockedExchange64(&g_FingerprintRejectCount, 0);
    InterlockedExchange64(&g_TranslationRejectCount, 0);
    InterlockedExchange64(&g_MatchCount, 0);
    InterlockedExchange64(&g_UpdateCount, 0);
    InterlockedExchange64(&g_PolicyEnableCount, 0);
    InterlockedExchange64(&g_PolicyDisableCount, 0);
    InterlockedExchange64(&g_TargetUpdateCount, 0);
}

void Cr3TrackerQueryTelemetry(CR3_TRACKER_TELEMETRY* telemetry)
{
    using namespace cr3_tracker_internal;

    if (telemetry == nullptr)
    {
        return;
    }

    telemetry->acquireCount = InterlockedCompareExchange64(&g_AcquireCount, 0, 0);
    telemetry->acquireFailureCount = InterlockedCompareExchange64(&g_AcquireFailureCount, 0, 0);
    telemetry->observeCount = InterlockedCompareExchange64(&g_ObserveCount, 0, 0);
    telemetry->fingerprintRejectCount = InterlockedCompareExchange64(&g_FingerprintRejectCount, 0, 0);
    telemetry->translationRejectCount = InterlockedCompareExchange64(&g_TranslationRejectCount, 0, 0);
    telemetry->matchCount = InterlockedCompareExchange64(&g_MatchCount, 0, 0);
    telemetry->updateCount = InterlockedCompareExchange64(&g_UpdateCount, 0, 0);
    telemetry->policyEnableCount = InterlockedCompareExchange64(&g_PolicyEnableCount, 0, 0);
    telemetry->policyDisableCount = InterlockedCompareExchange64(&g_PolicyDisableCount, 0, 0);
    telemetry->targetUpdateCount = InterlockedCompareExchange64(&g_TargetUpdateCount, 0, 0);
}

void Cr3TrackerDumpTelemetry()
{
    CR3_TRACKER_TELEMETRY telemetry = { 0 };
    Cr3TrackerQueryTelemetry(&telemetry);
    //DbgPrintEx(
    //    DPFLTR_IHVDRIVER_ID,
    //    DPFLTR_INFO_LEVEL,
    //    "[UnrealDbg][CR3Tracker] acquire=%llu failed=%llu observe=%llu "
    //    "fingerprintReject=%llu translationReject=%llu match=%llu update=%llu "
    //    "policy=%llu/%llu targets=%llu\n",
    //    telemetry.acquireCount,
    //    telemetry.acquireFailureCount,
    //    telemetry.observeCount,
    //    telemetry.fingerprintRejectCount,
    //    telemetry.translationRejectCount,
    //    telemetry.matchCount,
    //    telemetry.updateCount,
    //    telemetry.policyEnableCount,
    //    telemetry.policyDisableCount,
    //    telemetry.targetUpdateCount);
}

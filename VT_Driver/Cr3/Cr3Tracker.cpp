#include "Cr3Tracker.h"
#include "Cr3TrackerInternal.h"

#include "../Driver.h"
#include "../Globals.h"
#include "../spinlock.h"

namespace cr3_tracker_internal
{
    volatile long g_TrackerLock = 0;
    volatile LONG g_ActiveTrackerCount = 0;
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
        spinlock::lock(&g_TrackerLock);
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
        spinlock::unlock(&g_TrackerLock);
        return snapshotCount;
    }

    bool CommitObservedCr3(
        const TRACKER_SNAPSHOT& snapshot,
        unsigned __int64 root)
    {
        bool matched = false;
        spinlock::lock(&g_TrackerLock);
        TRACKER_ENTRY& entry = g_Trackers[snapshot.trackerId];
        if (entry.active &&
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

                spinlock::lock(&eptWatchList_lock);
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
                spinlock::unlock(&eptWatchList_lock);
                InterlockedIncrement64(&g_UpdateCount);
            }
        }
        spinlock::unlock(&g_TrackerLock);
        return matched;
    }
}

int Cr3TrackerAcquire(
    unsigned __int64 initialCr3,
    unsigned __int64 probeVirtualAddress,
    unsigned __int64 probePhysicalAddress)
{
    using namespace cr3_tracker_internal;

    InterlockedIncrement64(&g_AcquireCount);
    const unsigned __int64 root = NormalizeCr3(initialCr3);
    const unsigned __int64 probePfn = probePhysicalAddress >> PAGE_SHIFT;
    const unsigned __int64 fingerprint =
        ReadPml4Fingerprint(root, probeVirtualAddress);
    if (root == 0 || probePfn == 0 || fingerprint == 0)
    {
        InterlockedIncrement64(&g_AcquireFailureCount);
        return kInvalidCr3TrackerId;
    }

    int freeIndex = kInvalidCr3TrackerId;
    spinlock::lock(&g_TrackerLock);
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

        if (entry.cr3 == root ||
            (entry.pml4Fingerprint == fingerprint && entry.probePfn == probePfn))
        {
            entry.cr3 = root;
            ++entry.referenceCount;
            spinlock::unlock(&g_TrackerLock);
            return index;
        }
    }

    if (freeIndex != kInvalidCr3TrackerId)
    {
        TRACKER_ENTRY& entry = g_Trackers[freeIndex];
        entry.cr3 = root;
        entry.probeVirtualAddress = probeVirtualAddress;
        entry.probePfn = probePfn;
        entry.pml4Fingerprint = fingerprint;
        entry.generation = 1;
        entry.referenceCount = 1;
        entry.active = TRUE;
        InterlockedIncrement(&g_ActiveTrackerCount);
    }
    spinlock::unlock(&g_TrackerLock);

    if (freeIndex == kInvalidCr3TrackerId)
    {
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

    spinlock::lock(&g_TrackerLock);
    TRACKER_ENTRY& entry = g_Trackers[trackerId];
    if (entry.active && --entry.referenceCount <= 0)
    {
        entry = { 0 };
        InterlockedDecrement(&g_ActiveTrackerCount);
    }
    spinlock::unlock(&g_TrackerLock);
}

void Cr3TrackerObserve(unsigned __int64 candidateCr3)
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
    const int snapshotCount = SnapshotTrackers(snapshots);
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

        if (CommitObservedCr3(snapshot, root))
        {
            break;
        }
    }
}

void Cr3TrackerReset()
{
    using namespace cr3_tracker_internal;

    spinlock::lock(&g_TrackerLock);
    RtlZeroMemory(g_Trackers, sizeof(g_Trackers));
    InterlockedExchange(&g_ActiveTrackerCount, 0);
    spinlock::unlock(&g_TrackerLock);

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
    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "[UnrealDbg][CR3Tracker] acquire=%llu failed=%llu observe=%llu "
        "fingerprintReject=%llu translationReject=%llu match=%llu update=%llu "
        "policy=%llu/%llu targets=%llu\n",
        telemetry.acquireCount,
        telemetry.acquireFailureCount,
        telemetry.observeCount,
        telemetry.fingerprintRejectCount,
        telemetry.translationRejectCount,
        telemetry.matchCount,
        telemetry.updateCount,
        telemetry.policyEnableCount,
        telemetry.policyDisableCount,
        telemetry.targetUpdateCount);
}

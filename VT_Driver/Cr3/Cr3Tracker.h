#pragma once

#include <ntifs.h>

struct __vcpu;

constexpr int kInvalidCr3TrackerId = -1;

// Detailed Cr3TrackerAcquire failure reasons; reported as 140 + detail.
constexpr int kCr3TrackerFailureNone = 0;
constexpr int kCr3TrackerFailureInvalidRoot = 1;
constexpr int kCr3TrackerFailureInvalidPfn = 2;
constexpr int kCr3TrackerFailureFingerprint = 3;
constexpr int kCr3TrackerFailurePoolFull = 4;

struct CR3_TRACKER_TELEMETRY
{
    unsigned __int64 acquireCount;
    unsigned __int64 acquireFailureCount;
    unsigned __int64 observeCount;
    unsigned __int64 fingerprintRejectCount;
    unsigned __int64 translationRejectCount;
    unsigned __int64 matchCount;
    unsigned __int64 updateCount;
    unsigned __int64 policyEnableCount;
    unsigned __int64 policyDisableCount;
    unsigned __int64 targetUpdateCount;
};

int Cr3TrackerAcquire(
    unsigned __int64 initialCr3,
    unsigned __int64 probeVirtualAddress,
    unsigned __int64 probePhysicalAddress,
    unsigned __int64 pid,
    int* failureDetail = nullptr);

void Cr3TrackerRelease(int trackerId);
void Cr3TrackerObserve(unsigned __int64 candidateCr3, unsigned __int64 current_pid);
void Cr3TrackerPublishCandidate(__vcpu* vcpu, unsigned __int64 candidateCr3);
bool Cr3TrackerStartWorker();
void Cr3TrackerStopWorker();
bool Cr3TrackerSynchronizeExitPolicy(__vcpu* vcpu);
void Cr3TrackerRememberTarget(__vcpu* vcpu, unsigned __int64 candidateCr3);
void Cr3TrackerReset();
void Cr3TrackerDumpTelemetry();
void Cr3TrackerQueryTelemetry(CR3_TRACKER_TELEMETRY* telemetry);

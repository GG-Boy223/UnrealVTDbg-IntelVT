#include "Cr3Tracker.h"

#include "../Driver.h"
#include "../Globals.h"

namespace
{
    // 已移除后台系统线程：Cr3TrackerObserve 现改在 VM-exit 命中时（VMX root）同步执行
    // 保留 PublishCandidate/RememberTarget（在 mov_to_cr3_handler 里廉价记账，无物理读）

    bool TryConsumeCandidate(
        __vcpu* vcpu,
        unsigned __int64* candidateCr3)
    {
        if (vcpu == nullptr || candidateCr3 == nullptr)
        {
            return false;
        }

        const LONG64 sequence = InterlockedCompareExchange64(
            &vcpu->cr3_candidate.sequence,
            0,
            0);
        if (sequence == vcpu->cr3_candidate.consumedSequence)
        {
            return false;
        }

        const unsigned __int64 candidate = vcpu->cr3_candidate.value;
        KeMemoryBarrier();
        if (sequence != InterlockedCompareExchange64(
            &vcpu->cr3_candidate.sequence,
            0,
            0))
        {
            return false;
        }

        vcpu->cr3_candidate.consumedSequence = sequence;
        *candidateCr3 = candidate;
        return true;
    }

    // 已废弃：不再创建系统线程
    VOID Cr3TrackerWorker(_In_ PVOID startContext)
    {
        UNREFERENCED_PARAMETER(startContext);
        PsTerminateSystemThread(STATUS_UNSUCCESSFUL);
    }
}

void Cr3TrackerPublishCandidate(
    __vcpu* vcpu,
    unsigned __int64 candidateCr3)
{
    if (vcpu == nullptr)
    {
        return;
    }

    vcpu->cr3_candidate.value = candidateCr3;
    KeMemoryBarrier();
    InterlockedIncrement64(&vcpu->cr3_candidate.sequence);
}

bool Cr3TrackerStartWorker()
{
    // 不再创建后台线程：观察逻辑已迁移至 VM-exit 命中处
    return true;
}

void Cr3TrackerStopWorker()
{
    // 无线程需停止
}

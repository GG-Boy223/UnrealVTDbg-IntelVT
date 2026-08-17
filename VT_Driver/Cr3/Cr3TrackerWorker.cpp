#include "Cr3Tracker.h"

#include "../Driver.h"
#include "../Globals.h"

namespace
{
    HANDLE g_Cr3WorkerHandle = nullptr;
    KEVENT g_Cr3WorkerStopEvent = { 0 };
    volatile LONG g_Cr3WorkerStopRequested = 0;

    bool TryConsumeCandidate(
        __vcpu* vcpu,
        unsigned __int64* candidateCr3)
    {
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

    VOID Cr3TrackerWorker(_In_ PVOID startContext)
    {
        UNREFERENCED_PARAMETER(startContext);

        LARGE_INTEGER interval = { 0 };
        interval.QuadPart = -500000;
        while (InterlockedCompareExchange(
            &g_Cr3WorkerStopRequested,
            0,
            0) == 0)
        {
            for (unsigned int index = 0;
                index < g_vmm_context.processor_count;
                ++index)
            {
                unsigned __int64 candidateCr3 = 0;
                if (TryConsumeCandidate(
                    &g_vmm_context.vcpu[index],
                    &candidateCr3))
                {
                    Cr3TrackerObserve(candidateCr3);
                }
            }

            KeWaitForSingleObject(
                &g_Cr3WorkerStopEvent,
                Executive,
                KernelMode,
                FALSE,
                &interval);
        }

        PsTerminateSystemThread(STATUS_SUCCESS);
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
    if (g_Cr3WorkerHandle != nullptr)
    {
        return true;
    }

    KeInitializeEvent(&g_Cr3WorkerStopEvent, NotificationEvent, FALSE);
    InterlockedExchange(&g_Cr3WorkerStopRequested, 0);
    const NTSTATUS status = PsCreateSystemThread(
        &g_Cr3WorkerHandle,
        THREAD_ALL_ACCESS,
        nullptr,
        nullptr,
        nullptr,
        Cr3TrackerWorker,
        nullptr);
    if (!NT_SUCCESS(status))
    {
        g_Cr3WorkerHandle = nullptr;
        return false;
    }

    return true;
}

void Cr3TrackerStopWorker()
{
    if (g_Cr3WorkerHandle == nullptr)
    {
        return;
    }

    InterlockedExchange(&g_Cr3WorkerStopRequested, 1);
    KeSetEvent(&g_Cr3WorkerStopEvent, IO_NO_INCREMENT, FALSE);
    ZwWaitForSingleObject(g_Cr3WorkerHandle, FALSE, nullptr);
    ZwClose(g_Cr3WorkerHandle);
    g_Cr3WorkerHandle = nullptr;
}

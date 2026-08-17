#include "VmExitTelemetry.h"

#include "../Driver.h"
#include "../Globals.h"

void VmExitTelemetryDump()
{
    if (g_vmm_context.vcpu == nullptr)
    {
        return;
    }

    for (unsigned int cpu = 0; cpu < g_vmm_context.processor_count; ++cpu)
    {
        const auto& telemetry = g_vmm_context.vcpu[cpu].vmexit_telemetry;
        for (unsigned int reason = 0;
            reason < kVmExitTelemetryReasonCapacity;
            ++reason)
        {
            if (telemetry.reasonCount[reason] == 0)
            {
                continue;
            }

            outFile(
                "[VMEXIT统计] CPU[%u] Reason=%u Count=%llu Cycles=%llu",
                cpu,
                reason,
                telemetry.reasonCount[reason],
                telemetry.reasonCycles[reason]);
        }

        if (telemetry.overflowCount != 0)
        {
            outFile(
                "[VMEXIT统计] CPU[%u] ReasonOverflow Count=%llu Cycles=%llu",
                cpu,
                telemetry.overflowCount,
                telemetry.overflowCycles);
        }
    }
}

#include "Cr3Tracker.h"
#include "Cr3TrackerInternal.h"

#include "../Driver.h"
#include "../Globals.h"
#include "../hypervisor_routines.h"
#include "../Vmcs/VmcsFields.h"
#include "../../Common/Ring0/Hvm/Cr3ExitPolicy.h"

namespace
{
    void WriteCr3Target(unsigned __int32 slot, unsigned __int64 value)
    {
        switch (slot)
        {
        case 0:
            hv::vmwrite<unsigned __int64>(CR3_TARGET_VALUE0, value);
            break;
        case 1:
            hv::vmwrite<unsigned __int64>(CR3_TARGET_VALUE1, value);
            break;
        case 2:
            hv::vmwrite<unsigned __int64>(CR3_TARGET_VALUE2, value);
            break;
        case 3:
            hv::vmwrite<unsigned __int64>(CR3_TARGET_VALUE3, value);
            break;
        default:
            break;
        }
    }

    void ResetCr3Targets(__vcpu* vcpu)
    {
        RtlZeroMemory(vcpu->cr3_target_values, sizeof(vcpu->cr3_target_values));
        vcpu->cr3_target_count = 0;
        vcpu->cr3_target_cursor = 0;
        hv::vmwrite<unsigned __int64>(CR3_TARGET_COUNT, 0);
        WriteCr3Target(0, 0);
        WriteCr3Target(1, 0);
        WriteCr3Target(2, 0);
        WriteCr3Target(3, 0);
    }
}

bool Cr3TrackerSynchronizeExitPolicy(__vcpu* vcpu)
{
    using namespace cr3_tracker_internal;

    if (vcpu == nullptr)
    {
        return false;
    }

    const bool observationRequired =
        InterlockedCompareExchange(&g_ActiveTrackerCount, 0, 0) != 0;
    const auto currentControls = static_cast<std::uint32_t>(
        hv::vmread(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL));
    const auto desiredControls = hv::cr3_exit_policy::Apply(
        currentControls,
        observationRequired);

    if (desiredControls != currentControls)
    {
        hv::vmwrite<unsigned __int64>(
            PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL,
            desiredControls);
        InterlockedIncrement64(observationRequired ?
            &g_PolicyEnableCount :
            &g_PolicyDisableCount);
    }

    ResetCr3Targets(vcpu);
    if (observationRequired)
    {
        Cr3TrackerRememberTarget(vcpu, hv::vmread(GUEST_CR3));
    }
    return true;
}

void Cr3TrackerRememberTarget(__vcpu* vcpu, unsigned __int64 candidateCr3)
{
    using namespace cr3_tracker_internal;

    if (vcpu == nullptr)
    {
        return;
    }

    const unsigned __int64 root = NormalizeCr3(candidateCr3);
    if (root == 0)
    {
        return;
    }

    for (unsigned __int32 index = 0; index < vcpu->cr3_target_count; ++index)
    {
        if (vcpu->cr3_target_values[index] == root)
        {
            return;
        }
    }

    const unsigned __int32 slot = hv::cr3_exit_policy::SelectTargetSlot(
        vcpu->cr3_target_count,
        vcpu->cr3_target_cursor);
    vcpu->cr3_target_values[slot] = root;
    WriteCr3Target(slot, root);
    vcpu->cr3_target_cursor = hv::cr3_exit_policy::AdvanceTargetCursor(
        vcpu->cr3_target_count,
        slot);
    if (vcpu->cr3_target_count < hv::cr3_exit_policy::kTargetCapacity)
    {
        ++vcpu->cr3_target_count;
        hv::vmwrite<unsigned __int64>(CR3_TARGET_COUNT, vcpu->cr3_target_count);
    }
    InterlockedIncrement64(&g_TargetUpdateCount);
}

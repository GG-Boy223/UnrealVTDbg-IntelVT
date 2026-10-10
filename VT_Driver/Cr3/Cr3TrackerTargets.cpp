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

    // Intel SDM Vol3C 27.6.7：CR3-target 的硬件比较对象是 MOV to CR3 源操作数的
    // 完整 64 位值（含 PCID 与 bit63 no-flush），"There are no limitations on the values
    // that can be written for the CR3-target values"。因此写入 VMCS 的必须是来访最近
    // 一次实际使用的完整 CR3 值；只能把 NormalizeCr3 的结果用于页表遍历与地址空间去重。
    // 旧逻辑把去掉低 12 位的 root 写入 CR3_TARGET_VALUE：
    //   - guest CR4.PCIDE=1 时 CR3 低 12 位是 PCID，完整值与 root 永不相等，
    //     于是每次 MOV to CR3（Windows 每次进程切换）都强制 VM-exit，
    //     CR3-target 优化完全失效，并在热路径反复做 PML4 指纹物理读。
    for (unsigned __int32 index = 0; index < vcpu->cr3_target_count; ++index)
    {
        if (NormalizeCr3(vcpu->cr3_target_values[index]) == root)
        {
            if (vcpu->cr3_target_values[index] != candidateCr3)
            {
                vcpu->cr3_target_values[index] = candidateCr3;
                WriteCr3Target(index, candidateCr3);
            }
            return;
        }
    }

    const unsigned __int32 slot = hv::cr3_exit_policy::SelectTargetSlot(
        vcpu->cr3_target_count,
        vcpu->cr3_target_cursor);
    vcpu->cr3_target_values[slot] = candidateCr3;
    WriteCr3Target(slot, candidateCr3);
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

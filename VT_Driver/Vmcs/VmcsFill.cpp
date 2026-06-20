#include "VmcsInternal.h"

void fill_vmcs(__vcpu* vcpu, void* guest_rsp)
{
    __exception_bitmap exception_bitmap = { 0 };
    __vmx_basic_msr vmx_basic = { 0 };
    __vmx_entry_control entry_controls = { 0 };
    __vmx_exit_control exit_controls = { 0 };
    __vmx_pinbased_control_msr pinbased_controls = { 0 };
    __vmx_primary_processor_based_control primary_controls = { 0 };
    __vmx_secondary_processor_based_control secondary_controls = { 0 };

    vmx_basic.all = __readmsr(IA32_VMX_BASIC);

    set_entry_control(entry_controls);
    set_exit_control(exit_controls);
    set_primary_controls(primary_controls);
    set_secondary_controls(secondary_controls);
    set_pinbased_control_msr(pinbased_controls);
    set_exception_bitmap(exception_bitmap);

#ifndef _MINIMAL
    memset(vcpu->vcpu_bitmaps.msr_bitmap, 0xff, PAGE_SIZE);
#endif

    vcpu->vmcs_effective_controls.pinbased = ajdust_controls(pinbased_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_PINBASED_CTLS : IA32_VMX_PINBASED_CTLS);
    vcpu->vmcs_effective_controls.primary = ajdust_controls(primary_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_PROCBASED_CTLS : IA32_VMX_PROCBASED_CTLS);
    vcpu->vmcs_effective_controls.secondary = ajdust_controls(secondary_controls.all, IA32_VMX_PROCBASED_CTLS2);
    vcpu->vmcs_effective_controls.exit = ajdust_controls(exit_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_EXIT_CTLS : IA32_VMX_EXIT_CTLS);
    vcpu->vmcs_effective_controls.entry = ajdust_controls(entry_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS);

    __vmx_primary_processor_based_control effective_primary = { 0 };
    __vmx_secondary_processor_based_control effective_secondary = { 0 };
    effective_primary.all = vcpu->vmcs_effective_controls.primary;
    effective_secondary.all = vcpu->vmcs_effective_controls.secondary;
    unsigned __int64 ept_vpid_cap = 0;
    if (effective_secondary.enable_ept || effective_secondary.enable_vpid)
    {
        ept_vpid_cap = __readmsr(IA32_VMX_EPT_VPID_CAP);
    }

    LogInfo("VMCS effective controls: pin=0x%llX primary=0x%llX secondary=0x%llX exit=0x%llX entry=0x%llX EPT_VPID_CAP=0x%llX",
        vcpu->vmcs_effective_controls.pinbased,
        vcpu->vmcs_effective_controls.primary,
        vcpu->vmcs_effective_controls.secondary,
        vcpu->vmcs_effective_controls.exit,
        vcpu->vmcs_effective_controls.entry,
        ept_vpid_cap);
    outFile("[信息][VMCS控制位] 硬件调整后的有效控制位：PIN=0x%llX PRIMARY=0x%llX SECONDARY=0x%llX EXIT=0x%llX ENTRY=0x%llX EPT_VPID_CAP=0x%llX",
        vcpu->vmcs_effective_controls.pinbased,
        vcpu->vmcs_effective_controls.primary,
        vcpu->vmcs_effective_controls.secondary,
        vcpu->vmcs_effective_controls.exit,
        vcpu->vmcs_effective_controls.entry,
        ept_vpid_cap);

    hv::set_msr_bitmap(vcpu->msr_bitmap, IA32_FEATURE_CONTROL, true);
    hv::vmwrite<unsigned __int64>(PIN_BASED_VM_EXEC_CONTROL, vcpu->vmcs_effective_controls.pinbased);
    hv::vmwrite<unsigned __int64>(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL, vcpu->vmcs_effective_controls.primary);
    hv::vmwrite<unsigned __int64>(SECONDARY_PROCESSOR_BASED_VM_EXEC_CONTROL, vcpu->vmcs_effective_controls.secondary);
    hv::vmwrite<unsigned __int64>(VM_EXIT_CONTROLS, vcpu->vmcs_effective_controls.exit);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_CONTROLS, vcpu->vmcs_effective_controls.entry);
    hv::vmwrite<unsigned __int64>(VMCS_LINK_POINTER, MAXULONG64);
    hv::vmwrite(EXCEPTION_BITMAP, exception_bitmap.all);
    hv::vmwrite<unsigned int>(PAGE_FAULT_ERROR_CODE_MASK, 0);
    hv::vmwrite<unsigned int>(PAGE_FAULT_ERROR_CODE_MATCH, 0);

    if (effective_primary.use_msr_bitmaps == true)
    {
        hv::vmwrite(MSR_BITMAP_ADDRESS, MmGetPhysicalAddress(&vcpu->msr_bitmap).QuadPart);
        outFile("[成功][VMCS控制位] 已写入MSR Bitmap：PA=0x%llX", MmGetPhysicalAddress(&vcpu->msr_bitmap).QuadPart);
    }

    if (effective_primary.use_io_bitmaps == true)
    {
        hv::vmwrite(IO_BITMAP_A_ADDRESS, vcpu->vcpu_bitmaps.io_bitmap_a_physical);
        hv::vmwrite(IO_BITMAP_B_ADDRESS, vcpu->vcpu_bitmaps.io_bitmap_b_physical);
        outFile("[成功][VMCS控制位] 已写入IO Bitmap：A_PA=0x%llX B_PA=0x%llX",
            vcpu->vcpu_bitmaps.io_bitmap_a_physical,
            vcpu->vcpu_bitmaps.io_bitmap_b_physical);
    }

    if (effective_secondary.enable_vpid == true)
    {
        hv::vmwrite(VIRTUAL_PROCESSOR_ID, guest_vpid);
        outFile("[成功][VMCS控制位] 已写入VPID：Value=%u", guest_vpid);
    }
    else
    {
        outFile("[信息][VMCS控制位] 有效控制位未启用VPID，跳过VPID写入");
    }

    if (effective_secondary.enable_ept == true)
    {
        hv::vmwrite(EPT_POINTER, vcpu->ept_state->ept_pointer->all);
        LogInfo("VMCS EPTP=0x%llX VPID=%s", vcpu->ept_state->ept_pointer->all, effective_secondary.enable_vpid ? "enabled" : "disabled");
        outFile("[成功][VMCS控制位] 已写入EPTP：EPTP=0x%llX VPID=%s",
            vcpu->ept_state->ept_pointer->all,
            effective_secondary.enable_vpid ? "已启用" : "未启用");
    }
    else
    {
        outFile("[信息][VMCS控制位] 有效控制位未启用EPT，跳过EPTP写入");
    }

    hv::vmwrite<unsigned __int64>(VM_EXIT_MSR_LOAD_COUNT, 0);
    hv::vmwrite<unsigned __int64>(VM_EXIT_MSR_LOAD_ADDR, 0);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_INTERRUPTION_INFO_FIELD, 0);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_EXCEPTION_ERROR_CODE, 0);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_INSTRUCTION_LENGTH, 0);

    fill_vmcs_guest_fields(vcpu, guest_rsp);
    fill_vmcs_host_fields(vcpu);
}

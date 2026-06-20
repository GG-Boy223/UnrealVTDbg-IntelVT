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

    hv::set_msr_bitmap(vcpu->msr_bitmap, IA32_FEATURE_CONTROL, true);
    hv::vmwrite<unsigned __int64>(PIN_BASED_VM_EXEC_CONTROL, ajdust_controls(pinbased_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_PINBASED_CTLS : IA32_VMX_PINBASED_CTLS));
    hv::vmwrite<unsigned __int64>(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL, ajdust_controls(primary_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_PROCBASED_CTLS : IA32_VMX_PROCBASED_CTLS));
    hv::vmwrite<unsigned __int64>(SECONDARY_PROCESSOR_BASED_VM_EXEC_CONTROL, ajdust_controls(secondary_controls.all, IA32_VMX_PROCBASED_CTLS2));
    hv::vmwrite<unsigned __int64>(VM_EXIT_CONTROLS, ajdust_controls(exit_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_EXIT_CTLS : IA32_VMX_EXIT_CTLS));
    hv::vmwrite<unsigned __int64>(VM_ENTRY_CONTROLS, ajdust_controls(entry_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS));
    hv::vmwrite<unsigned __int64>(VMCS_LINK_POINTER, MAXULONG64);
    hv::vmwrite(EXCEPTION_BITMAP, exception_bitmap.all);
    hv::vmwrite<unsigned int>(PAGE_FAULT_ERROR_CODE_MASK, 0);
    hv::vmwrite<unsigned int>(PAGE_FAULT_ERROR_CODE_MATCH, 0);

    if (primary_controls.use_msr_bitmaps == true)
    {
        hv::vmwrite(MSR_BITMAP_ADDRESS, MmGetPhysicalAddress(&vcpu->msr_bitmap).QuadPart);
    }

    if (primary_controls.use_io_bitmaps == true)
    {
        hv::vmwrite(IO_BITMAP_A_ADDRESS, vcpu->vcpu_bitmaps.io_bitmap_a_physical);
        hv::vmwrite(IO_BITMAP_B_ADDRESS, vcpu->vcpu_bitmaps.io_bitmap_b_physical);
    }

    if (secondary_controls.enable_vpid == true)
    {
        hv::vmwrite(VIRTUAL_PROCESSOR_ID, guest_vpid);
    }

    if (secondary_controls.enable_ept == true && secondary_controls.enable_vpid == true)
    {
        hv::vmwrite(EPT_POINTER, vcpu->ept_state->ept_pointer->all);
    }

    hv::vmwrite<unsigned __int64>(VM_EXIT_MSR_LOAD_COUNT, 0);
    hv::vmwrite<unsigned __int64>(VM_EXIT_MSR_LOAD_ADDR, 0);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_INTERRUPTION_INFO_FIELD, 0);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_EXCEPTION_ERROR_CODE, 0);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_INSTRUCTION_LENGTH, 0);

    fill_vmcs_guest_fields(vcpu, guest_rsp);
    fill_vmcs_host_fields(vcpu);
}

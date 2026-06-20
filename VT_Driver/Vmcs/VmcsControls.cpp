#include "VmcsInternal.h"

void set_entry_control(__vmx_entry_control& entry_control)
{
    entry_control.load_dbg_controls = true;
    entry_control.ia32e_mode_guest = true;
    entry_control.entry_to_smm = false;
    entry_control.deactivate_dual_monitor_treament = false;
    entry_control.load_ia32_perf_global_control = true;
    entry_control.load_ia32_pat = true;
    entry_control.load_ia32_efer = false;
    entry_control.load_ia32_bndcfgs = false;
    entry_control.conceal_vmx_from_pt = true;
    entry_control.load_ia32_rtit_ctl = false;
    entry_control.load_cet_state = false;
    entry_control.load_pkrs = false;
}

void set_exit_control(__vmx_exit_control& exit_control)
{
    exit_control.save_dbg_controls = true;
    exit_control.host_address_space_size = true;
    exit_control.load_ia32_perf_global_control = true;
    exit_control.acknowledge_interrupt_on_exit = true;
    exit_control.save_ia32_pat = true;
    exit_control.load_ia32_pat = true;
    exit_control.save_ia32_efer = false;
    exit_control.load_ia32_efer = false;
    exit_control.save_vmx_preemption_timer_value = false;
    exit_control.clear_ia32_bndcfgs = false;
    exit_control.conceal_vmx_from_pt = true;
    exit_control.clear_ia32_rtit_ctl = false;
    exit_control.load_cet_state = false;
    exit_control.load_pkrs = false;
}

void set_primary_controls(__vmx_primary_processor_based_control& primary_controls)
{
    primary_controls.interrupt_window_exiting = false;
    primary_controls.use_tsc_offsetting = false;
    primary_controls.hlt_exiting = false;

#ifdef _MINIMAL
    primary_controls.invlpg_exiting = false;
#else
    primary_controls.invlpg_exiting = true;
#endif

    primary_controls.mwait_exiting = false;
    primary_controls.rdpmc_exiting = false;

#ifdef _MINIMAL
    primary_controls.rdtsc_exiting = false;
#else
    primary_controls.rdtsc_exiting = true;
#endif

#ifdef _MINIMAL
    primary_controls.cr3_load_exiting = true;
#else
    primary_controls.cr3_load_exiting = true;
#endif

#ifdef _MINIMAL
    primary_controls.cr3_store_exiting = false;
#else
    primary_controls.cr3_store_exiting = true;
#endif

    primary_controls.cr8_load_exiting = false;
    primary_controls.cr8_store_exiting = false;
    primary_controls.use_tpr_shadow = false;
    primary_controls.nmi_window_exiting = false;

#ifdef _MINIMAL
    primary_controls.mov_dr_exiting = false;
#else
    primary_controls.mov_dr_exiting = true;
#endif

    primary_controls.unconditional_io_exiting = false;

#ifdef _MINIMAL
    primary_controls.use_io_bitmaps = false;
#else
    primary_controls.use_io_bitmaps = true;
#endif

    primary_controls.monitor_trap_flag = false;
    primary_controls.use_msr_bitmaps = true;
    primary_controls.monitor_exiting = false;
    primary_controls.pause_exiting = false;
    primary_controls.active_secondary_controls = true;
}

void set_secondary_controls(__vmx_secondary_processor_based_control& secondary_controls)
{
    secondary_controls.virtualize_apic_accesses = false;
    secondary_controls.enable_ept = true;

#ifdef _MINIMAL
    secondary_controls.descriptor_table_exiting = false;
#else
    secondary_controls.descriptor_table_exiting = true;
#endif

    secondary_controls.enable_rdtscp = true;
    secondary_controls.virtualize_x2apic_mode = false;
    secondary_controls.enable_vpid = true;

#ifdef _MINIMAL
    secondary_controls.wbinvd_exiting = false;
#else
    secondary_controls.wbinvd_exiting = true;
#endif

    secondary_controls.unrestricted_guest = false;
    secondary_controls.apic_register_virtualization = false;
    secondary_controls.virtual_interrupt_delivery = false;
    secondary_controls.pause_loop_exiting = false;

#ifdef _MINIMAL
    secondary_controls.rdrand_exiting = false;
#else
    secondary_controls.rdrand_exiting = true;
#endif

    secondary_controls.enable_invpcid = true;
    secondary_controls.enable_vmfunc = false;
    secondary_controls.vmcs_shadowing = false;
    secondary_controls.enable_encls_exiting = false;

#ifdef _MINIMAL
    secondary_controls.rdseed_exiting = false;
#else
    secondary_controls.rdseed_exiting = true;
#endif

    secondary_controls.enable_pml = false;
    secondary_controls.ept_violation = false;
    secondary_controls.conceal_vmx_from_pt = true;
    secondary_controls.enable_xsaves_xrstors = true;
    secondary_controls.mode_based_execute_control_ept = false;
    secondary_controls.sub_page_write_permission_for_ept = false;
    secondary_controls.intel_pt_uses_guest_physical_address = false;
    secondary_controls.use_tsc_scaling = false;
    secondary_controls.enable_user_wait_and_pause = true;
    secondary_controls.enable_enclv_exiting = false;
}

void set_exception_bitmap(__exception_bitmap& exception_bitmap)
{
    exception_bitmap.divide_error = false;
    exception_bitmap.debug = false;
    exception_bitmap.nmi_interrupt = false;
    exception_bitmap.breakpoint = false;
    exception_bitmap.overflow = false;
    exception_bitmap.bound = false;
    exception_bitmap.invalid_opcode = false;
    exception_bitmap.coprocessor_segment_overrun = false;
    exception_bitmap.invalid_tss = false;
    exception_bitmap.segment_not_present = false;
    exception_bitmap.stack_segment_fault = false;
    exception_bitmap.general_protection = false;
    exception_bitmap.page_fault = false;
    exception_bitmap.floating_point_error = false;
    exception_bitmap.alignment_check = false;
    exception_bitmap.machine_check = false;
    exception_bitmap.simd_floating_point_exception = false;
    exception_bitmap.virtualization_exception = false;
    exception_bitmap.control_protection_exception = false;
}

void set_pinbased_control_msr(__vmx_pinbased_control_msr& pinbased_controls)
{
    pinbased_controls.external_interrupt_exiting = false;
    pinbased_controls.nmi_exiting = true;
    pinbased_controls.virtual_nmis = true;
    pinbased_controls.activate_vmxpreemption_timer = false;
    pinbased_controls.process_posted_interrupts = false;
}

unsigned __int64 ajdust_controls(unsigned __int64 ctl, unsigned long msr)
{
    __msr msr_value = { 0 };
    msr_value.all = __readmsr(msr);
    ctl &= msr_value.high;
    ctl |= msr_value.low;
    return ctl;
}

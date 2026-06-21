#include "VmExitCommon.h"

//恢复guest状态
void RestoreGuest()
{
	hv::vmwrite(CR0_READ_SHADOW, hv::read_effective_guest_cr0().flags);
	hv::vmwrite(CR4_READ_SHADOW, hv::read_effective_guest_cr4().flags);

	__writedr(7, hv::vmread(GUEST_DR7));
	__writecr3(hv::vmread(GUEST_CR3));

	__writemsr(IA32_SYSENTER_CS, hv::vmread(GUEST_SYSENTER_CS));
	__writemsr(IA32_SYSENTER_ESP, hv::vmread(GUEST_SYSENTER_ESP));
	__writemsr(IA32_SYSENTER_EIP, hv::vmread(GUEST_SYSENTER_EIP));
	__writemsr(IA32_PAT, hv::vmread(GUEST_PAT));
	__writemsr(IA32_DEBUGCTL, hv::vmread(GUEST_DEBUG_CONTROL));

	__reload_gdtr(hv::vmread(GUEST_GDTR_BASE), hv::vmread(GUEST_GDTR_LIMIT));
	__reload_idtr(hv::vmread(GUEST_IDTR_BASE), hv::vmread(GUEST_IDTR_LIMIT));

	segment_selector guest_tr;
	guest_tr.flags = static_cast<uint16_t>(hv::vmread(GUEST_TR_SELECTOR));

	__pseudo_descriptor64 gdtr = { 0 };
	__sgdt(&gdtr);
	(reinterpret_cast<segment_descriptor_32*>(gdtr.base_address) + guest_tr.index)->type = SEGMENT_DESCRIPTOR_TYPE_TSS_AVAILABLE;
	write_tr(guest_tr.flags);

	write_ds(static_cast<uint16_t>(hv::vmread(GUEST_DS_SELECTOR)));
	write_es(static_cast<uint16_t>(hv::vmread(GUEST_ES_SELECTOR)));
	write_fs(static_cast<uint16_t>(hv::vmread(GUEST_FS_SELECTOR)));
	write_gs(static_cast<uint16_t>(hv::vmread(GUEST_GS_SELECTOR)));
	write_ldtr(static_cast<uint16_t>(hv::vmread(GUEST_LDTR_SELECTOR)));

	_writefsbase_u64(hv::vmread(GUEST_FS_BASE));
	_writegsbase_u64(hv::vmread(GUEST_GS_BASE));
}

void set_hide_vm_exit_overhead(__vcpu* vcpu, bool value)
{
	vcpu->hide_vm_exit_overhead = value;
}

void hide_vm_exit_overhead(__vcpu* vcpu)
{
	ia32_perf_global_ctrl_register perf_global_ctrl;
	perf_global_ctrl.flags = vcpu->msr_exit_store.perf_global_ctrl.msr_data;

	vcpu->msr_entry_load.aperf.msr_data = vcpu->msr_exit_store.aperf.msr_data;
	vcpu->msr_entry_load.mperf.msr_data = vcpu->msr_exit_store.mperf.msr_data;
	hv::vmwrite(VMCS_GUEST_PERF_GLOBAL_CTRL, perf_global_ctrl.flags);

	vcpu->msr_entry_load.aperf.msr_data -= vcpu->vm_exit_mperf_overhead;
	vcpu->msr_entry_load.mperf.msr_data -= vcpu->vm_exit_mperf_overhead;

	if (perf_global_ctrl.en_fixed_ctrn & (1ull << 2))
	{
		auto const cpl = hv::current_guest_cpl();

		ia32_fixed_ctr_ctrl_register fixed_ctr_ctrl;
		fixed_ctr_ctrl.flags = __readmsr(IA32_FIXED_CTR_CTRL);

		if ((cpl == 0 && fixed_ctr_ctrl.en2_os) || (cpl == 3 && fixed_ctr_ctrl.en2_usr))
			__writemsr(IA32_FIXED_CTR2, __readmsr(IA32_FIXED_CTR2) - vcpu->vm_exit_ref_tsc_overhead);
	}

	if (!vcpu->hide_vm_exit_overhead || vcpu->vm_exit_tsc_overhead > 10000)
	{
		vcpu->tsc_offset = 0;
		vcpu->preemption_timer = ~0ull;
		return;
	}

	ia32_vmx_misc_register vmx_misc;
	vmx_misc.flags = __readmsr(IA32_VMX_MISC);
	vcpu->preemption_timer = max(2, 10000 >> vmx_misc.preemption_timer_tsc_relationship);

	vcpu->tsc_offset -= vcpu->vm_exit_tsc_overhead;
}

void skip_instruction()
{
	auto const old_rip = hv::vmread(GUEST_RIP);
	auto new_rip = old_rip + hv::vmread(VM_EXIT_INSTRUCTION_LENGTH);

	if (old_rip < (1ull << 32) && new_rip >= (1ull << 32))
	{
		vmx_segment_access_rights cs_access_rights;
		cs_access_rights.flags = static_cast<uint32_t>(hv::vmread(GUEST_CS_ACCESS_RIGHTS));

		if (!cs_access_rights.long_mode)
			new_rip &= 0xFFFF'FFFF;
	}

	hv::vmwrite(GUEST_RIP, new_rip);

	auto interrupt_state = hv::read_interruptibility_state();
	interrupt_state.blocking_by_mov_ss = 0;
	interrupt_state.blocking_by_sti = 0;
	hv::write_interruptibility_state(interrupt_state);

	ia32_debugctl_register debugctl;
	debugctl.flags = hv::vmread(GUEST_DEBUG_CONTROL);

	rflags rflags;
	rflags.flags = hv::vmread(GUEST_RFLAGS);

	if (rflags.trap_flag && !debugctl.btf)
	{
		vmx_pending_debug_exceptions dbg_exception;
		dbg_exception.flags = hv::vmread(GUEST_PENDING_DEBUG_EXCEPTION);
		dbg_exception.bs = 1;
		hv::vmwrite(GUEST_PENDING_DEBUG_EXCEPTION, dbg_exception.flags);
	}
}

//void adjust_rip(__vcpu* vcpu) // 根据已解析的指令长度推进来宾 RIP，并处理单步相关状态
//{
//	//skip_instruction(); // 更完整的跳过逻辑（含中断屏蔽/单步注入），此处未启用
//	hv::vmwrite(GUEST_RIP, vcpu->vmexit_info.guest_rip + vcpu->vmexit_info.instruction_length); // 直接把 RIP 前移到下一条指令
//	if (vcpu->vmexit_info.guest_rflags.trap_flag)  // 判断 guest 是否开启单步（TF=1）
//	{
//		__vmx_pending_debug_exceptions pending_debug = { hv::vmread(GUEST_PENDING_DEBUG_EXCEPTION) }; // 读取 VMCS 中待注入调试异常状态
//		__vmx_interruptibility_state interruptibility = { hv::vmread(GUEST_INTERRUPTIBILITY_STATE) }; // 读取来宾中断阻塞状态
//
//		pending_debug.bs = true;  // 置位 BS，要求 VM-entry 后注入 #DB 以模拟单步
//		hv::vmwrite(GUEST_PENDING_DEBUG_EXCEPTION, pending_debug.all); // 写回待注入调试异常字段
//
//		//如果 guest 里开启单步，则必须取消因 STI/MOV SS 导致的阻塞，否则 #DB 可能被延迟
//		interruptibility.blocking_by_sti = false; // 清除 STI 引起的中断阻塞
//		interruptibility.blocking_by_mov_ss = false; // 清除 MOV SS 引起的中断阻塞
//		hv::vmwrite(GUEST_INTERRUPTIBILITY_STATE, interruptibility.all); // 写回中断阻塞状态，确保 #DB 及时注入
//	}
//}

void adjust_rip(__vcpu* vcpu)
{
	UNREFERENCED_PARAMETER(vcpu);
	skip_instruction();
}

void CleanUp(__vcpu* vcpu, __ept_hooked_page_info* hooked_page_info)
{
	hooked_page_info->ID = -1;
	hooked_page_info->isBp = false;
	hooked_page_info->isInt3 = false;
	hv::clear_pending_mtf_metadata(vcpu);
}

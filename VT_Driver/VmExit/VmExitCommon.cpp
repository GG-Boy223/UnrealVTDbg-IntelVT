#include "VmExitCommon.h"

namespace
{
	constexpr ULONG kHypervisorErrorBugCheck = 0x00020001;

	struct guest_restore_state
	{
		unsigned __int64 cr0_mask;
		unsigned __int64 cr0_shadow;
		unsigned __int64 guest_cr0;
		unsigned __int64 cr4_mask;
		unsigned __int64 cr4_shadow;
		unsigned __int64 guest_cr4;
		unsigned __int64 guest_cr3;
		unsigned __int64 guest_dr7;
		unsigned __int64 sysenter_cs;
		unsigned __int64 sysenter_esp;
		unsigned __int64 sysenter_eip;
		unsigned __int64 pat;
		unsigned __int64 debug_control;
		unsigned __int64 perf_global_control;
		unsigned __int64 gdtr_base;
		unsigned __int64 gdtr_limit;
		unsigned __int64 idtr_base;
		unsigned __int64 idtr_limit;
		unsigned __int64 tr_selector;
		unsigned __int64 ds_selector;
		unsigned __int64 es_selector;
		unsigned __int64 fs_selector;
		unsigned __int64 gs_selector;
		unsigned __int64 ldtr_selector;
		unsigned __int64 fs_base;
		unsigned __int64 gs_base;
		unsigned __int64 guest_rip;
		unsigned __int64 guest_rsp;
		unsigned __int64 guest_rflags;
		unsigned __int64 guest_cs;
		unsigned __int64 guest_ss;
	};

	bool try_vmread(unsigned __int64 field, unsigned __int64* value)
	{
		return value != nullptr && __vmx_vmread(field, value) == 0;
	}

	bool try_vmwrite(unsigned __int64 field, unsigned __int64 value)
	{
		return __vmx_vmwrite(field, value) == 0;
	}

	bool should_capture_vmcs_for_fatal(unsigned __int64 reason)
	{
		switch (static_cast<vmx_fatal_reason>(reason))
		{
		case vmx_fatal_vmresume_restore:
		case vmx_fatal_vmxoff_instruction:
		case vmx_fatal_vm_entry_failure:
		case vmx_fatal_triple_fault:
		case vmx_fatal_missing_vcpu:
		case vmx_fatal_unhandled_ept_violation:
		case vmx_fatal_ept_misconfiguration:
		case vmx_fatal_vm_entry_event_queue_overflow:
			return true;
		default:
			return false;
		}
	}

	__vcpu* resolve_current_vcpu()
	{
		if (g_vmm_context.vcpu == nullptr || g_vmm_context.processor_count == 0)
		{
			return nullptr;
		}

		auto const begin = reinterpret_cast<unsigned __int64>(g_vmm_context.vcpu);
		auto const size = static_cast<unsigned __int64>(sizeof(__vcpu)) * g_vmm_context.processor_count;
		auto const candidate = _readfsbase_u64();
		if (candidate >= begin && candidate - begin < size &&
			(candidate - begin) % sizeof(__vcpu) == 0)
		{
			return reinterpret_cast<__vcpu*>(candidate);
		}

		PROCESSOR_NUMBER current = {};
		KeGetCurrentProcessorNumberEx(&current);
		for (unsigned int i = 0; i < g_vmm_context.processor_count; ++i)
		{
			auto& vcpu = g_vmm_context.vcpu[i];
			if (vcpu.processor_number.Group == current.Group &&
				vcpu.processor_number.Number == current.Number)
			{
				return &vcpu;
			}
		}

		return nullptr;
	}

	bool capture_guest_restore_state(guest_restore_state* state)
	{
		if (state == nullptr)
		{
			return false;
		}

		return
			try_vmread(CR0_GUEST_HOST_MASK, &state->cr0_mask) &&
			try_vmread(CR0_READ_SHADOW, &state->cr0_shadow) &&
			try_vmread(GUEST_CR0, &state->guest_cr0) &&
			try_vmread(CR4_GUEST_HOST_MASK, &state->cr4_mask) &&
			try_vmread(CR4_READ_SHADOW, &state->cr4_shadow) &&
			try_vmread(GUEST_CR4, &state->guest_cr4) &&
			try_vmread(GUEST_CR3, &state->guest_cr3) &&
			try_vmread(GUEST_DR7, &state->guest_dr7) &&
			try_vmread(GUEST_SYSENTER_CS, &state->sysenter_cs) &&
			try_vmread(GUEST_SYSENTER_ESP, &state->sysenter_esp) &&
			try_vmread(GUEST_SYSENTER_EIP, &state->sysenter_eip) &&
			try_vmread(GUEST_PAT, &state->pat) &&
			try_vmread(GUEST_DEBUG_CONTROL, &state->debug_control) &&
			try_vmread(GUEST_PERF_GLOBAL_CONTROL, &state->perf_global_control) &&
			try_vmread(GUEST_GDTR_BASE, &state->gdtr_base) &&
			try_vmread(GUEST_GDTR_LIMIT, &state->gdtr_limit) &&
			try_vmread(GUEST_IDTR_BASE, &state->idtr_base) &&
			try_vmread(GUEST_IDTR_LIMIT, &state->idtr_limit) &&
			try_vmread(GUEST_TR_SELECTOR, &state->tr_selector) &&
			try_vmread(GUEST_DS_SELECTOR, &state->ds_selector) &&
			try_vmread(GUEST_ES_SELECTOR, &state->es_selector) &&
			try_vmread(GUEST_FS_SELECTOR, &state->fs_selector) &&
			try_vmread(GUEST_GS_SELECTOR, &state->gs_selector) &&
			try_vmread(GUEST_LDTR_SELECTOR, &state->ldtr_selector) &&
			try_vmread(GUEST_FS_BASE, &state->fs_base) &&
			try_vmread(GUEST_GS_BASE, &state->gs_base) &&
			try_vmread(GUEST_RIP, &state->guest_rip) &&
			try_vmread(GUEST_RSP, &state->guest_rsp) &&
			try_vmread(GUEST_RFLAGS, &state->guest_rflags) &&
			try_vmread(GUEST_CS_SELECTOR, &state->guest_cs) &&
			try_vmread(GUEST_SS_SELECTOR, &state->guest_ss);
	}
}

EXTERN_C __declspec(noreturn)
void vmx_fatal_error(
	unsigned __int64 reason,
	unsigned __int64 detail1,
	unsigned __int64 detail2)
{
	__vcpu* const vcpu = resolve_current_vcpu();
	ULONG_PTR processor_index = static_cast<ULONG_PTR>(-1);
	if (vcpu != nullptr)
	{
		processor_index = vcpu->logical_processor_index;
		auto& snapshot = vcpu->fatal_vmx_snapshot;
		++snapshot.count;
		snapshot.reason = reason;
		snapshot.detail1 = detail1;
		snapshot.detail2 = detail2;
		snapshot.vm_instruction_error = 0;
		snapshot.vm_exit_reason = 0;
		snapshot.guest_rip = 0;
		snapshot.guest_rsp = 0;
		snapshot.guest_cr3 = 0;
		if (should_capture_vmcs_for_fatal(reason))
		{
			(void)try_vmread(VM_INSTRUCTION_ERROR, &snapshot.vm_instruction_error);
			(void)try_vmread(VM_EXIT_REASON, &snapshot.vm_exit_reason);
			(void)try_vmread(GUEST_RIP, &snapshot.guest_rip);
			(void)try_vmread(GUEST_RSP, &snapshot.guest_rsp);
			(void)try_vmread(GUEST_CR3, &snapshot.guest_cr3);
		}
	}

	KeBugCheckEx(
		kHypervisorErrorBugCheck,
		static_cast<ULONG_PTR>(reason),
		processor_index,
		static_cast<ULONG_PTR>(detail1),
		static_cast<ULONG_PTR>(detail2));
	__assume(0);
}

bool PrepareGuestForVmxoff(__vcpu* vcpu)
{
	if (vcpu == nullptr)
	{
		return false;
	}

	guest_restore_state state = {};
	if (!capture_guest_restore_state(&state))
	{
		return false;
	}

	segment_selector guest_tr = {};
	guest_tr.flags = static_cast<unsigned __int16>(state.tr_selector);
	auto const tr_descriptor_offset =
		static_cast<unsigned __int64>(guest_tr.index) * sizeof(segment_descriptor_32);
	if (state.gdtr_base == 0 ||
		tr_descriptor_offset + sizeof(segment_descriptor_32) > state.gdtr_limit + 1)
	{
		return false;
	}

	auto const effective_cr0 =
		(state.cr0_shadow & state.cr0_mask) | (state.guest_cr0 & ~state.cr0_mask);
	auto const effective_cr4 =
		(state.cr4_shadow & state.cr4_mask) | (state.guest_cr4 & ~state.cr4_mask);
	if (!try_vmwrite(CR0_READ_SHADOW, effective_cr0) ||
		!try_vmwrite(CR4_READ_SHADOW, effective_cr4))
	{
		return false;
	}

	vcpu->vmx_off_state.guest_rip = state.guest_rip;
	vcpu->vmx_off_state.guest_rsp = state.guest_rsp;
	vcpu->vmx_off_state.guest_rflags = state.guest_rflags;
	vcpu->vmx_off_state.guest_cs = state.guest_cs;
	vcpu->vmx_off_state.guest_ss = state.guest_ss;
	vcpu->vmx_off_state.guest_cr0 = effective_cr0;
	vcpu->vmx_off_state.guest_cr4 = effective_cr4;

	__writedr(7, state.guest_dr7);
	__writemsr(IA32_SYSENTER_CS, state.sysenter_cs);
	__writemsr(IA32_SYSENTER_ESP, state.sysenter_esp);
	__writemsr(IA32_SYSENTER_EIP, state.sysenter_eip);
	__writemsr(IA32_PAT, state.pat);
	__writemsr(IA32_DEBUGCTL, state.debug_control);
	__writemsr(IA32_PERF_GLOBAL_CTRL, state.perf_global_control);

	__reload_gdtr(state.gdtr_base, state.gdtr_limit);
	__reload_idtr(state.idtr_base, state.idtr_limit);
	(reinterpret_cast<segment_descriptor_32*>(state.gdtr_base) + guest_tr.index)->type =
		SEGMENT_DESCRIPTOR_TYPE_TSS_AVAILABLE;
	write_tr(guest_tr.flags);
	write_ds(static_cast<unsigned __int16>(state.ds_selector));
	write_es(static_cast<unsigned __int16>(state.es_selector));
	write_fs(static_cast<unsigned __int16>(state.fs_selector));
	write_gs(static_cast<unsigned __int16>(state.gs_selector));
	write_ldtr(static_cast<unsigned __int16>(state.ldtr_selector));

	_writefsbase_u64(state.fs_base);
	_writegsbase_u64(state.gs_base);
	__writecr3(state.guest_cr3);
	return true;
}

//恢复guest状态
void RestoreGuest()
{
	__vcpu* const vcpu = resolve_current_vcpu();
	if (vcpu == nullptr || !PrepareGuestForVmxoff(vcpu))
	{
		vmx_fatal_error(vmx_fatal_vmresume_restore, 0, 0);
	}
}

void set_hide_vm_exit_overhead(__vcpu* vcpu, bool value) // 设置“隐藏 VM-exit 开销”的开关状态，供后续逻辑判断是否要进行补偿
{
	vcpu->hide_vm_exit_overhead = value; // 将外部传入的布尔值写入 vCPU 状态，用于控制是否执行隐藏开销的策略
}

void hide_vm_exit_overhead(__vcpu* vcpu)
{
	ia32_perf_global_ctrl_register perf_global_ctrl; // 定义 PERF_GLOBAL_CTRL 的解析结构体，便于按位访问控制字段
	perf_global_ctrl.flags = vcpu->msr_exit_store.perf_global_ctrl.msr_data; // 读取 VM-exit 保存的来宾 PERF_GLOBAL_CTRL 值作为基准

	// 确保 VM-entry 时 CPU 加载“来宾上次的 APERF/MPERF 值”，保持计数连续。// 硬件原理：VM-entry 会根据 MSR-load 列表写入 MSR
	vcpu->msr_entry_load.aperf.msr_data = vcpu->msr_exit_store.aperf.msr_data; // 将来宾 APERF 恢复值放入 MSR-load 列表
	vcpu->msr_entry_load.mperf.msr_data = vcpu->msr_exit_store.mperf.msr_data; // 将来宾 MPERF 恢复值放入 MSR-load 列表
	hv::vmwrite(VMCS_GUEST_PERF_GLOBAL_CTRL, perf_global_ctrl.flags); // 写回 VMCS 来宾 PERF_GLOBAL_CTRL，保证 VM-entry 后控制位一致

	// 扣除“VM-exit 固定开销”对 APERF/MPERF 的影响，让来宾看到的增量更接近真实。// 原理：把固定误差提前减掉
	vcpu->msr_entry_load.aperf.msr_data -= vcpu->vm_exit_mperf_overhead; // APERF 计数补偿：减去 VM-exit 固定成本
	vcpu->msr_entry_load.mperf.msr_data -= vcpu->vm_exit_mperf_overhead; // MPERF 计数补偿：减去 VM-exit 固定成本

	if (perf_global_ctrl.en_fixed_ctrn & (1ull << 2))
	{
		auto const cpl = hv::current_guest_cpl(); // 获取来宾当前 CPL（特权级），决定计数器是否对 OS/用户可见

		ia32_fixed_ctr_ctrl_register fixed_ctr_ctrl; // 固定计数器控制寄存器解析结构，方便检查 OS/用户使能位
		fixed_ctr_ctrl.flags = __readmsr(IA32_FIXED_CTR_CTRL); // 读取硬件固定计数器控制 MSR 的当前状态

		// 只有当对应特权级允许计数时，才对 FIXED_CTR2 做补偿。// 目的：避免在计数被禁用时改写无意义值
		if ((cpl == 0 && fixed_ctr_ctrl.en2_os) || (cpl == 3 && fixed_ctr_ctrl.en2_usr)) // CPL=0 看 en2_os，CPL=3 看 en2_usr
			__writemsr(IA32_FIXED_CTR2, __readmsr(IA32_FIXED_CTR2) - vcpu->vm_exit_ref_tsc_overhead); // 读出后减去 VM-exit 参考 TSC 开销再写回
	}

	if (!vcpu->hide_vm_exit_overhead || vcpu->vm_exit_tsc_overhead > 10000)
	{
		// 这是重新同步 TSC 偏移的时机，避免累计误差。// 作用：让后续 TSC 偏移回到基准
		vcpu->tsc_offset = 0; // 清零 TSC 偏移，放弃隐藏本次开销
		// 软禁用 VMX 预抢占计时器，减少额外 VM-exit 干扰。// 硬件原理：计时器值全 1 相当于“极大延迟”
		vcpu->preemption_timer = ~0ull; // 设置为全 1，基本等价于不触发预抢占 VM-exit
		return; // 直接返回，停止当前的开销隐藏流程
	}

	// 设定预抢占计时器：让来宾在 10000 个 TSC tick 后产生 VM-exit，用于重新校准。// 原理：以较短周期重新测量偏差
	ia32_vmx_misc_register vmx_misc; // VMX_MISC 包含计时器与 TSC 的比例关系字段
	vmx_misc.flags = __readmsr(IA32_VMX_MISC); // 读取 VMX_MISC，获取“预抢占计时器与 TSC 的缩放关系”
	vcpu->preemption_timer = max(2, 10000 >> vmx_misc.preemption_timer_tsc_relationship); // 计算计时器 ticks，并确保最小为 2 防止过短

	// 通过 TSC offsetting 抵消 VM-exit 开销，让来宾读到“被平移后的 TSC”。// 硬件原理：VMCS 的 TSC_OFFSET 会加到来宾 TSC
	vcpu->tsc_offset -= vcpu->vm_exit_tsc_overhead; // 直接减去测得的开销，相当于把来宾时间“拉回去”
}

void skip_instruction() // 在 VM-exit 后“跳过当前指令”，模拟该指令已在来宾中执行完成
{
	// increment RIP：把来宾 RIP 前移到下一条指令
	auto const old_rip = hv::vmread(GUEST_RIP); // 读取当前来宾 RIP（触发 VM-exit 的指令地址）
	auto new_rip = old_rip + hv::vmread(VM_EXIT_INSTRUCTION_LENGTH); // 用指令长度推进 RIP，模拟“已执行”

	if (old_rip < (1ull << 32) && new_rip >= (1ull << 32))
	{
		vmx_segment_access_rights cs_access_rights; // CS 段访问权限结构，用来判断当前是 32 位还是 64 位代码段
		cs_access_rights.flags = static_cast<uint32_t>(hv::vmread(GUEST_CS_ACCESS_RIGHTS)); // 从 VMCS 读取 CS 权限位

		// make sure guest is in 32-bit mode：仅当不是 64 位长模式时才做 32 位回绕
		// 如果 guest 是 32 位模式，则修正 new_rip，确保仍在 32 位范围内
		if (!cs_access_rights.long_mode) // long_mode=0 表示不处于 64 位代码段
			new_rip &= 0xFFFF'FFFF;
	}

	hv::vmwrite(GUEST_RIP, new_rip); // 将修正后的 RIP 写回 VMCS，使来宾从下一条指令继续执行

	// 如果当前处于“阻止中断”的状态（由 MOV SS 或 STI 触发），需要解除阻止
	// 硬件原理：MOV SS / STI 会设置临时的中断屏蔽窗口，但我们已模拟执行指令，应清除该屏蔽
	auto interrupt_state = hv::read_interruptibility_state(); // 读取来宾的中断可中断状态（VMCS 字段）
	interrupt_state.blocking_by_mov_ss = 0; // 清除因 MOV SS 导致的中断阻塞
	interrupt_state.blocking_by_sti = 0; // 清除因 STI 导致的中断阻塞
	hv::write_interruptibility_state(interrupt_state); // 写回新的中断可中断状态到 VMCS

	ia32_debugctl_register debugctl; // 调试控制寄存器结构，用于判断单步/BTM 等调试行为
	debugctl.flags = hv::vmread(GUEST_DEBUG_CONTROL); // 读取来宾 IA32_DEBUGCTL，确定 BTF 等调试位

	rflags rflags; // RFLAGS 结构，用于判断 TF（单步标志）等位
	rflags.flags = hv::vmread(GUEST_RFLAGS); // 读取来宾 RFLAGS，查看是否开启单步

	if (rflags.trap_flag && !debugctl.btf)
	{
		vmx_pending_debug_exceptions dbg_exception; // VMCS 中的“待注入调试异常”字段结构
		dbg_exception.flags = hv::vmread(GUEST_PENDING_DEBUG_EXCEPTION); // 读取当前待注入调试异常标志
		dbg_exception.bs = 1; // 设置 BS（单步）位，表示应注入 #DB
		hv::vmwrite(GUEST_PENDING_DEBUG_EXCEPTION, dbg_exception.flags); // 写回 VMCS，确保 VM-entry 时注入 #DB
	}
}


//调整 RIP 指向下一条指令（简化版本的指令跳过）
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
//		//如果 guest 里开启单步，则必须取消因 STI/MOV SS 导致的阻塞，否则 #DB 可能被延迟
//		interruptibility.blocking_by_sti = false; // 清除 STI 引起的中断阻塞
//		interruptibility.blocking_by_mov_ss = false; // 清除 MOV SS 引起的中断阻塞
//		hv::vmwrite(GUEST_INTERRUPTIBILITY_STATE, interruptibility.all); // 写回中断阻塞状态，确保 #DB 及时注入
//	}
//}
void adjust_rip(__vcpu* vcpu) // Advance guest RIP using the complete skip helper.
{
	UNREFERENCED_PARAMETER(vcpu); // 原理：显式标记未使用参数；作用：消除编译器警告；结果：代码语义更清晰
	skip_instruction();
}

//void CleanUp(__vcpu* vcpu, __ept_hooked_page_info* hooked_page_info) // 清理一次 EPT 钩子操作的临时状态，防止残留影响后续逻辑
//{
//	// 别忘了扫尾：把当前钩子条目标记为无效，避免被重复使用
//	hooked_page_info->ID = -1; // 将 ID 置为 -1，表示该钩子条目不再有效
//	hooked_page_info->isBp = false; // 清除"命中断点"标记，避免后续误判
//	hooked_page_info->isInt3 = false; // 清除"INT3 断点"类型标记，恢复为默认状态
//	vcpu->ept_state->page_to_change = nullptr; // 清空待处理页面指针，避免下一次误用旧指针
//}
//
void CleanUp(__vcpu* vcpu, __ept_hooked_page_info* hooked_page_info) // Cleanup one pending EPT/MTF operation.
{
	hooked_page_info->ID = -1;
	hooked_page_info->isBp = false;
	hooked_page_info->isInt3 = false;
	hv::clear_pending_mtf_metadata(vcpu);
}

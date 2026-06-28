#include "VmExitCommon.h"

//恢复guest状态
void RestoreGuest() // 恢复来宾寄存器/段/表等状态，确保 VMXOFF 后来宾可继续运行
{
	//恢复guest的寄存器状态
	hv::vmwrite(CR0_READ_SHADOW, hv::read_effective_guest_cr0().flags); // 硬件原理：CR0 读影子用于 VMX 模拟，写回保证来宾读 CR0 得到一致值
	hv::vmwrite(CR4_READ_SHADOW, hv::read_effective_guest_cr4().flags); // 硬件原理：CR4 读影子用于 VMX 模拟，写回保证来宾读 CR4 得到一致值

	// 恢复dr7 cr3寄存器的值
	__writedr(7, hv::vmread(GUEST_DR7)); // 恢复来宾 DR7，确保断点/单步等调试行为一致
	__writecr3(hv::vmread(GUEST_CR3)); // 恢复来宾 CR3（页表基址），确保地址转换正确

	// MSRs
	__writemsr(IA32_SYSENTER_CS, hv::vmread(GUEST_SYSENTER_CS)); // 恢复 SYSENTER CS，保持快速系统调用语义
	__writemsr(IA32_SYSENTER_ESP, hv::vmread(GUEST_SYSENTER_ESP)); // 恢复 SYSENTER ESP，确保系统调用栈指针正确
	__writemsr(IA32_SYSENTER_EIP, hv::vmread(GUEST_SYSENTER_EIP)); // 恢复 SYSENTER EIP，确保系统调用入口正确
	__writemsr(IA32_PAT, hv::vmread(GUEST_PAT)); // 恢复 PAT，保证内存类型映射正确
	__writemsr(IA32_DEBUGCTL, hv::vmread(GUEST_DEBUG_CONTROL)); // 恢复调试控制 MSR，保持调试状态一致

	//恢复gdtr和idtr寄存器的值
	__reload_gdtr(hv::vmread(GUEST_GDTR_BASE), hv::vmread(GUEST_GDTR_LIMIT)); // 恢复 GDTR 基址与界限，保证分段描述符表正确
	__reload_idtr(hv::vmread(GUEST_IDTR_BASE), hv::vmread(GUEST_IDTR_LIMIT)); // 恢复 IDTR 基址与界限，保证中断描述符表正确

	segment_selector guest_tr; // 声明 TR 选择子变量，用于恢复任务寄存器
	guest_tr.flags = static_cast<uint16_t>(hv::vmread(GUEST_TR_SELECTOR)); // 从 VMCS 读取来宾 TR 选择子并保存

	// TSS
	//注意顺序要先将gdt的值写回gdtr寄存器
	__pseudo_descriptor64 gdtr = { 0 }; // 准备 GDTR 结构，用于读取当前 GDT
	__sgdt(&gdtr); // 读取当前 GDTR，获取 GDT 基址与界限
	(reinterpret_cast<segment_descriptor_32*>(gdtr.base_address) + guest_tr.index)->type = SEGMENT_DESCRIPTOR_TYPE_TSS_AVAILABLE; // 把 TSS 描述符标记为可用，满足加载 TR 的硬件要求
	write_tr(guest_tr.flags); // 从 VMCS 读取来宾 TR 选择子并保存

	// segment selectors
	write_ds(static_cast<uint16_t>(hv::vmread(GUEST_DS_SELECTOR))); // 恢复 DS 段选择子，保持来宾数据段一致
	write_es(static_cast<uint16_t>(hv::vmread(GUEST_ES_SELECTOR))); // 恢复 ES 段选择子，保持来宾数据段一致
	write_fs(static_cast<uint16_t>(hv::vmread(GUEST_FS_SELECTOR))); // 恢复 FS 段选择子，保持线程本地存储基址语义
	write_gs(static_cast<uint16_t>(hv::vmread(GUEST_GS_SELECTOR))); // 恢复 GS 段选择子，保持内核/线程相关基址语义
	write_ldtr(static_cast<uint16_t>(hv::vmread(GUEST_LDTR_SELECTOR))); // 恢复 LDTR 选择子，保持本地描述符表一致

	// FS and GS base address
	// 和__writemsr(IA32_GS_BASE, hv::vmread(GUEST_GS_BASE))比起来
	// 这种方式能立即将GUEST_GS_BASE的值写入到gs base寄存器里
	_writefsbase_u64(hv::vmread(GUEST_FS_BASE)); // 直接恢复 FS 基址，硬件立即生效，避免延迟
	_writegsbase_u64(hv::vmread(GUEST_GS_BASE)); // 直接恢复 GS 基址，硬件立即生效，避免延迟
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

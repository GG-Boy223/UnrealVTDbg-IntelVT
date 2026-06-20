#include "VmExitInstruction.h"

void vmexit_vm_instruction(__vcpu* vcpu) // 处理 guest 执行 VMX 指令导致的 VM-exit，统一向 guest 注入 #UD。
{
	sLog("\n"); // 保留日志占位，便于确认 guest 是否尝试执行 VMX 指令。
	UNREFERENCED_PARAMETER(vcpu); // 当前逻辑不需要修改 vCPU 状态，只按硬件语义注入异常。
	hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
		INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false); // VMX 指令对普通 guest 不可见，注入无错误码 #UD。
}

void vmexit_getsec_handler(__vcpu* vcpu) // 处理 GETSEC 退出，避免 guest 进入 SMX/安全启动相关路径。
{
	sLog("\n"); // 记录 GETSEC 触发，便于排查异常指令流。
	hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
		INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // 当前 VMM 不模拟 GETSEC，按保护异常反馈 guest。
}

void vmexit_vmx_on_handler(__vcpu* vcpu) // 处理 guest VMXON，防止嵌套 VMX 干扰 host VMCS。
{
	sLog("\n"); // 记录 guest VMXON 尝试。
	if (!hv::read_effective_guest_cr4().vmx_enable)
	{
		hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false); // CR4.VMXE 未开启时，硬件语义为 #UD。
	}
	else
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // CR4.VMXE 已开启但不允许嵌套虚拟化时反馈 #GP。
	}
}

void vmexit_invd_handler(__vcpu* vcpu) // 模拟 INVD 指令退出。
{
	sLog("\n"); // 记录缓存失效指令路径。
	__wbinvd(); // 使用 WBINVD 保守写回并失效缓存，避免直接 INVD 丢弃脏缓存造成数据风险。
	adjust_rip(vcpu); // 指令已模拟完成，推进 guest RIP。
}

void vmexit_invlpg_handler(__vcpu* vcpu) // 模拟 INVLPG，维护 VPID 单地址失效。
{
	sLog("\n"); // 记录 INVLPG 退出。

	invvpid_invidual_address_func(vcpu->vmexit_info.qualification, 1); // qualification 保存 INVLPG 线性地址，VPID=1 对应当前 guest 上下文。

	adjust_rip(vcpu); // TLB 失效完成后跳过 INVLPG。
}

void vmexit_rdtscp_handler(__vcpu* vcpu) // 模拟 RDTSCP，返回 TSC 和处理器 AUX 值。
{
	sLog("\n"); // 记录时间读取路径，便于分析反虚拟化检测。
	unsigned __int32 processor_id; // 接收 RDTSCP 返回的 IA32_TSC_AUX。
	unsigned __int64 tsc = __rdtscp(&processor_id); // 在 host 执行 RDTSCP，随后拆分写回 guest 寄存器。

	vcpu->vmexit_info.guest_registers->rcx = processor_id; // RDTSCP 约定 ECX 返回 TSC_AUX。
	vcpu->vmexit_info.guest_registers->rdx = MASK_GET_HIGHER_32BITS(tsc) >> 32; // RDX 返回 TSC 高 32 位。
	vcpu->vmexit_info.guest_registers->rax = MASK_GET_LOWER_32BITS(tsc); // RAX 返回 TSC 低 32 位。

	adjust_rip(vcpu); // 模拟完成后推进 RIP。
}

void vmexit_vmx_preemption_handler(__vcpu* vcpu) // VMX preemption timer 退出保留入口。
{
	sLog("\n"); // 当前仅记录触发，时间开销隐藏逻辑在其他路径控制。
}

void vmexit_xsetbv_handler(__vcpu* vcpu) // 模拟 XSETBV，严格验证 XCR0 合法性后再写入。
{
	sLog("\n"); // 记录扩展状态寄存器写入路径。
	__xcr0 new_xcr0; // 保存 guest 请求写入的新 XCR0。
	__xcr0 current_xcr0; // 保存 host 当前 XCR0，用于检查保留位一致性。

	if (!hv::read_effective_guest_cr4().os_xsave)
	{
		hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false); // CR4.OSXSAVE 未开启时 XSETBV 应产生 #UD。
		return; // 异常已安排，不推进 RIP。
	}

	uint32_t xcr_number = static_cast<uint32_t>(vcpu->vmexit_info.guest_registers->rcx); // ECX 指定 XCR 编号，当前只允许 XCR0。

	new_xcr0.all = (vcpu->vmexit_info.guest_registers->rdx << 32) | MASK_GET_LOWER_32BITS(vcpu->vmexit_info.guest_registers->rax); // EDX:EAX 组成 guest 请求的新值。

	current_xcr0.all = _xgetbv(0); // 读取 host 当前 XCR0，避免 guest 修改不可变/保留状态。

	cpuid_eax_0d_ecx_00 cpuid_0d; // CPUID.(EAX=0Dh,ECX=0) 描述处理器支持的 XCR0 位。
	__cpuidex(reinterpret_cast<int*>(&cpuid_0d), 0x0D, 0x00); // 查询扩展状态支持掩码。
	auto const unsupported_mask = ~((static_cast<uint64_t>(cpuid_0d.edx.flags) << 32) | cpuid_0d.eax.flags); // 取反得到 guest 不允许置位的位。

	if ((xcr_number != 0) ||
		(new_xcr0.reserved1 != current_xcr0.reserved1) ||
		(new_xcr0.reserved2 != current_xcr0.reserved2) ||
		(new_xcr0.reserved3 != current_xcr0.reserved3) ||
		(new_xcr0.reserved4 != current_xcr0.reserved4) ||
		(new_xcr0.all & unsupported_mask) ||
		(new_xcr0.x87 == 0) ||
		(new_xcr0.sse == 0 && new_xcr0.avx == 1) ||
		(new_xcr0.avx == 0 && (new_xcr0.opmask || new_xcr0.zmm_hi256 || new_xcr0.hi16_zmm)) ||
		(new_xcr0.bndreg != new_xcr0.bndcsr) ||
		(new_xcr0.opmask != new_xcr0.zmm_hi256 || new_xcr0.zmm_hi256 != new_xcr0.hi16_zmm) ||
		(new_xcr0.TILECFG != new_xcr0.TILEDATA)
		)
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // XCR 编号、保留位或依赖关系非法时按硬件语义注入 #GP(0)。
		return; // 非法写入不推进 RIP。
	}

	hv::host_exception_info e; // 捕获 host 执行 XSETBV 时可能产生的异常。
	hv::xsetbv_safe(e, xcr_number, new_xcr0.all); // 通过安全封装写 XCR，防止 host 异常直接 BugCheck。

	if (e.exception_occurred)
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // host 侧写入失败时向 guest 反馈 #GP。
		return; // 失败路径不推进 RIP。
	}

	set_hide_vm_exit_overhead(vcpu, true); // XSETBV 属于可被计时检测的指令，标记需要隐藏 VM-exit 开销。
	adjust_rip(vcpu); // 写入成功后推进 guest RIP。
}

void vmexit_rdtsc_handler(__vcpu* vcpu) // 模拟 RDTSC，返回 host TSC 拆分值。
{
	sLog("\n"); // 记录时间读取路径。
	unsigned __int64 tsc = __rdtsc(); // 在 host 执行 RDTSC，后续可由时间隐藏逻辑修正开销。

	vcpu->vmexit_info.guest_registers->rdx = MASK_GET_HIGHER_32BITS(tsc) >> 32; // RDX 返回高 32 位。
	vcpu->vmexit_info.guest_registers->rax = MASK_GET_LOWER_32BITS(tsc); // RAX 返回低 32 位。

	adjust_rip(vcpu); // 模拟完成后推进 RIP。
}

void vmexit_cpuid_handler(__vcpu* vcpu) // 模拟 CPUID，按 guest 输入 leaf/subleaf 返回寄存器结果。
{
	int regs[4]; // 保存 EAX/EBX/ECX/EDX 返回值。
	__cpuidex(regs, vcpu->vmexit_info.guest_registers->eax, vcpu->vmexit_info.guest_registers->ecx); // 使用 guest EAX/ECX 作为 leaf/subleaf。

	vcpu->vmexit_info.guest_registers->rax = regs[0]; // 写回 CPUID.EAX。
	vcpu->vmexit_info.guest_registers->rbx = regs[1]; // 写回 CPUID.EBX。
	vcpu->vmexit_info.guest_registers->rcx = regs[2]; // 写回 CPUID.ECX。
	vcpu->vmexit_info.guest_registers->rdx = regs[3]; // 写回 CPUID.EDX。

	set_hide_vm_exit_overhead(vcpu, true); // CPUID 常被用于虚拟化开销检测，标记需要时间补偿。
	adjust_rip(vcpu); // CPUID 已模拟完成，推进 RIP。
}

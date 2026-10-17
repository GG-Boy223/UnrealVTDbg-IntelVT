#include "VmExitInstruction.h"

//处理来宾执行 VMX 指令的 VM-exit：统一注入 #UD，阻止来宾进入 VMX operation
void vmexit_vm_instruction(__vcpu* vcpu) // 处理 guest 执行 VMX 指令导致的 VM-exit，统一向 guest 注入 #UD。
{
	sLog("\n"); // 保留日志占位，便于确认 guest 是否尝试执行 VMX 指令。
	UNREFERENCED_PARAMETER(vcpu); // 当前逻辑不需要修改 vCPU 状态，只按硬件语义注入异常。
	hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
		INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false); // VMX 指令对普通 guest 不可见，注入无错误码 #UD。
}

//Vol1[5.23 SAFER MODE EXTENSIONS]
//SMX: GETSEC.
//处理 GETSEC 指令的 VM-exit：来宾尝试进入 SMX 安全模式时统一注入 #GP
void vmexit_getsec_handler(__vcpu* vcpu) // 处理 GETSEC 退出，避免 guest 进入 SMX/安全启动相关路径。
{
	sLog("\n"); // 记录 GETSEC 触发，便于排查异常指令流。
	hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
		INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // 当前 VMM 不模拟 GETSEC，按保护异常反馈 guest。
}

//处理 VMXON 指令的 VM-exit：根据 CR4.VMXE 状态注入 #UD 或 #GP，阻止来宾进入 VMX operation
void vmexit_vmx_on_handler(__vcpu* vcpu) // 处理 guest VMXON，防止嵌套 VMX 干扰 host VMCS。
{
	sLog("\n"); // 记录 guest VMXON 尝试。
	if (!hv::read_effective_guest_cr4().vmx_enable) // 原理：CR4.VMXE=0 表示 VMX 指令无效；作用：按硬件语义注入 #UD；结果：来宾认为指令不可用
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

//清除cpu缓存,并将数据写回到内存
void vmexit_invd_handler(__vcpu* vcpu) // 模拟 INVD 指令退出。
{
	sLog("\n"); // 记录缓存失效指令路径。
	//hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT, INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
	__wbinvd(); // 使用 WBINVD 保守写回并失效缓存，避免直接 INVD 丢弃脏缓存造成数据风险。
	adjust_rip(vcpu); // 指令已模拟完成，推进 guest RIP。
}

//处理 INVLPG 指令的 VM-exit：按来宾给定地址失效 TLB，并让来宾继续执行后续指令
void vmexit_invlpg_handler(__vcpu* vcpu) // 模拟 INVLPG，维护 VPID 单地址失效。
{
	sLog("\n"); // 记录 INVLPG 退出。

	invvpid_invidual_address_func(vcpu->vmexit_info.qualification, guest_vpid); // qualification 保存 INVLPG 线性地址，VPID=1 对应当前 guest 上下文。

	adjust_rip(vcpu); // TLB 失效完成后跳过 INVLPG。
}

//处理 RDTSCP 指令：读取时间戳计数器 TSC，并返回 IA32_TSC_AUX（通常为 CPU/核心标识）
void vmexit_rdtscp_handler(__vcpu* vcpu) // 模拟 RDTSCP，返回 TSC 和处理器 AUX 值。
{
	sLog("\n"); // 记录时间读取路径，便于分析反虚拟化检测。
	unsigned __int32 processor_id; // 接收 RDTSCP 返回的 IA32_TSC_AUX。
	unsigned __int64 tsc = __rdtscp(&processor_id); // 在 host 执行 RDTSCP，随后拆分写回 guest 寄存器。

	// 下面三行将 RDTSCP 的输出写回到来宾寄存器，等价于来宾自己执行了 RDTSCP
	vcpu->vmexit_info.guest_registers->rcx = processor_id; // RDTSCP 约定 ECX 返回 TSC_AUX。
	vcpu->vmexit_info.guest_registers->rdx = MASK_GET_HIGHER_32BITS(tsc) >> 32; // RDX 返回 TSC 高 32 位。
	vcpu->vmexit_info.guest_registers->rax = MASK_GET_LOWER_32BITS(tsc); // RAX 返回 TSC 低 32 位。

	adjust_rip(vcpu); // 模拟完成后推进 RIP。
}

void vmexit_vmx_preemption_handler(__vcpu* vcpu) // VMX preemption timer 退出保留入口。
{
	// 这里目前什么都不做
	sLog("\n"); // 当前仅记录触发，时间开销隐藏逻辑在其他路径控制。
}

//处理 XSETBV 指令：校验 XCR0 写入规则并安全执行，非法则注入 #UD/#GP
void vmexit_xsetbv_handler(__vcpu* vcpu) // 模拟 XSETBV，严格验证 XCR0 合法性后再写入。
{
	sLog("\n"); // 记录扩展状态寄存器写入路径。
	__xcr0 new_xcr0; // 保存 guest 请求写入的新 XCR0。
	__xcr0 current_xcr0; // 保存 host 当前 XCR0，用于检查保留位一致性。

	// 原理：XSETBV 只有在 CR4.OSXSAVE=1 时合法；作用：检查前置条件；结果：不满足则注入 #UD
	if (!hv::read_effective_guest_cr4().os_xsave) // 原理：读取来宾 CR4.OSXSAVE；作用：判断是否允许执行 XSETBV；结果：为 0 则非法
	{
		hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false); // CR4.OSXSAVE 未开启时 XSETBV 应产生 #UD。
		return; // 异常已安排，不推进 RIP。
	}

	//// 原理:XSETBV 是特权指令,非 CPL0 执行必须产生 #GP(0);作用:补齐硬件语义;结果:不推进 RIP
	//if (hv::current_guest_cpl() != 0) // 原理:读取当前 guest CPL;作用:判断是否满足特权级要求;结果:非 0 则非法
	//{
	//	hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
	//		INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // 非特权级写入 XCR 时按硬件语义注入 #GP(0)。
	//	return; // 异常已安排,不推进 RIP。
	//}

	uint32_t xcr_number = static_cast<uint32_t>(vcpu->vmexit_info.guest_registers->rcx); // ECX 指定 XCR 编号，当前只允许 XCR0。

	new_xcr0.all = (vcpu->vmexit_info.guest_registers->rdx << 32) | MASK_GET_LOWER_32BITS(vcpu->vmexit_info.guest_registers->rax); // EDX:EAX 组成 guest 请求的新值。

	current_xcr0.all = _xgetbv(0); // 读取 host 当前 XCR0，避免 guest 修改不可变/保留状态。

	cpuid_eax_0d_ecx_00 cpuid_0d; // CPUID.(EAX=0Dh,ECX=0) 描述处理器支持的 XCR0 位。
	__cpuidex(reinterpret_cast<int*>(&cpuid_0d), 0x0D, 0x00); // 查询扩展状态支持掩码。
	auto const unsupported_mask = ~((static_cast<uint64_t>(cpuid_0d.edx.flags) << 32) | cpuid_0d.eax.flags); // 取反得到 guest 不允许置位的位。

	// 原理：XSETBV 对编号、保留位和依赖位有严格约束；作用：逐项校验；结果：不满足则注入 #GP
	if ((xcr_number != 0) || // 原理：当前仅支持 XCR0；作用：禁止其它编号；结果：非 0 则非法
		(new_xcr0.reserved1 != current_xcr0.reserved1) || // 原理：保留位必须保持不变；作用：检测非法改动；结果：不一致则非法
		(new_xcr0.reserved2 != current_xcr0.reserved2) || // 原理：保留位必须保持不变；作用：检测非法改动；结果：不一致则非法
		(new_xcr0.reserved3 != current_xcr0.reserved3) || // 原理：保留位必须保持不变；作用：检测非法改动；结果：不一致则非法
		(new_xcr0.reserved4 != current_xcr0.reserved4) || // 原理：保留位必须保持不变；作用：检测非法改动；结果：不一致则非法
		(new_xcr0.all & unsupported_mask) || // 原理：不支持的位不能置 1；作用：检测非法置位；结果：命中则非法
		(new_xcr0.x87 == 0) || // 原理：x87 位必须为 1；作用：保证基本浮点状态可用；结果：为 0 则非法
		(new_xcr0.sse == 0 && new_xcr0.avx == 1) || // 原理：AVX 依赖 SSE；作用：保证依赖关系；结果：违背则非法
		(new_xcr0.avx == 0 && (new_xcr0.opmask || new_xcr0.zmm_hi256 || new_xcr0.hi16_zmm)) || // 原理：AVX-512 依赖 AVX；作用：保证依赖关系；结果：违背则非法
		(new_xcr0.bndreg != new_xcr0.bndcsr) || // 原理：MPX 位需成对启用；作用：保持一致；结果：不一致则非法
		(new_xcr0.opmask != new_xcr0.zmm_hi256 || new_xcr0.zmm_hi256 != new_xcr0.hi16_zmm) || // 原理：AVX-512 相关位需一致；作用：保持一致；结果：不一致则非法
		(new_xcr0.TILECFG != new_xcr0.TILEDATA) // 原理：AMX TILECFG/TILEDATA 需成对启用；作用：保持一致；结果：不一致则非法
		)
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // XCR 编号、保留位或依赖关系非法时按硬件语义注入 #GP(0)。
		return; // 非法写入不推进 RIP。
	}

	// hv::host_exception_info e; // 旧逻辑：未清零会让safe异常路径读取到旧栈值，导致误判exception_occurred。
	hv::host_exception_info e = { 0 }; // 捕获host执行XSETBV时可能产生的异常，清零后再交给Host IDT写回。
	hv::xsetbv_safe(e, xcr_number, new_xcr0.all); // 通过安全封装写 XCR，防止 host 异常直接 BugCheck。

	if (e.exception_occurred) // 原理：xsetbv 执行期间可能触发异常；作用：检测写入是否失败；结果：失败则注入 #GP
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // host 侧写入失败时向 guest 反馈 #GP。
		return; // 失败路径不推进 RIP。
	}

	set_hide_vm_exit_overhead(vcpu, true); // XSETBV 属于可被计时检测的指令，标记需要隐藏 VM-exit 开销。
	adjust_rip(vcpu); // 写入成功后推进 guest RIP。
}

//处理 RDTSC 指令：读取时间戳计数器 TSC，并按 EDX:EAX 返回给来宾
void vmexit_rdtsc_handler(__vcpu* vcpu) // 模拟 RDTSC，返回 host TSC 拆分值。
{
	sLog("\n"); // 记录时间读取路径。
	unsigned __int64 tsc = __rdtsc(); // 在 host 执行 RDTSC，后续可由时间隐藏逻辑修正开销。

	vcpu->vmexit_info.guest_registers->rdx = MASK_GET_HIGHER_32BITS(tsc) >> 32; // RDX 返回高 32 位。
	vcpu->vmexit_info.guest_registers->rax = MASK_GET_LOWER_32BITS(tsc); // RAX 返回低 32 位。

	adjust_rip(vcpu); // 模拟完成后推进 RIP。
}

//处理 CPUID 指令：根据来宾 leaf/subleaf 返回 CPU 特性信息，可在此处做伪装/过滤
void vmexit_cpuid_handler(__vcpu* vcpu) // 模拟 CPUID，按 guest 输入 leaf/subleaf 返回寄存器结果。
{
	int regs[4]; // 保存 EAX/EBX/ECX/EDX 返回值。
	const unsigned __int32 cpuid_leaf =
		static_cast<unsigned __int32>(vcpu->vmexit_info.guest_registers->eax);
	__cpuidex(regs, cpuid_leaf, vcpu->vmexit_info.guest_registers->ecx); // 使用 guest EAX/ECX 作为 leaf/subleaf。

	// hv_presence=false 时隐藏 VMX 能力位与 hypervisor-present 位；否则 CPUID 会直接暴露
	// 本 VMM 的 VMX 能力/存在性，与 IA32_FEATURE_CONTROL / CR4.VMXE 的隐藏语义不一致。
	// Intel SDM Vol2A CPUID：01H:ECX[5]=VMX，01H:ECX[31]=Hypervisor present。
	if (!g_vmm_context.hv_presence)
	{
		if (cpuid_leaf == 0x1)
		{
			regs[2] &= ~(1 << 5);  // CPUID.01H:ECX.VMX
			regs[2] &= ~(1u << 31); // CPUID.01H:ECX.Hypervisor present
		}
		else if (cpuid_leaf >= CPUID_HV_VENDOR_AND_MAX_FUNCTIONS &&
			cpuid_leaf < CPUID_HV_VENDOR_AND_MAX_FUNCTIONS + 0x100)
		{
			// 非 hypervisor 环境下 40000000H..400000FFH 为未定义/不支持叶。
			// Intel CPU 对不支持的叶返回全 0；这里按该语义抹掉 VMM vendor/feature 信息。
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;
		}
	}

	// CPUID 只写 32 位寄存器，64 位模式下高 32 位按架构清零。
	// 直接赋值 int 会在 bit31=1 时把结果符号扩展成 0xFFFFFFFF'xxxxxxxx，
	// 使 guest 读到与真实 CPUID 不一致的 64 位值（Intel SDM Vol2 CPUID 语义）。
	vcpu->vmexit_info.guest_registers->rax = static_cast<unsigned __int32>(regs[0]); // 写回 CPUID.EAX，零扩展。
	vcpu->vmexit_info.guest_registers->rbx = static_cast<unsigned __int32>(regs[1]); // 写回 CPUID.EBX，零扩展。
	vcpu->vmexit_info.guest_registers->rcx = static_cast<unsigned __int32>(regs[2]); // 写回 CPUID.ECX，零扩展。
	vcpu->vmexit_info.guest_registers->rdx = static_cast<unsigned __int32>(regs[3]); // 写回 CPUID.EDX，零扩展。

	set_hide_vm_exit_overhead(vcpu, true); // CPUID 常被用于虚拟化开销检测，标记需要时间补偿。
	adjust_rip(vcpu); // CPUID 已模拟完成，推进 RIP。
}

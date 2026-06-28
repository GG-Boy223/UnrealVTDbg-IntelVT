#include "VmExitControl.h"

void vmexit_mov_dr_handler(__vcpu* vcpu)
{
	__exit_qualification_dr qual = { vcpu->vmexit_info.qualification };
	unsigned dr = (unsigned)qual.debug_register_number;
	unsigned gpr = (unsigned)qual.gp_register;

	if (qual.access_direction == 0) // mov to DRx
	{
		const uint64_t value = hv::read_guest_gpr(vcpu->vmexit_info.guest_registers, gpr);

		switch (dr)
		{
		case 0: __writedr(0, value); break;
		case 1: __writedr(1, value); break;
		case 2: __writedr(2, value); break;
		case 3: __writedr(3, value); break;
		case 6:
		{
			__dr6 dr6; dr6.all = value;
			dr6.reserved_1 = 0xFF;
			dr6.reserved_2 = 0;
			dr6.reserved_3 = 0x7FFF;
			__writedr(6, dr6.all);
			break;
		}
		case 7:
		{
			__dr7 dr7; dr7.all = value;
			dr7.reserved_1 = 1;
			dr7.reserved_2 = 0;
			dr7.reserved_3 = 0;
			__writedr(7, dr7.all);
			hv::vmwrite(GUEST_DR7, dr7.all);
			break;
		}
		default:
			//hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
			//	INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			//adjust_rip(vcpu);
			hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false);
			return;
		}
	}
	else
	{
		uint64_t value = 0;
		switch (dr)
		{
		case 0: value = __readdr(0); break;
		case 1: value = __readdr(1); break;
		case 2: value = __readdr(2); break;
		case 3: value = __readdr(3); break;
		case 6: value = __readdr(6); break;
		case 7: value = hv::vmread(GUEST_DR7); break;
		default:
			//hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
			//	INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			//adjust_rip(vcpu);
			hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false);
			return;
		}
		hv::write_guest_gpr(vcpu->vmexit_info.guest_registers, gpr, value);
	}

	adjust_rip(vcpu);
}

//处理 MOV 到 CR3：解析来宾写入值，识别 PCID 的不刷新语义，校验物理地址保留位，并按需刷新 TLB
bool mov_to_cr3_handler(__vcpu* vcpu, unsigned __int64 guest_cr3) // vcpu=当前虚拟CPU状态；guest_cr3=来宾请求写入的 CR3 原始值
{
	cr3 new_cr3; // 原理：CR3 是页表基址寄存器，结构体可按位访问 PCID/基址；作用：准备解析 guest_cr3；结果：new_cr3 可逐位读写
	new_cr3.flags = guest_cr3; // 原理：把来宾提供的 64 位原始值装入解析结构；作用：建立解析基准；结果：new_cr3 表示来宾请求的 CR3

	auto const curr_cr4 = hv::read_effective_guest_cr4(); // 原理：PCID 开关在 CR4.PCIDE 位；作用：判断 CR3[63] 的语义；结果：得到当前有效 CR4

	bool invalidate_tlb = true; // 原理：CR3 改变会导致地址翻译变化；作用：默认需要刷新 TLB；结果：先假设要刷新

	// 原理：当 CR4.PCIDE=1 且 CR3[63]=1 时，硬件定义为切换 PCID 但不刷新 TLB
	if (curr_cr4.pcid_enable && (new_cr3.flags & (1ull << 63)))
	{
		invalidate_tlb = false; // 原理：硬件允许保留 TLB；作用：告诉后续不执行刷新；结果：旧 TLB 条目被保留
		new_cr3.flags &= ~(1ull << 63); // 原理：bit63 是控制位不是地址位；作用：清除此位再写回 CR3；结果：CR3 地址字段合法
	}

	cpuid_eax_80000008 cpuid_80000008; // 原理：CPUID 0x80000008 返回 MAXPHYSADDR；作用：准备接收硬件能力；结果：结构体待填充
	__cpuid(reinterpret_cast<int*>(&cpuid_80000008), 0x80000008); // 参数1：输出缓冲区地址；参数2：CPUID 叶号；结果：填充物理地址位数信息
	uint64_t max_phys_addr = cpuid_80000008.eax.number_of_physical_address_bits; // 原理：EAX 字段给出有效物理地址位数；作用：用于构造保留位掩码；结果：得到 MAXPHYSADDR

	auto const reserved_mask = ~((1ull << max_phys_addr) - 1); // 原理：MAXPHYSADDR 以上位是保留位；作用：构造非法位掩码；结果：reserved_mask 标识所有必须为 0 的位
	if (new_cr3.flags & reserved_mask) // 原理：CR3 保留位为 1 属于非法写入；作用：检测非法高位；结果：非法则注入 #GP
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
		return false;
	}

	if (invalidate_tlb) // 原理：CR3 或 PCID 变化会使旧 TLB 失效；作用：根据标志决定是否刷新；结果：确保地址翻译一致
	{
		invvpid_single_context_except_global_translations(guest_vpid); // 参数：guest_vpid=当前来宾 VPID；结果：刷新该上下文的非全局 TLB
	}

	hv::vmwrite(GUEST_CR3, new_cr3.flags);
	return true; // 原理：走到此处表示写入成功；作用：通知上层继续执行；结果：返回成功
}

//处理 LMSW 指令：仅更新 CR0 的低 4 位（PE/MP/EM/TS），其余位保持不变
void emulate_lmsw(__vcpu* vcpu, uint16_t const value) // vcpu=当前虚拟CPU状态；value=LMSW 指令提供的 16 位源操作数
{
	sLog("\n"); // 原理：记录日志用于调试；作用：保持现有日志风格；结果：不改变来宾状态

	cr0 new_cr0; // 原理：用 CR0 结构解析低位；作用：准备从 value 提取 PE/MP/EM/TS；结果：new_cr0 仅低位有效
	new_cr0.flags = value; // 原理：LMSW 只提供 16 位；作用：把低位装入结构体；结果：高位自动为 0

	cr0 shadow_cr0; // 原理：CR0 读影子用于虚拟化下的 CR0 读取；作用：准备更新影子低位；结果：影子值可被修改
	shadow_cr0.flags = hv::vmread(CR0_READ_SHADOW); // 原理：从 VMCS 读取当前 CR0 影子；作用：获取现有影子位；结果：shadow_cr0 已初始化
	shadow_cr0.protection_enable = new_cr0.protection_enable; // 原理：LMSW 允许修改 PE；作用：同步 PE 位到影子；结果：影子 PE 更新
	shadow_cr0.monitor_coprocessor = new_cr0.monitor_coprocessor; // 原理：LMSW 允许修改 MP；作用：同步 MP 位到影子；结果：影子 MP 更新
	shadow_cr0.emulate_fpu = new_cr0.emulate_fpu; // 原理：LMSW 允许修改 EM；作用：同步 EM 位到影子；结果：影子 EM 更新
	shadow_cr0.task_switched = new_cr0.task_switched; // 原理：LMSW 允许修改 TS；作用：同步 TS 位到影子；结果：影子 TS 更新
	hv::vmwrite(CR0_READ_SHADOW, shadow_cr0.flags); // 原理：写回 VMCS 影子 CR0；作用：保证来宾读取 CR0 时看到新低位；结果：影子生效

	cr0 real_cr0; // 原理：真实 CR0 决定硬件行为；作用：准备更新真实 CR0 的低 4 位；结果：real_cr0 可被修改
	real_cr0.flags = hv::vmread(GUEST_CR0); // 原理：读取 VMCS 中的来宾 CR0；作用：获取当前真实 CR0；结果：real_cr0 初始化
	real_cr0.protection_enable = new_cr0.protection_enable; // 原理：PE 控制保护模式；作用：同步 PE 位到真实 CR0；结果：保护模式位一致
	real_cr0.monitor_coprocessor = new_cr0.monitor_coprocessor; // 原理：MP 控制协处理器监视；作用：同步 MP 位到真实 CR0；结果：MP 位一致
	real_cr0.emulate_fpu = new_cr0.emulate_fpu; // 原理：EM 控制 FPU 模拟；作用：同步 EM 位到真实 CR0；结果：EM 位一致
	real_cr0.task_switched = new_cr0.task_switched; // 原理：TS 表示任务切换；作用：同步 TS 位到真实 CR0；结果：TS 位一致
	hv::vmwrite(GUEST_CR0, real_cr0.flags); // 原理：写回 VMCS 的 GUEST_CR0；作用：使来宾真实 CR0 生效；结果：硬件按新低位工作
}

//处理 MOV 到 CR0：严格校验组合合法性，必要时同步 EPT 内存类型，并更新真实 CR0 与读影子
bool mov_to_cr0_handler(__vcpu* vcpu, unsigned __int64 guest_cr0) // vcpu=当前虚拟CPU状态；guest_cr0=来宾请求写入的 CR0 原始值
{
	sLog("\n"); // 原理：记录日志用于调试；作用：保持现有日志风格；结果：不改变来宾状态

	// 原理：使用 union 复用同一段 64 位数据；作用：可按 CR0/CR3/CR4 不同视图解释；结果：便于按位解析
	union
	{
		__cr0 cr0;
		__cr3 cr3;
		__cr4 cr4;
		unsigned __int64 all;
	}cr_registers;

	cr_registers.cr0.all = guest_cr0; // 原理：把原始值解释为 CR0 位域；作用：拆分 guest_cr0；结果：cr_registers.cr0 可直接访问

	auto const curr_cr0 = hv::read_effective_guest_cr0(); // 原理：读取当前有效 CR0；作用：用于比较变化与依赖；结果：curr_cr0 就绪
	auto const curr_cr4 = hv::read_effective_guest_cr4(); // 原理：部分 CR0 位与 CR4 有依赖；作用：辅助校验；结果：curr_cr4 就绪

	// 原理：下列组合在硬件上非法或本实现不支持；作用：提前拦截；结果：非法则注入 #GP
	if ((cr_registers.cr0.paging_enable == 0) || // 原理：此实现要求分页保持开启；作用：阻止关闭 PG；结果：PG=0 视为非法
		(cr_registers.cr0.paging_enable && !cr_registers.cr0.protection_enable) || // 原理：PG=1 但 PE=0 违反硬件规则；作用：检测非法组合；结果：触发 #GP
		(!cr_registers.cr0.cache_disable && cr_registers.cr0.not_write_through) || // 原理：NW=1 必须同时 CD=1；作用：检测缓存位冲突；结果：触发 #GP
		(!cr_registers.cr0.write_protect && curr_cr4.control_flow_enforcement_enable) || // 原理：CET 需要 CR0.WP=1；作用：检测依赖关系；结果：违规则 #GP
		(cr_registers.cr0.all & 0xFFFFFFFF00000000)) // 原理：高 32 位为保留位；作用：检测保留位是否被置 1；结果：非法则 #GP
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
		return false;
	}

	if (cr_registers.cr0.cache_disable != curr_cr0.cache_disable ||
		cr_registers.cr0.not_write_through != curr_cr0.not_write_through)
	{
		// 原理：CD/NW 改变会影响缓存策略；作用：同步 EPT 内存类型；结果：来宾看到一致的缓存行为
		if (cr_registers.cr0.cache_disable)
		{
			hv::set_ept_memory_type(*vcpu->ept_state, MEMORY_TYPE_UNCACHEABLE); // 参数1：EPT 状态；参数2：UC；结果：EPT 设为不可缓存
		}
		else
		{
			hv::update_ept_memory_type(*vcpu->ept_state); // 参数：EPT 状态；结果：按 MTRR 重新计算内存类型
		}

		_mm_mfence();
		invept_all_contexts_func();
	}

	__cr0 cr0; // 原理：准备写回真实 CR0；作用：建立可修改副本；结果：cr0 可调整
	cr0.all = cr_registers.cr0.all; // 原理：复制来宾请求值；作用：作为写回基础；结果：cr0 包含来宾位
	cr0.reserved_1 = 0; // 原理：保留位必须为 0；作用：清除保留位；结果：满足硬件要求
	cr0.reserved_2 = 0; // 原理：保留位必须为 0；作用：清除保留位；结果：满足硬件要求
	cr0.reserved_3 = 0; // 原理：保留位必须为 0；作用：清除保留位；结果：满足硬件要求
	cr0.extension_type = 1; // 原理：ET 位固定为 1；作用：强制置位；结果：符合架构约束
	hv::vmwrite(CR0_READ_SHADOW, cr0.all); // 原理：更新 CR0 读影子；作用：保证来宾读取看到一致值；结果：影子生效

	__cr_fixed cr_fixed; // 原理：CR0 固定位掩码结构；作用：应用 VMX 的固定约束；结果：准备强制置/清位
	cr_fixed.all = __readmsr(IA32_VMX_CR0_FIXED0); // 参数：MSR IA32_VMX_CR0_FIXED0；结果：得到必须为 1 的位集合
	cr0.all |= cr_fixed.split.low; // 原理：FIXED0 标记必须为 1；作用：强制置位；结果：满足 VMX 要求
	cr_fixed.all = __readmsr(IA32_VMX_CR0_FIXED1);
	cr0.all &= cr_fixed.split.low; // 原理：FIXED1 中为 0 的位必须清零；作用：强制清位；结果：满足 VMX 要求
	hv::vmwrite(GUEST_CR0, cr0.all); // 原理：写回 VMCS 的 GUEST_CR0；作用：让来宾真实 CR0 生效；结果：硬件按新 CR0 行为运行
	return true; // 原理：执行成功到达末尾；作用：告知上层继续执行；结果：返回成功
}

//处理 MOV 到 CR4：校验保留位/模式合法性，并更新真实 CR4 与读影子
bool mov_to_cr4_handler(unsigned __int64 guest_cr4) // guest_cr4=来宾请求写入的 CR4 原始值
{
	sLog("\n"); // 原理：记录日志用于调试；作用：保持现有日志风格；结果：不改变来宾状态
	cr4 new_cr4; // 原理：CR4 结构便于逐位解析；作用：准备解析 guest_cr4；结果：new_cr4 可访问各功能位
	new_cr4.flags = guest_cr4; // 原理：把原始值装入解析结构；作用：建立解析基准；结果：new_cr4 表示来宾请求

	__cr3 guest_cr3; // 原理：CR3 中包含 PCID 字段；作用：用于 PCID 启用校验；结果：准备读取 CR3
	guest_cr3.all = hv::vmread(GUEST_CR3); // 原理：从 VMCS 读取当前来宾 CR3；作用：获取 PCID；结果：guest_cr3 初始化

	auto const curr_cr0 = hv::read_effective_guest_cr0(); // 原理：部分 CR4 位依赖 CR0；作用：辅助校验；结果：curr_cr0 就绪
	auto const curr_cr4 = hv::read_effective_guest_cr4(); // 原理：比较变化与依赖关系；作用：辅助校验；结果：curr_cr4 就绪

	// 原理：下列位组合非法或此实现不支持；作用：提前拦截；结果：非法则注入 #GP
	if ((new_cr4.reserved1 != 0) || // 原理：保留位必须为 0；作用：检测保留位；结果：非法则 #GP
		(new_cr4.reserved2 != 0) || // 原理：保留位必须为 0；作用：检测保留位；结果：非法则 #GP
		(new_cr4.physical_address_extension == 0) || // 原理：长模式下必须启用 PAE；作用：阻止关闭 PAE；结果：非法则 #GP
		(new_cr4.linear_addresses_57_bit) || // 原理：LA57（5 级页表）此实现不支持；作用：阻止开启；结果：非法则 #GP
		(new_cr4.control_flow_enforcement_enable && !curr_cr0.write_protect) || // 原理：CET 需要 CR0.WP=1；作用：检测依赖；结果：违规则 #GP
		((new_cr4.pcid_enable && !curr_cr4.pcid_enable) && guest_cr3.pcid != 0) || // 原理：首次开启 PCID 时 CR3.PCID 必须为 0；作用：检测规则；结果：违规则 #GP
		(new_cr4.flags & 0xFFFFFFFF00000000)) // 原理：高 32 位为保留位；作用：检测保留位；结果：非法则 #GP
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
		return false;
	}

	if (new_cr4.page_global_enable != curr_cr4.page_global_enable ||
		!new_cr4.pcid_enable && curr_cr4.pcid_enable ||
		new_cr4.smep_enable && !curr_cr4.smep_enable)
	{
		invvpid_single_context_func(guest_vpid); // 参数：guest_vpid=当前来宾 VPID；结果：刷新该 vCPU 的 TLB
	}

	hv::vmwrite<unsigned __int64>(CR4_READ_SHADOW, new_cr4.flags); // 原理：更新 CR4 读影子；作用：保证来宾读取一致；结果：影子生效

	__cr4 cr4; // 原理：准备写回真实 CR4；作用：建立可修改副本；结果：cr4 可调整
	__cr_fixed cr_fixed; // 原理：CR4 固定位掩码结构；作用：应用 VMX 固定约束；结果：准备强制置/清位
	cr_fixed.all = __readmsr(IA32_VMX_CR4_FIXED0); // 参数：MSR IA32_VMX_CR4_FIXED0；结果：得到必须为 1 的位集合
	cr4.all = new_cr4.flags; // 原理：复制来宾请求值；作用：作为写回基础；结果：cr4 包含来宾位
	cr4.all |= cr_fixed.split.low; // 原理：FIXED0 标记必须为 1；作用：强制置位；结果：满足 VMX 要求
	cr_fixed.all = __readmsr(IA32_VMX_CR4_FIXED1);
	cr4.all &= cr_fixed.split.low; // 原理：FIXED1 中为 0 的位必须清零；作用：强制清位；结果：满足 VMX 要求
	hv::vmwrite<unsigned __int64>(GUEST_CR4, cr4.all); // 原理：写回 VMCS 的 GUEST_CR4；作用：让来宾真实 CR4 生效；结果：硬件按新 CR4 行为运行
	return true; // 原理：执行成功到达末尾；作用：告知上层继续执行；结果：返回成功
}

//处理 CR 访问 VM-exit：解析访问类型并分发 MOV/CLTS/LMSW，同时更新寄存器与控制状态
void vmexit_cr_handler(__vcpu* vcpu) // vcpu=当前虚拟CPU状态
{
	__cr0 guest_cr0; // 原理：保留变量便于扩展；作用：与原结构保持一致；结果：当前未直接使用
	__cr3 guest_cr3; // 原理：保留变量便于扩展；作用：与原结构保持一致；结果：当前未直接使用

	vmx_exit_qualification_mov_cr qualification; // 原理：VM-exit qualification 描述 CR 访问细节；作用：用于解析访问类型/寄存器；结果：结构体待填充
	qualification.flags = vcpu->vmexit_info.qualification; // 原理：VMCS 提供退出资格字段；作用：填充解析结构；结果：qualification 可用

	// 原理：使用 union 复用同一段 64 位数据；作用：按 CR0/CR3/CR4 视图访问源操作数；结果：便于分发处理
	union
	{
		__cr0 cr0;
		__cr3 cr3;
		__cr4 cr4;
		unsigned __int64 all;
	}cr_registers;

	cr_registers.all = hv::read_guest_gpr(vcpu->vmexit_info.guest_registers, // 参数1：来宾寄存器快照地址
		qualification.general_purpose_register); // 参数2：通用寄存器编号；结果：读出源寄存器值
	switch (qualification.access_type) // 原理：不同访问类型代表不同指令语义；作用：按类型分发；结果：进入对应分支
	{
	case CR_ACCESS_MOV_TO_CR: // 原理：MOV 到控制寄存器；作用：写入 CRx；结果：调用相应处理函数
	{
		switch (qualification.control_register)
		{
		case 0: // 目标 CR0
		{
			if (mov_to_cr0_handler(vcpu, cr_registers.cr0.all) == false) // 参数1：vcpu 状态；参数2：新 CR0 原始值；结果：失败则停止
			{
				return;
			}
			break;
		}
		case 3: // 目标 CR3
		{
			if (mov_to_cr3_handler(vcpu, cr_registers.cr3.all) == false) // 参数1：vcpu 状态；参数2：新 CR3 原始值；结果：失败则停止
			{
				return;
			}
			break;
		}
		case 4: // 目标 CR4
		{
			if (mov_to_cr4_handler(cr_registers.cr4.all) == false) // 参数：新 CR4 原始值；结果：失败则停止
			{
				return;
			}
			break;
		}
		}

		break;
	}
	case CR_ACCESS_MOV_FROM_CR: // 原理：MOV 从控制寄存器；作用：把 CRx 读到通用寄存器；结果：完成寄存器写回
	{
		switch (qualification.control_register)
		{
		case 3: // 读取 CR3
		{
			hv::write_guest_gpr(vcpu->vmexit_info.guest_registers, // 参数1：来宾寄存器快照地址
				qualification.general_purpose_register, // 参数2：目标通用寄存器编号
				hv::vmread(GUEST_CR3)); // 参数3：写入值=当前 GUEST_CR3
			break; // 原理：CR3 值已写入通用寄存器；作用：结束该分支；结果：返回上层
		}
		}
		break;
	}
	case CR_ACCESS_CLTS: // 原理：CLTS 清除 CR0.TS；作用：允许执行 FPU 指令；结果：更新影子与真实 CR0
	{
		hv::vmwrite<unsigned __int64>(CR0_READ_SHADOW, // 参数1：写入字段=CR0 影子
			hv::vmread(CR0_READ_SHADOW) & ~CR0_TASK_SWITCHED_FLAG); // 参数2：清除 TS 位后的值

		hv::vmwrite<unsigned __int64>(GUEST_CR0, // 参数1：写入字段=真实 CR0
			hv::vmread(GUEST_CR0) & ~CR0_TASK_SWITCHED_FLAG); // 参数2：清除 TS 位后的值

		break; // 原理：CLTS 处理完成；作用：结束分支；结果：进入统一收尾
	}

	case CR_ACCESS_LMSW: // 原理：LMSW 修改 CR0 低 4 位；作用：调用软件模拟；结果：影子与真实 CR0 同步
	{
		emulate_lmsw(vcpu, qualification.lmsw_source_data); // 参数1：vcpu 状态；参数2：LMSW 的源数据；结果：更新 CR0 低位
		break; // 原理：LMSW 处理完成；作用：结束分支；结果：进入统一收尾
	}
	}

	set_hide_vm_exit_overhead(vcpu, true);
	adjust_rip(vcpu);
}

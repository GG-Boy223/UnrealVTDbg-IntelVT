#include "VmExitMsr.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../hypervisor_routines.h"
#include "../Vmcs/VmcsApi.h"
#include "../interrupt.h"
#include "../AsmCallset.h"
#include "../msr.h"
#include "VmExitCommon.h"
#include "../../Common/Ring0/Hvm/DebuggerVisibilityPolicy.h"

namespace
{
	bool is_valid_pat_value(unsigned __int64 value)
	{
		for (unsigned int index = 0; index < 8; ++index)
		{
			const unsigned __int8 type = static_cast<unsigned __int8>(value >> (index * 8));
			if (type != MEMORY_TYPE_UNCACHEABLE &&
				type != MEMORY_TYPE_WRITE_COMBINING &&
				type != MEMORY_TYPE_WRITE_THROUGH &&
				type != MEMORY_TYPE_WRITE_PROTECTED &&
				type != MEMORY_TYPE_WRITE_BACK &&
				type != MEMORY_TYPE_UNCACHEABLE_MINUS)
			{
				return false;
			}
		}

		return true;
	}

	bool is_valid_guest_efer_value(unsigned __int64 value)
	{
		constexpr unsigned __int64 supported_bits =
			(1ull << 0) |  // SCE
			(1ull << 8) |  // LME
			(1ull << 10) | // LMA
			(1ull << 11);  // NXE
		return (value & ~supported_bits) == 0;
	}

	bool is_valid_mtrr_type(unsigned __int64 value)
	{
		return value < 8 && ((1ull << value) & 0x73ull) != 0;
	}

	bool is_fixed_mtrr_msr(unsigned __int32 msr_index)
	{
		return msr_index == IA32_MTRR_FIX64K_00000 ||
			msr_index == IA32_MTRR_FIX16K_80000 ||
			msr_index == IA32_MTRR_FIX16K_A0000 ||
			(msr_index >= IA32_MTRR_FIX4K_C0000 && msr_index <= IA32_MTRR_FIX4K_F8000);
	}

	bool is_variable_mtrr_msr(__guest_mtrr_state const& state, unsigned __int32 msr_index)
	{
		if (msr_index < IA32_MTRR_PHYSBASE0)
		{
			return false;
		}

		// Only the pairs reported by IA32_MTRRCAP[7:0] are variable MTRRs.
		// The same numeric window contains architectural PMU MSRs (0x38E+).
		unsigned __int32 const index = msr_index - IA32_MTRR_PHYSBASE0;
		unsigned __int32 const variable_range_count = static_cast<unsigned __int32>(state.cap & 0xff);
		return index < variable_range_count * 2;
	}

	bool try_read_guest_mtrr_msr(__vcpu const* vcpu, unsigned __int32 msr_index,
		unsigned __int64& value, bool& fault)
	{
		__guest_mtrr_state const& state = vcpu->guest_mtrr_state;
		fault = false;

		if (msr_index == IA32_MTRRCAP)
		{
			value = state.cap;
			return true;
		}

		if (msr_index == IA32_MTRR_DEF_TYPE)
		{
			value = state.def_type;
			return true;
		}

		if (is_fixed_mtrr_msr(msr_index))
		{
			if (msr_index == IA32_MTRR_FIX64K_00000)
			{
				value = state.fixed_64k;
			}
			else if (msr_index == IA32_MTRR_FIX16K_80000 || msr_index == IA32_MTRR_FIX16K_A0000)
			{
				value = state.fixed_16k[msr_index - IA32_MTRR_FIX16K_80000];
			}
			else
			{
				value = state.fixed_4k[msr_index - IA32_MTRR_FIX4K_C0000];
			}
			return true;
		}

		if (is_variable_mtrr_msr(state, msr_index))
		{
			unsigned __int32 const index = msr_index - IA32_MTRR_PHYSBASE0;
			value = state.variable[index];
			return true;
		}

		return false;
	}

	bool try_write_guest_mtrr_msr(__vcpu* vcpu, unsigned __int32 msr_index,
		unsigned __int64 value, bool& fault)
	{
		__guest_mtrr_state& state = vcpu->guest_mtrr_state;
		fault = false;

		if (msr_index == IA32_MTRRCAP)
		{
			fault = true; // IA32_MTRRCAP 是只读 MSR
			return true;
		}

		if (msr_index == IA32_MTRR_DEF_TYPE)
		{
			if ((value & ~0xcffull) != 0 || !is_valid_mtrr_type(value & 0xff))
			{
				fault = true;
				return true;
			}

			state.def_type = value;
			return true;
		}

		if (is_fixed_mtrr_msr(msr_index))
		{
			for (unsigned int index = 0; index < 8; ++index)
			{
				if (!is_valid_mtrr_type((value >> (index * 8)) & 0xff))
				{
					fault = true;
					return true;
				}
			}

			if (msr_index == IA32_MTRR_FIX64K_00000)
			{
				state.fixed_64k = value;
			}
			else if (msr_index == IA32_MTRR_FIX16K_80000 || msr_index == IA32_MTRR_FIX16K_A0000)
			{
				state.fixed_16k[msr_index - IA32_MTRR_FIX16K_80000] = value;
			}
			else
			{
				state.fixed_4k[msr_index - IA32_MTRR_FIX4K_C0000] = value;
			}
			return true;
		}

		if (is_variable_mtrr_msr(state, msr_index))
		{
			unsigned __int32 const index = msr_index - IA32_MTRR_PHYSBASE0;
			unsigned __int32 const max_physical_address_bits = vcpu->max_physical_address_bits;
			unsigned __int64 const address_mask = max_physical_address_bits >= 64
				? MAXULONG64
				: ((1ull << max_physical_address_bits) - 1);

			if ((msr_index & 1) == 0)
			{
				if (!is_valid_mtrr_type(value & 0xff) ||
					(value & 0xf00ull) != 0 ||
					(value & ~address_mask) != 0)
				{
					fault = true;
					return true;
				}
			}
			else
			{
				if ((value & 0x7ffull) != 0 || (value & ~address_mask) != 0)
				{
					fault = true;
					return true;
				}
			}

			state.variable[index] = value;
			return true;
		}

		return false;
	}
}

// 处理 RDMSR 指令引发的 VM-exit，根据需要读取或伪装 MSR，最后返回到 EDX:EAX
// /*当“use MSR bitmap”为 1 时，使用 RDMSR 指令读 MSR，但 ECX 寄存器提
// 供的 MSR 地址值不在 00000000H～00001FFFH 或者 C0000000H～C0001FFFH 范围内，
// 将导致 VM - exit。
// 如在 00000000H～00001FFFH 或者 C0000000H～C0001FFFH 范围内，则由MSR bitmap区域来决定
// 是否触发vmexit
// 将 ECX 指定的 MSR 读入 EDX:EAX。
// */
// /*当"use MSR bitmap"为 1 时，使用 RDMSR 指令读 MSR，但 ECX 寄存器提
// 将导致 VM-exit。
// 如在 00000000H～00001FFFH 或者 C0000000H～C0001FFFH 范围内，则由 MSR bitmap 区域来决定
// 是否触发 vmexit
//处理 RDMSR 指令：按来宾请求读取 MSR，必要时伪造特性并安全返回到 EDX:EAX
void vmexit_msr_read_handler(__vcpu* vcpu) // vcpu=当前虚拟CPU状态
{
	__msr msr; // 原理：MSR 由高/低 32 位组成；作用：使用联合体承载读取结果；结果：msr 可拆分为 high/low
	uint32_t msr_index = static_cast<uint32_t>(vcpu->vmexit_info.guest_registers->rcx); // 原理：RDMSR 使用 ECX 指定 MSR；作用：取低 32 位作为编号；结果：msr_index 得到目标 MSR

	// CPUID.01H:ECX.VMX 已向 guest 显示为 0 时，VMX capability MSR 也必须按
	// “处理器不支持 VMX” 的架构语义注入 #GP，不能把真实硬件能力暴露出去。
	if (!g_vmm_context.hv_presence &&
		hvm_debug_visibility::IsVmxCapabilityMsr(msr_index))
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
		return;
	}

	switch (msr_index) // 原理：对关键 MSR 做特殊伪装/过滤；作用：分发读取逻辑；结果：生成返回值
	{
	case IA32_PAT:
	{
		msr.all = hv::vmread(GUEST_PAT); // Guest PAT 由 VMCS 持有，VM-exit 后硬件 PAT 已是 Host PAT。
		break;
	}
	case IA32_EFER:
	{
		// GUEST_EFER 由 VMCS 保存,读取该值才能保证 guest 看到的 EFER 与 VMCS 状态一致。
		msr.all = hv::vmread(GUEST_EFER);
		break;
	}

	case IA32_FEATURE_CONTROL:
	{
		__ia32_feature_control_msr feature_msr = { 0 }; // 原理：该 MSR 控制 VMX/SMX 能力；作用：构造可控返回值；结果：feature_msr 就绪
		feature_msr.all = __readmsr(IA32_FEATURE_CONTROL); // 原理：读取真实硬件值作为基线；作用：获取当前配置；结果：feature_msr 含真实内容
		feature_msr.lock = 1; // 原理：锁定位表示 BIOS 配置已锁定；作用：保持一致性；结果：来宾看到锁定状态
		feature_msr.vmxon_inside_smx = 0; // 原理：禁止 SMX 内 VMXON；作用：隐藏相关能力；结果：来宾认为不支持
		feature_msr.vmxon_outside_smx = 0; // 原理：禁止 SMX 外 VMXON；作用：伪装 BIOS 未启用 VT；结果：来宾认为 VMX 不可用
		feature_msr.senter_local = 0; // 原理：SENTER 相关能力可被隐藏；作用：禁用本地 SENTER；结果：来宾认为不支持
		feature_msr.senter_global = 0; // 原理：SENTER 相关能力可被隐藏；作用：禁用全局 SENTER；结果：来宾认为不支持
		msr.all = feature_msr.all; // 原理：将伪造后的值作为返回；作用：提供给来宾；结果：msr 含目标返回值
		break; // 原理：该 MSR 已处理完成；作用：跳出 switch；结果：进入返回路径
	}
	case IA32_PERF_GLOBAL_CTRL:
	{
		// VM-entry 的 load IA32_PERF_GLOBAL_CTRL（entry control bit 13）会从
		// GUEST_PERF_GLOBAL_CONTROL 装载物理 MSR；VM-exit 又用 HOST_PERF_GLOBAL_CTRL
		// 覆盖它，因此 root 里的物理 MSR 永远不是 guest 值。
		// guest 的 RDMSR 只能返回 VMCS 影子值（见同文件 WRMSR 分支）。
		msr.all = hv::vmread(GUEST_PERF_GLOBAL_CONTROL);
		break;
	}
	default:
	{
		unsigned __int64 guest_mtrr_value = 0;
		bool mtrr_fault = false;
		if (try_read_guest_mtrr_msr(vcpu, msr_index, guest_mtrr_value, mtrr_fault))
		{
			if (mtrr_fault)
			{
				hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
					INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
				return;
			}

			msr.all = guest_mtrr_value;
			break;
		}

		// hv::host_exception_info e; // 旧逻辑：未清零会让safe RDMSR异常状态受旧栈内容影响。
		hv::host_exception_info e = { 0 }; // safe RDMSR通过Host IDT写回异常状态，必须先清零避免读取旧栈值。
		msr.all = hv::rdmsr_safe(e, msr_index); // 参数1：异常信息结构；参数2：MSR 编号；结果：安全读取 MSR 值

		if (e.exception_occurred) // 原理：读取非法 MSR 会触发异常；作用：检测读取是否失败；结果：失败则注入 #GP
		{
			hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			return;
		}
		break;
	}
	}

	vcpu->vmexit_info.guest_registers->edx = msr.high; // 原理：RDMSR 规定 EDX=高 32 位；作用：回写高位；结果：来宾 EDX 得到 MSR 高半部分
	vcpu->vmexit_info.guest_registers->eax = msr.low; // 原理：RDMSR 规定 EAX=低 32 位；作用：回写低位；结果：来宾 EAX 得到 MSR 低半部分

	set_hide_vm_exit_overhead(vcpu, true); // 参数1：vcpu 状态；参数2：开启隐藏；原理：标记需要补偿 VM-exit 开销；结果：为后续处理提供标记
	adjust_rip(vcpu);
}

// 处理 WRMSR 指令引发的 VM-exit，安全写入到 MSR，涉及 MTRR 时同步 EPT 内存类型
//处理 WRMSR 指令：将来宾写入的 MSR 值安全写到硬件，并在涉及 MTRR 时同步 EPT 内存类型
void vmexit_msr_write_handler(__vcpu* vcpu) // vcpu=当前虚拟CPU状态
{
	sLog("\n"); // 原理：记录日志用于调试；作用：标记该 VM-exit 分支被触发；结果：不改变来宾状态
	uint32_t msr_index = static_cast<uint32_t>(vcpu->vmexit_info.guest_registers->rcx); // 原理：WRMSR 使用 ECX 指定 MSR；作用：取低 32 位作为编号；结果：msr_index 得到目标 MSR

	__msr msr; // 原理：MSR 由高/低 32 位组成；作用：使用联合体拼接 64 位；结果：msr 可同时表示高/低与整体值
	msr.high = vcpu->vmexit_info.guest_registers->edx; // 原理：WRMSR 规定 EDX=高 32 位；作用：填充高位；结果：msr.high 就绪
	msr.low = vcpu->vmexit_info.guest_registers->eax; // 原理：WRMSR 规定 EAX=低 32 位；作用：填充低位；结果：msr.low 就绪

	if (msr_index == IA32_PAT)
	{
		if (!is_valid_pat_value(msr.all))
		{
			hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			return;
		}

		hv::vmwrite<unsigned __int64>(GUEST_PAT, msr.all); // VM-entry 从该字段加载 Guest PAT，不污染 VMX Root PAT。
		set_hide_vm_exit_overhead(vcpu, true);
		adjust_rip(vcpu);
		return;
	}
	if (msr_index == IA32_EFER)
	{
		if (!is_valid_guest_efer_value(msr.all))
		{
			hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			return;
		}

		__ia32_efer_t current_efer = { 0 };
		__ia32_efer_t requested_efer = { 0 };
		current_efer.all = hv::vmread(GUEST_EFER);
		requested_efer.all = msr.all;

		// WRMSR 对 LMA 的修改被硬件忽略，保留当前 LMA。
		requested_efer.long_mode_active = current_efer.long_mode_active;

		auto const current_cr0 = hv::read_effective_guest_cr0();
		if (current_cr0.paging_enable &&
			requested_efer.long_mode_enable != current_efer.long_mode_enable)
		{
			hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			return;
		}

		bool const execute_disable_changed =
			requested_efer.execute_disable != current_efer.execute_disable;
		hv::vmwrite<unsigned __int64>(GUEST_EFER, requested_efer.all);
		if (execute_disable_changed)
		{
			invvpid_single_context_func(guest_vpid);
		}

		set_hide_vm_exit_overhead(vcpu, true);
		adjust_rip(vcpu);
		return;
	}
	if (msr_index == IA32_PERF_GLOBAL_CTRL)
	{
		// 该 MSR 的 guest 影子存放在 VMCS（entry control bit 13 会装载它）。
		// 先在 root 执行一次 WRMSR 让硬件校验取值：计数器数量以外的保留位
		// 会触发 #GP，必须按架构语义注入给 guest，而不是写进 VMCS 后
		// 在 VM-entry 才以 invalid-guest-state 方式失败（那会变成宿主 BugCheck）。
		hv::host_exception_info e = { 0 };
		hv::wrmsr_safe(e, msr_index, msr.all);
		if (e.exception_occurred)
		{
			hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			return;
		}

		// VM-exit 时物理 MSR 已被 HOST_PERF_GLOBAL_CTRL 覆盖，guest 写入值
		// 必须落在 VMCS 来宾字段；同时设置 exit control 的 save 位
		//（见 Vmcs/VmcsControls.cpp），两条路径共同保证跨 VM-exit 一致。
		hv::vmwrite<unsigned __int64>(GUEST_PERF_GLOBAL_CONTROL, msr.all);
		set_hide_vm_exit_overhead(vcpu, true);
		adjust_rip(vcpu);
		return;
	}

	bool mtrr_fault = false;
	if (try_write_guest_mtrr_msr(vcpu, msr_index, msr.all, mtrr_fault))
	{
		if (mtrr_fault)
		{
			hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			return;
		}

		set_hide_vm_exit_overhead(vcpu, true);
		adjust_rip(vcpu);
		return;
	}

	// hv::host_exception_info e; // 旧逻辑：未清零会让safe WRMSR异常状态受旧栈内容影响。
	hv::host_exception_info e = { 0 }; // safe WRMSR失败时向guest注入#GP，不让Host异常扩散到未处理路径。
	hv::wrmsr_safe(e, msr_index, msr.all); // 参数1：异常信息结构；参数2：MSR 编号；参数3：写入值；结果：安全尝试写入 MSR

	if (e.exception_occurred) // 原理：写入非法 MSR 会触发异常；作用：检测写入是否失败；结果：失败则注入 #GP
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
		return;
	}

	set_hide_vm_exit_overhead(vcpu, true);
	adjust_rip(vcpu);
}

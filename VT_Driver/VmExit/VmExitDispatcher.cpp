#include "VmExitDispatcher.h"

namespace
{
	constexpr unsigned __int64 rflags_carry_flag = 1ull << 0;
	constexpr unsigned __int64 rflags_zero_flag = 1ull << 6;

	enum vmresume_vmread_failure_bits : unsigned __int64
	{
		vmresume_vmread_failed_instruction_error = 1ull << 0,
		vmresume_vmread_failed_exit_reason = 1ull << 1,
		vmresume_vmread_failed_exit_qualification = 1ull << 2,
		vmresume_vmread_failed_guest_rip = 1ull << 3,
		vmresume_vmread_failed_guest_rsp = 1ull << 4,
		vmresume_vmread_failed_guest_cr3 = 1ull << 5,
	};

	unsigned __int64 read_vmresume_snapshot_field(unsigned __int64 const field, unsigned __int64 const failure_bit, unsigned __int64* const failure_mask)
	{
		unsigned __int64 value = 0;
		if (__vmx_vmread(field, &value) != 0)
		{
			*failure_mask |= failure_bit; // VMfailInvalid时当前VMCS可能不可读，记录失败位而不是使用未初始化值。
			return 0;
		}

		return value;
	}

	__vcpu* current_vcpu_from_host_fs()
	{
		if (g_vmm_context.vcpu == nullptr || g_vmm_context.processor_count == 0)
		{
			return nullptr;
		}

		auto const begin = reinterpret_cast<unsigned __int64>(g_vmm_context.vcpu);
		auto const size = static_cast<unsigned __int64>(sizeof(__vcpu)) * g_vmm_context.processor_count;
		auto const candidate = _readfsbase_u64();
		if (candidate < begin || candidate - begin >= size ||
			(candidate - begin) % sizeof(__vcpu) != 0)
		{
			return nullptr;
		}

		return reinterpret_cast<__vcpu*>(candidate);
	}

	void clear_vm_entry_event_fields() // 清空 VM-entry 注入字段，保证每次 VM-exit 重新决定是否投递事件。
	{
		// 清空 VM-entry 中断注入字段
		hv::vmwrite<unsigned __int64>(VM_ENTRY_INTERRUPTION_INFO_FIELD, 0); // 清除 pending event 标志，避免沿用上一次注入请求。
		hv::vmwrite<unsigned __int64>(VM_ENTRY_EXCEPTION_ERROR_CODE, 0); // 清除异常错误码，避免无错误码异常继承旧值。
		hv::vmwrite<unsigned __int64>(VM_ENTRY_INSTRUCTION_LENGTH, 0); // 清除软件中断指令长度，后续注入路径按需重新写入。
	}
}

EXTERN_C
void record_vmresume_failure(unsigned __int64 const rflags) // 只写内存快照，禁止在VM-exit路径内做文件IO或调用复杂内核API。
{
	__vcpu* const vcpu = current_vcpu_from_host_fs(); // HOST_FS_BASE保存当前CPU的VCPU指针，避免在VMX root路径查询处理器号。
	if (vcpu == nullptr)
	{
		return; // FS Base异常时不能安全写诊断结构，直接走后续VMXOFF路径。
	}

	auto& snapshot = vcpu->vmresume_failure_snapshot;
	unsigned __int64 vmread_failure_mask = 0;

	++snapshot.count; // 失败路径单CPU写入，不做锁；卸载/回滚阶段再读取输出。
	snapshot.rflags = rflags;
	snapshot.vmfail_valid = ((rflags & rflags_zero_flag) != 0 && (rflags & rflags_carry_flag) == 0) ? 1 : 0;
	snapshot.vmfail_invalid = ((rflags & rflags_carry_flag) != 0) ? 1 : 0;
	snapshot.vm_instruction_error = snapshot.vmfail_valid
		? read_vmresume_snapshot_field(VM_INSTRUCTION_ERROR, vmresume_vmread_failed_instruction_error, &vmread_failure_mask)
		: 0; // 旧逻辑：VMRESUME失败后直接落入VMXOFF，没有保存VM_INSTRUCTION_ERROR。
	snapshot.vm_exit_reason = read_vmresume_snapshot_field(VM_EXIT_REASON, vmresume_vmread_failed_exit_reason, &vmread_failure_mask);
	snapshot.exit_qualification = read_vmresume_snapshot_field(EXIT_QUALIFICATION, vmresume_vmread_failed_exit_qualification, &vmread_failure_mask);
	snapshot.guest_rip = read_vmresume_snapshot_field(GUEST_RIP, vmresume_vmread_failed_guest_rip, &vmread_failure_mask);
	snapshot.guest_rsp = read_vmresume_snapshot_field(GUEST_RSP, vmresume_vmread_failed_guest_rsp, &vmread_failure_mask);
	snapshot.guest_cr3 = read_vmresume_snapshot_field(GUEST_CR3, vmresume_vmread_failed_guest_cr3, &vmread_failure_mask);
	snapshot.vmread_failure_mask = vmread_failure_mask;
}

EXTERN_C
unsigned __int64 handle_vmresume_failure(unsigned __int64 const rflags)
{
	__vcpu* const vcpu = current_vcpu_from_host_fs();
	if (vcpu == nullptr)
	{
		vmx_fatal_error(vmx_fatal_missing_vcpu, rflags, 0);
	}

	record_vmresume_failure(rflags);
	vcpu->vmx_off_state.vmx_off_executed = true;
	if (!PrepareGuestForVmxoff(vcpu))
	{
		vmx_fatal_error(
			vmx_fatal_vmresume_restore,
			rflags,
			vcpu->vmresume_failure_snapshot.vmread_failure_mask);
	}

	vcpu->vcpu_status.vmm_launched = false;
	return reinterpret_cast<unsigned __int64>(&vcpu->vmx_off_state);
}

//注意：在 Host 上永远禁止直接调用 Windows API，因为 Host 处于 VMX 非根路径上，任何段检查都可能导致不稳定
//原因：很多 Windows API 依赖 IRQL、栈边界（stack base limit）、其他约束，Host 在 VM-exit 期间不一定满足这些约束
//如果一项调用失败，系统可能直接触发 BugCheck（BSOD）；因此像 ASSERT/DbgBreakPoint 等调试函数也不应该使用
//原因：很多 Windows API 会检查 IRQL、栈边界（stack base limit）等条件，Host 在 VM-exit 期间不一定满足这些约束
//后果：一旦检查失败，系统可能直接触发 BugCheck（BSOD），因此像 ASSERT/DbgBreakPoint 等调试函数也不应在这里使用
EXTERN_C // 使用 C 链接方式导出函数名，避免 C++ 名字修饰影响汇编入口或链接匹配
bool vmexit_handler(guest_context* guest_registers, PFXSAVE64 fxsave) // VM-exit 总入口：采集 guest 状态并分派到具体 exit handler。
{
	//从 fs_base 获取到 vcpu 的指针，因为 Host 已经将每个 CPU 的 vCPU 指针到 FS 基址
	//硬件原理：FS Base 是每个 CPU 可独立维护的寄存器，适合用于快速访问 CPU 相关数据（类似 TLS）结构
	//从 fs_base 里取出 vcpu 的指针，因为 Host 保存了每个 CPU 的 vCPU 指针到 FS 基址
	//硬件原理：FS Base 是每个 CPU 可独立维护的寄存器，适合作为快速访问 CPU 本地数据（类似 TLS）的入口
	__vcpu* vcpu = reinterpret_cast<__vcpu*>(_readfsbase_u64()); // 当前 CPU 的 vCPU 上下文必须来自 FS Base，避免调用内核 API 查询处理器号。
	if (vcpu == nullptr || vcpu != current_vcpu_from_host_fs())
	{
		vmx_fatal_error(vmx_fatal_missing_vcpu, 0, 0);
	}

	guest_registers->rsp = hv::vmread(GUEST_RSP); // 汇编入口保存通用寄存器后，这里补齐 VMCS 中的 guest RSP。

	vcpu->vmexit_info.reason = hv::vmread(VM_EXIT_REASON) & 0xffff; // 低 16 位是基础 exit reason，高位状态不参与 switch 分发。
	vcpu->vmexit_info.qualification = hv::vmread(EXIT_QUALIFICATION); // 保存 exit qualification，供 CR/DR/EPT 等 handler 解码。
	vcpu->vmexit_info.guest_rflags.all = hv::vmread(GUEST_RFLAGS); // 保存 RFLAGS，后续单步、IF/TF 状态都依赖该快照。
	vcpu->vmexit_info.guest_rip = hv::vmread(GUEST_RIP); // 保存触发 VM-exit 的 guest RIP，供模拟执行或日志定位。
	vcpu->vmexit_info.instruction_length = hv::vmread(VM_EXIT_INSTRUCTION_LENGTH); // 保存退出指令长度，adjust_rip/skip_instruction 需要使用。
	vcpu->vmexit_info.instruction_information = hv::vmread(VM_EXIT_INSTRUCTION_INFORMATION); // 保存 VMX 指令解码信息，供部分 exit handler 使用。
	vcpu->vmexit_info.guest_registers = guest_registers; // 暴露通用寄存器保存区，handler 可读写模拟后的寄存器值。
	vcpu->vmexit_info.fxsave = fxsave; // 暴露浮点/SIMD 保存区，保留给需要处理 FXSAVE 状态的路径。

	vcpu->hide_vm_exit_overhead = false; // 每次退出默认不隐藏开销，只有 CPUID/RDTSC 等时间敏感路径主动开启。

	//
	//采用函数棒（函数指针表）的设计无法满足需求，因为无法很好地处理未知/未来的退出原因
	//改用 switch：可以把未收录的 reason 统一落到 default 做保护性处理
	dispatch_vm_exit(vcpu); // 根据 exit reason 执行模拟、注入异常、EPT 切换或 VMCALL 服务。

	if (vcpu->vmx_off_state.vmx_off_executed) // VMCALL/退出流程要求关闭 VMX 时，由汇编上层执行最终 VMXOFF。
	{
		if (!PrepareGuestForVmxoff(vcpu))
		{
			vmx_fatal_error(vmx_fatal_vmresume_restore, vcpu->vmexit_info.reason, 0);
		}

		vcpu->vmexit_info.guest_registers->rsp =
			reinterpret_cast<unsigned __int64>(&vcpu->vmx_off_state); // 汇编复用guest_context的RSP占位槽取得受检恢复帧。
		vcpu->vcpu_status.vmm_launched = false; // 标记当前 vCPU 不再处于已 launch 状态，防止后续误用 VMRESUME。
		//如果要停止虚拟化，vmexit_handler 将返回 true，通知上层跳出 VMX 处理循环
		return true; // 通知汇编入口走退出虚拟化路径，而不是继续 VMRESUME。
	}

	return false; // 常规 VM-exit 处理完成，继续 VMRESUME 返回 guest。
}

void dispatch_vm_exit(__vcpu* vcpu) // 按 VM-exit 原因码分发，保持所有事件注入策略集中在进入 switch 前处理。
{
	// Reset VM-entry injection fields for this VM-exit. New handlers set them again before VMRESUME.
	//hv::vmwrite<unsigned __int64>(VM_ENTRY_INTERRUPTION_INFO_FIELD, 0);
	//hv::vmwrite<unsigned __int64>(VM_ENTRY_EXCEPTION_ERROR_CODE, 0);
	//hv::vmwrite<unsigned __int64>(VM_ENTRY_INSTRUCTION_LENGTH, 0);
	clear_vm_entry_event_fields(); // 先清理上一轮 VM-entry 注入状态，后续 handler 再按当前 exit 决定是否写入。
	if (vcpu->vmexit_info.reason != VMX_EXIT_REASON_EXCEPTION_OR_NMI) // Exception/NMI handler 会直接读取 exit interruption 信息，其他 exit 需先处理 IDT-vectoring。
	{
		reinject_idt_vectoring_event_if_needed(vcpu); // VM-exit 打断事件投递时重注入，避免 guest 异常/中断丢失。
	}

	switch (vcpu->vmexit_info.reason) // 只按基础 reason 分发，具体访问方向/寄存器编号由 handler 解析 qualification。
	{
	case VMX_EXIT_REASON_EXCEPTION_OR_NMI:             vmexit_exception_or_nmi_handler(vcpu);     break; // 异常/NMI 退出：区分 NMI 队列和可重注入异常。
	case VMX_EXIT_REASON_INIT_SIGNAL:                  vmexit_init_signal_handler(vcpu);          break; // INIT 信号退出：恢复 guest activity state。
	case VMX_EXIT_REASON_STARTUP_IPI:                  vmexit_sipi_handler(vcpu);                 break; // SIPI 退出：处理 AP 启动相关状态。
	case VMX_EXIT_REASON_EXECUTE_GETSEC:               vmexit_getsec_handler(vcpu);               break; // GETSEC 不允许透传，按硬件约束注入 #GP。
	case VMX_EXIT_REASON_EXECUTE_INVD:                 vmexit_invd_handler(vcpu);                 break; // INVD 退出：Host 执行等价缓存失效后推进 RIP。
	case EXIT_REASON_WBINVD:                           vmexit_invd_handler(vcpu);                 break; // WBINVD 已被次级控制拦截，必须执行等价写回/失效并推进 RIP。
	case VMX_EXIT_REASON_EXECUTE_INVLPG:               vmexit_invlpg_handler(vcpu);               break; // INVLPG 退出：同步 VPID 单地址失效再推进 RIP。
	case VMX_EXIT_REASON_NMI_WINDOW:                   vmexit_nmi_window_handler(vcpu);           break; // NMI-window：仅在无 pending event 时投递排队 NMI。
	case VMX_EXIT_REASON_EXECUTE_CPUID:                vmexit_cpuid_handler(vcpu);                break; // CPUID 退出：执行宿主 CPUID 并写回 guest 寄存器。
	case VMX_EXIT_REASON_MOV_CR:                       vmexit_cr_handler(vcpu);                   break; // MOV CR 退出：维护 CR shadow 和 guest 控制寄存器语义。
	case VMX_EXIT_REASON_EXECUTE_RDMSR:                vmexit_msr_read_handler(vcpu);             break; // RDMSR 退出：按白名单/模拟路径读取 MSR。
	case VMX_EXIT_REASON_EXECUTE_WRMSR:                vmexit_msr_write_handler(vcpu);            break; // WRMSR 退出：按白名单/模拟路径写入 MSR。
	case VMX_EXIT_REASON_EXECUTE_XSETBV:               vmexit_xsetbv_handler(vcpu);               break; // XSETBV 退出：验证 XCR0 合法性后安全执行。
	case VMX_EXIT_REASON_EXECUTE_VMXON:                vmexit_vmx_on_handler(vcpu);               break; // Guest VMXON 退出：防止嵌套 VMX 破坏当前 VMM。
	case VMX_EXIT_REASON_EXECUTE_VMCALL:               vmexit_vmcall_handler(vcpu);               break; // VMCALL 退出：用户态/内核态与 VMM 的服务入口。
	case VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED: vmexit_vmx_preemption_handler(vcpu);       break; // VMX 预占计时器：保留给时间开销隐藏校准。
	case VMX_EXIT_REASON_EPT_VIOLATION:                vmexit_ept_violation_handler(vcpu);        break; // EPT violation：处理 EPT hook/watch 权限切换。
	case VMX_EXIT_REASON_EXECUTE_RDTSC:                vmexit_rdtsc_handler(vcpu);                break; // RDTSC 退出：返回校准后的时间戳寄存器。
	case VMX_EXIT_REASON_EXECUTE_RDTSCP:               vmexit_rdtscp_handler(vcpu);               break; // RDTSCP 退出：返回 TSC 和 IA32_TSC_AUX。
	case VMX_EXIT_REASON_MONITOR_TRAP_FLAG:            vmexit_monitor_trap_flag_handler(vcpu);    break; // MTF 退出：恢复一次性 EPT 权限和 watch 状态。
	case VMX_EXIT_REASON_EPT_MISCONFIGURATION:         vmexit_ept_misconfiguration_handler(vcpu); break; // EPT misconfig：记录/处理非法 EPT 项配置。
	case VMX_EXIT_REASON_MOV_DR:                       vmexit_mov_dr_handler(vcpu);               break; // MOV DR 退出：同步调试寄存器并处理非法 DR 编号。
	case EXIT_REASON_TRIPLE_FAULT:                     vmexit_triple_fault_handler(vcpu);         break; // Guest triple fault必须显式处理，避免落入默认#GP造成语义混乱。
	case EXIT_REASON_VM_ENTRY_FAILURE_INVALID_GUEST_STATE:
	case EXIT_REASON_VM_ENTRY_FAILURE_MSR_LODAING:
	case EXIT_REASON_VM_ENTRY_FAILURE_MACHINE_CHECK_EVENT:
		vmexit_failed(vcpu); // VM-entry failure是VMX硬件报告的失败路径，只记录/反馈，不改变VT进入条件。
		break;

	case VMX_EXIT_REASON_EXECUTE_VMCLEAR: // 以下 VMX 指令不允许 guest 直接执行，统一按非法指令语义处理。
	case VMX_EXIT_REASON_EXECUTE_VMLAUNCH: // VMLAUNCH
	case VMX_EXIT_REASON_EXECUTE_VMPTRLD: // VMPTRLD
	case VMX_EXIT_REASON_EXECUTE_VMPTRST: // VMPTRST
	case VMX_EXIT_REASON_EXECUTE_VMREAD: // VMREAD
	case VMX_EXIT_REASON_EXECUTE_VMRESUME: // VMRESUME
	case VMX_EXIT_REASON_EXECUTE_VMWRITE: // VMWRITE
	case VMX_EXIT_REASON_EXECUTE_VMXOFF: // VMXOFF
	case VMX_EXIT_REASON_EXECUTE_INVEPT: // INVEPT
	case VMX_EXIT_REASON_EXECUTE_INVVPID: // INVVPID
	case VMX_EXIT_REASON_EXECUTE_VMFUNC:               vmexit_vm_instruction(vcpu);                break; // 注入 #UD，避免 guest 干扰 host VMCS/EPT/VPID 状态。

	default:
	{
		//其他 vmexit：统一先注入 #GP，避免来宾继续执行未知指令/状态
		sLog("reason: %d\n", vcpu->vmexit_info.reason); // 未实现 reason 必须记录，便于按硬件手册补齐新 exit。
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT, INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // 默认给 guest #GP(0)，避免静默继续导致状态错乱。
		break; // 结束 default 分支，回到 VMRESUME 路径。
	}
	}
}

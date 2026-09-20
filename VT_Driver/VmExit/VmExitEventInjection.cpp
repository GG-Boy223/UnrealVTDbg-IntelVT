#include "VmExitException.h"

namespace
{
	void write_vm_entry_event_to_vmcs(
		unsigned __int32 interrupt_info,
		unsigned __int32 instruction_length,
		unsigned __int32 error_code)
	{
		if (instruction_length != 0)
		{
			hv::vmwrite<unsigned __int64>(VM_ENTRY_INSTRUCTION_LENGTH, instruction_length);
		}

		__vmentry_interrupt_info info = { 0 };
		info.all = interrupt_info;
		if (info.deliver_error_code != 0)
		{
			hv::vmwrite<unsigned __int64>(VM_ENTRY_EXCEPTION_ERROR_CODE, error_code);
		}

		hv::vmwrite<unsigned __int64>(VM_ENTRY_INTERRUPTION_INFO_FIELD, interrupt_info);
	}

	bool vmexit_event_type_can_be_reinjected(unsigned __int32 type) // 判断 VM-exit 携带的事件类型是否可通过 VM-entry 字段重新投递。
	{
		return type == INTERRUPT_TYPE_EXTERNAL_INTERRUPT || // 外部中断可由 VM-entry interruption info 重新注入。
			type == INTERRUPT_TYPE_HARDWARE_EXCEPTION || // #PF/#GP/#DB 等硬件异常需要按原类型回灌给 guest。
			type == INTERRUPT_TYPE_SOFTWARE_INTERRUPT || // INT n 需要保留软件中断语义和指令长度。
			type == INTERRUPT_TYPE_PRIVILEGED_SOFTWARE_INTERRUPT || // INT1 等特权软件中断需要保留原始类型。
			type == INTERRUPT_TYPE_SOFTWARE_EXCEPTION; // #BP/#OF 等软件异常需要按 VMX 定义重注入。
	}

	void sync_cr2_for_page_fault(
		__vcpu* vcpu,
		unsigned __int32 vector,
		unsigned __int64 fault_address) // #PF 重注入前恢复 CR2，保证 guest 页错误处理器看到原始 fault address。
	{
		if (vector == EXCEPTION_VECTOR_PAGE_FAULT &&
			vcpu != nullptr &&
			vcpu->vmexit_info.guest_registers != nullptr)
		{
			vcpu->vmexit_info.guest_registers->cr2 = fault_address; // 汇编 VMRESUME 前会从该槽恢复 CR2。
			__writecr2(fault_address);
		}
	}
}

namespace hv
{
	void queue_pending_vm_entry_event(
		__vcpu* vcpu,
		unsigned __int32 interrupt_info,
		unsigned __int32 instruction_length,
		unsigned __int32 error_code)
	{
		if (vcpu == nullptr)
		{
			vmx_fatal_error(vmx_fatal_missing_vcpu, 0, 0);
		}

		auto& queue = vcpu->pending_vm_entry_events;
		if (queue.count >= kVmEntryEventQueueCapacity)
		{
			vmx_fatal_error(
				vmx_fatal_vm_entry_event_queue_overflow,
				interrupt_info,
				queue.count);
		}

		__pending_vm_entry_event& entry = queue.entries[queue.tail];
		entry.interrupt_info = interrupt_info;
		entry.instruction_length = instruction_length;
		entry.error_code = error_code;
		queue.tail = (queue.tail + 1) % kVmEntryEventQueueCapacity;
		++queue.count;
	}

	bool pop_pending_vm_entry_event(
		__vcpu* vcpu,
		unsigned __int32* interrupt_info,
		unsigned __int32* instruction_length,
		unsigned __int32* error_code)
	{
		if (vcpu == nullptr || interrupt_info == nullptr || instruction_length == nullptr || error_code == nullptr)
		{
			return false;
		}

		auto& queue = vcpu->pending_vm_entry_events;
		if (queue.count == 0)
		{
			return false;
		}

		__pending_vm_entry_event const& entry = queue.entries[queue.head];
		*interrupt_info = entry.interrupt_info;
		*instruction_length = entry.instruction_length;
		*error_code = entry.error_code;
		queue.head = (queue.head + 1) % kVmEntryEventQueueCapacity;
		--queue.count;
		return true;
	}

	void drain_pending_vm_entry_event(__vcpu* vcpu)
	{
		if (vcpu == nullptr || vm_entry_event_pending())
		{
			return;
		}

		unsigned __int32 interrupt_info = 0;
		unsigned __int32 instruction_length = 0;
		unsigned __int32 error_code = 0;
		if (pop_pending_vm_entry_event(vcpu, &interrupt_info, &instruction_length, &error_code))
		{
			write_vm_entry_event_to_vmcs(interrupt_info, instruction_length, error_code);
		}
	}

	void ensure_pending_vm_entry_event_progress(__vcpu* vcpu)
	{
		if (vcpu != nullptr && vcpu->pending_vm_entry_events.count != 0)
		{
			hv::set_mtf(true);
		}
	}
}

bool vm_entry_event_pending() // 查询当前 VM-entry 是否已经安排事件，防止覆盖前一个 handler 写入的注入请求。
{
	__vmentry_interrupt_info pending = { 0 }; // 解析 VM-entry interruption info 的 valid 位。
	pending.all = static_cast<unsigned __int32>(hv::vmread(VM_ENTRY_INTERRUPTION_INFO_FIELD)); // 从当前 VMCS 读取待注入事件。
	return pending.valid != 0; // valid=1 表示 VM-entry 会尝试投递事件。
}

bool reinject_idt_vectoring_event_if_needed(__vcpu* vcpu) // VM-exit 打断 IDT 投递时重新安排事件，避免 guest 丢异常/中断。
{
	if (vcpu == nullptr)
	{
		return false; // 无 vCPU 上下文时不能维护 NMI 队列或 CR2，只能放弃重注入。
	}

	__vmexit_interrupt_info vectoring = { 0 }; // IDT-vectoring 字段描述被 VM-exit 中断的事件投递。
	vectoring.all = static_cast<unsigned __int32>(hv::vmread(IDT_VECTORING_INFO_FIELD)); // 读取硬件保存的 vector/type/error-code 信息。
	if (!vectoring.valid)
	{
		return false; // 没有被打断的 IDT 投递事件，不需要做任何重注入。
	}

	if (vectoring.interruption_type == INTERRUPT_TYPE_NMI &&
		vectoring.vector == EXCEPTION_VECTOR_NMII)
	{
		if (vm_entry_event_pending())
		{
			InterlockedIncrement(&vcpu->queued_nmis); // 已有 pending event 时不能覆盖 VM-entry 字段，改用 NMI 队列延后投递。
			auto ctrl = hv::read_ctrl_proc_based(); // 读取 primary processor-based controls。
			ctrl.nmi_window_exiting = 1; // 打开 NMI-window exiting，等待 guest 可接收 NMI 时再次退出。
			hv::write_ctrl_proc_based(ctrl); // 写回控制位，使队列中的 NMI 有机会投递。
			return false; // 本次没有直接注入 NMI。
		}

		hv::inject_nmi(); // 无 pending event 时可直接通过 VM-entry 投递 NMI。
		return true; // 已成功安排 NMI 注入。
	}

	if (!vmexit_event_type_can_be_reinjected(vectoring.interruption_type))
	{
		sLog("unsupported IDT-vectoring type:%u vector:%u info:0x%X\n",
			vectoring.interruption_type,
			vectoring.vector,
			vectoring.all);
		return false; // 不支持的事件类型不强行转换，避免破坏 VMX 事件语义。
	}

	const unsigned __int32 error_code = vectoring.error_code_valid ?
		static_cast<unsigned __int32>(hv::vmread(IDT_VECTORING_ERROR_CODE)) : 0; // 只有 error_code_valid=1 时读取 IDT-vectoring 错误码。
	sync_cr2_for_page_fault(
		vcpu,
		vectoring.vector,
		vcpu->vmexit_info.guest_registers->cr2); // original #PF 是间接退出，硬件已更新 CR2。
	//	vectoring.interruption_type,
	//	error_code,
	//hv::inject_interruption(vectoring.vector,
	//	vectoring.error_code_valid != 0);
	hv::inject_interruption(vectoring.vector,
		vectoring.interruption_type,
		error_code,
		vectoring.error_code_valid != 0); // 按原 vector/type/error-code 重新安排 VM-entry 注入。
	return true; // 已成功处理 IDT-vectoring 事件。
}

void vmexit_exception_or_nmi_handler(__vcpu* vcpu) // 处理 exception-or-NMI 退出，避免把普通异常误当成 NMI。
{
	sLog("\n"); // 保留最小日志占位，便于定位该高频敏感路径是否触发。
	if (vcpu == nullptr)
	{
		return; // 防御空上下文，避免在异常路径继续访问 vCPU 状态。
	}

	__vmexit_interrupt_info exit_interrupt = { 0 }; // 解析本次 exception/NMI VM-exit 的事件描述。
	exit_interrupt.all = static_cast<unsigned __int32>(hv::vmread(VM_EXIT_INTERRUPTION_INFORMATION)); // 硬件提供 vector/type/error-code-valid。
	if (!exit_interrupt.valid)
	{
		sLog("invalid exception-or-nmi info: 0x%X\n", exit_interrupt.all);
		return; // 无效事件不能重注入，否则可能制造错误异常。
	}

	if (exit_interrupt.interruption_type == INTERRUPT_TYPE_NMI &&
		exit_interrupt.vector == EXCEPTION_VECTOR_NMII)
	{
		InterlockedIncrement(&vcpu->queued_nmis); // 真正的 NMI 进入队列，由 NMI-window handler 在可投递窗口注入。
		auto ctrl = hv::read_ctrl_proc_based(); // 读取 VM-execution control。
		ctrl.nmi_window_exiting = 1; // 请求 guest 解除 NMI blocking 后再次 VM-exit。
		hv::write_ctrl_proc_based(ctrl); // 写回控制位，保证 queued_nmis 不会长期滞留。
		return; // NMI 不走普通异常重注入路径。
	}

	if (!vmexit_event_type_can_be_reinjected(exit_interrupt.interruption_type))
	{
		sLog("unsupported exception-or-nmi type:%u vector:%u info:0x%X\n",
			exit_interrupt.interruption_type,
			exit_interrupt.vector,
			exit_interrupt.all);
		return; // 不支持类型只记录日志，不做语义不明的转换。
	}

	const unsigned __int32 error_code = exit_interrupt.error_code_valid ?
		static_cast<unsigned __int32>(hv::vmread(VM_EXIT_INTERRUPTION_ERROR_CODE)) : 0; // 仅对带错误码异常读取 VM-exit error code。
	sync_cr2_for_page_fault(
		vcpu,
		exit_interrupt.vector,
		vcpu->vmexit_info.qualification); // 直接 #PF 退出不更新 CR2，故障地址在 exit qualification。
	//	exit_interrupt.interruption_type,
	//	error_code,
	//hv::inject_interruption(exit_interrupt.vector,
	//	exit_interrupt.error_code_valid != 0);
	hv::inject_interruption(exit_interrupt.vector,
		exit_interrupt.interruption_type,
		error_code,
		exit_interrupt.error_code_valid != 0); // 按 VMX 原始事件信息安排 VM-entry 重注入。
}

#include "Driver.h"
#include "Globals.h"
#include "interrupt.h"
#include "vmcall_reason.h"
#include "EPT.h"
#include "hypervisor_routines.h"
#include "vmexit_handler.h"
#include "VmcallDispatch.h"
#include "../VmExit/VmExitException.h" // vmexit_monitor_trap_flag_handler：VMCALL 入口先收敛本核 pending MTF

namespace
{
	bool IsVmcallContextValid(__vcpu* vcpu)
	{
		return vcpu != nullptr && vcpu->vmexit_info.guest_registers != nullptr;
	}

	bool IsProjectVmcall(guest_context const* guest_registers)
	{
		return guest_registers != nullptr &&
			(guest_registers->rax == VMCALL_IDENTIFIER ||
				guest_registers->eax == VMCALL_IDENTIFIER2);
	}

	void InjectUndefinedOpcode()
	{
		hv::inject_interruption(
			EXCEPTION_VECTOR_UNDEFINED_OPCODE,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION,
			0,
			false);
	}

	void CompleteVmcall(__vcpu* vcpu, bool status)
	{
		adjust_rip(vcpu);
		vcpu->vmexit_info.guest_registers->rax = status;
	}

	VmcallDispatch::Request ReadRequest(guest_context const* guest_registers)
	{
		VmcallDispatch::Request request = {};

		if (guest_registers->eax == VMCALL_IDENTIFIER2)
		{
			request.reason = guest_registers->ecx;
			request.parameter1 = guest_registers->edx;
			return request;
		}

		request.reason = guest_registers->rcx;
		request.parameter1 = guest_registers->rdx;
		request.parameter2 = guest_registers->r8;
		request.parameter3 = guest_registers->r9;
		request.parameter4 = guest_registers->r10;
		request.parameter5 = guest_registers->r11;
		request.parameter6 = guest_registers->r12;
		request.parameter7 = guest_registers->r13;
		request.parameter8 = guest_registers->r14;
		request.parameter9 = guest_registers->r15;
		return request;
	}
}

void vmexit_vmcall_handler(__vcpu* vcpu)
{
	if (vcpu == nullptr)
	{
		return;
	}

	// 先收敛本核尚未触发的 pending MTF（修复 0x101 通路 P8）。
	// 背景：mtf_reference_count 只在 MTF VM-exit 内递减（MtfPendingState.cpp:19），
	// 而 MTF 是在「下一条指令执行之后」才触发。若来宾下一条指令恰好是 VMCALL，
	// 本次 VM-exit 的原因就是 VMCALL 而不是 MTF，pending 引用会被继续持有。
	// 此时若另一核正持 eptWatchTransaction_lock 执行删除、并在 wait_for_page_quiescence 中
	// 等待本核的 mtf_reference_count 归零，而本核又要去抢同一把事务锁 —— 双方互相等待，
	// 两核都在根态（IF=0）永久自旋 → 双核 0x101。
	// 在进入 VMCALL 分发（即获取事务锁）之前先把 pending MTF 收尾，即可打破该循环。
	if (vcpu->ept_state != nullptr && vcpu->ept_state->mtf_pending)
	{
		vmexit_monitor_trap_flag_handler(vcpu);
	}

	if (!IsVmcallContextValid(vcpu))
	{
		InjectUndefinedOpcode();
		return;
	}

	guest_context* const guest_registers = vcpu->vmexit_info.guest_registers;
	if (!IsProjectVmcall(guest_registers))
	{
		if (vcpu->ept_state != nullptr && ept::handler_vmcall_rip(*vcpu->ept_state))
		{
			return;
		}

		InjectUndefinedOpcode();
		return;
	}

	VmcallDispatch::Request const request = ReadRequest(guest_registers);
	CompleteVmcall(vcpu, VmcallDispatch::Execute(vcpu, request));
}

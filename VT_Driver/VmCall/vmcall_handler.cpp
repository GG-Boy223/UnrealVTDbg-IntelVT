#include "Driver.h"
#include "Globals.h"
#include "interrupt.h"
#include "vmcall_reason.h"
#include "EPT.h"
#include "hypervisor_routines.h"
#include "vmexit_handler.h"
#include "VmcallDispatch.h"

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

#include "VmExitControl.h"

void vmexit_mov_dr_handler(__vcpu* vcpu)
{
	__exit_qualification_dr qualification = { vcpu->vmexit_info.qualification };
	unsigned const debugRegister = (unsigned)qualification.debug_register_number;
	unsigned const generalRegister = (unsigned)qualification.gp_register;

	if (qualification.access_direction == 0)
	{
		uint64_t const value = hv::read_guest_gpr(
			vcpu->vmexit_info.guest_registers,
			generalRegister);

		switch (debugRegister)
		{
		case 0:
			__writedr(0, value);
			break;
		case 1:
			__writedr(1, value);
			break;
		case 2:
			__writedr(2, value);
			break;
		case 3:
			__writedr(3, value);
			break;
		case 6:
		{
			__dr6 dr6;
			dr6.all = value;
			dr6.reserved_1 = 0xFF;
			dr6.reserved_2 = 0;
			dr6.reserved_3 = 0x7FFF;
			__writedr(6, dr6.all);
			break;
		}
		case 7:
		{
			__dr7 dr7;
			dr7.all = value;
			dr7.reserved_1 = 1;
			dr7.reserved_2 = 0;
			dr7.reserved_3 = 0;
			__writedr(7, dr7.all);
			hv::vmwrite(GUEST_DR7, dr7.all);
			break;
		}
		default:
			hv::inject_interruption(
				EXCEPTION_VECTOR_UNDEFINED_OPCODE,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION,
				0,
				false);
			return;
		}
	}
	else
	{
		uint64_t value = 0;
		switch (debugRegister)
		{
		case 0:
			value = __readdr(0);
			break;
		case 1:
			value = __readdr(1);
			break;
		case 2:
			value = __readdr(2);
			break;
		case 3:
			value = __readdr(3);
			break;
		case 6:
			value = __readdr(6);
			break;
		case 7:
			value = hv::vmread(GUEST_DR7);
			break;
		default:
			hv::inject_interruption(
				EXCEPTION_VECTOR_UNDEFINED_OPCODE,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION,
				0,
				false);
			return;
		}

		hv::write_guest_gpr(
			vcpu->vmexit_info.guest_registers,
			generalRegister,
			value);
	}

	adjust_rip(vcpu);
}

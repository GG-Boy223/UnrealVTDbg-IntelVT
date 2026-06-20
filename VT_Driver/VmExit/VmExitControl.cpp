#include "VmExitControl.h"

void vmexit_mov_dr_handler(__vcpu* vcpu)
{
	__exit_qualification_dr qual = { vcpu->vmexit_info.qualification };
	unsigned dr = (unsigned)qual.debug_register_number;
	unsigned gpr = (unsigned)qual.gp_register;

	if (qual.access_direction == 0)
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
			hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false);
			return;
		}
		hv::write_guest_gpr(vcpu->vmexit_info.guest_registers, gpr, value);
	}

	adjust_rip(vcpu);
}

bool mov_to_cr3_handler(__vcpu* vcpu, unsigned __int64 guest_cr3)
{
	cr3 new_cr3;
	new_cr3.flags = guest_cr3;

	auto const curr_cr4 = hv::read_effective_guest_cr4();

	bool invalidate_tlb = true;

	if (curr_cr4.pcid_enable && (new_cr3.flags & (1ull << 63)))
	{
		invalidate_tlb = false;
		new_cr3.flags &= ~(1ull << 63);
	}

	cpuid_eax_80000008 cpuid_80000008;
	__cpuid(reinterpret_cast<int*>(&cpuid_80000008), 0x80000008);
	uint64_t max_phys_addr = cpuid_80000008.eax.number_of_physical_address_bits;

	auto const reserved_mask = ~((1ull << max_phys_addr) - 1);
	if (new_cr3.flags & reserved_mask)
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
		return false;
	}

	if (invalidate_tlb)
	{
		invvpid_single_context_except_global_translations(guest_vpid);
	}

	hv::vmwrite(GUEST_CR3, new_cr3.flags);
	return true;
}

void emulate_lmsw(__vcpu* vcpu, uint16_t const value)
{
	sLog("\n");

	cr0 new_cr0;
	new_cr0.flags = value;

	cr0 shadow_cr0;
	shadow_cr0.flags = hv::vmread(CR0_READ_SHADOW);
	shadow_cr0.protection_enable = new_cr0.protection_enable;
	shadow_cr0.monitor_coprocessor = new_cr0.monitor_coprocessor;
	shadow_cr0.emulate_fpu = new_cr0.emulate_fpu;
	shadow_cr0.task_switched = new_cr0.task_switched;
	hv::vmwrite(CR0_READ_SHADOW, shadow_cr0.flags);

	cr0 real_cr0;
	real_cr0.flags = hv::vmread(GUEST_CR0);
	real_cr0.protection_enable = new_cr0.protection_enable;
	real_cr0.monitor_coprocessor = new_cr0.monitor_coprocessor;
	real_cr0.emulate_fpu = new_cr0.emulate_fpu;
	real_cr0.task_switched = new_cr0.task_switched;
	hv::vmwrite(GUEST_CR0, real_cr0.flags);
}

bool mov_to_cr0_handler(__vcpu* vcpu, unsigned __int64 guest_cr0)
{
	sLog("\n");

	union
	{
		__cr0 cr0;
		__cr3 cr3;
		__cr4 cr4;
		unsigned __int64 all;
	}cr_registers;

	cr_registers.cr0.all = guest_cr0;

	auto const curr_cr0 = hv::read_effective_guest_cr0();
	auto const curr_cr4 = hv::read_effective_guest_cr4();

	if ((cr_registers.cr0.paging_enable == 0) ||
		(cr_registers.cr0.paging_enable && !cr_registers.cr0.protection_enable) ||
		(!cr_registers.cr0.cache_disable && cr_registers.cr0.not_write_through) ||
		(!cr_registers.cr0.write_protect && curr_cr4.control_flow_enforcement_enable) ||
		(cr_registers.cr0.all & 0xFFFFFFFF00000000))
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
		return false;
	}

	if (cr_registers.cr0.cache_disable != curr_cr0.cache_disable ||
		cr_registers.cr0.not_write_through != curr_cr0.not_write_through)
	{
		if (cr_registers.cr0.cache_disable)
		{
			hv::set_ept_memory_type(*vcpu->ept_state, MEMORY_TYPE_UNCACHEABLE);
		}
		else
		{
			hv::update_ept_memory_type(*vcpu->ept_state);
		}

		_mm_mfence();
		invept_all_contexts_func();
	}

	__cr0 cr0;
	cr0.all = cr_registers.cr0.all;
	cr0.reserved_1 = 0;
	cr0.reserved_2 = 0;
	cr0.reserved_3 = 0;
	cr0.extension_type = 1;
	hv::vmwrite(CR0_READ_SHADOW, cr0.all);

	__cr_fixed cr_fixed;
	cr_fixed.all = __readmsr(IA32_VMX_CR0_FIXED0);
	cr0.all |= cr_fixed.split.low;
	cr_fixed.all = __readmsr(IA32_VMX_CR0_FIXED1);
	cr0.all &= cr_fixed.split.low;
	hv::vmwrite(GUEST_CR0, cr0.all);
	return true;
}

bool mov_to_cr4_handler(unsigned __int64 guest_cr4)
{
	sLog("\n");
	cr4 new_cr4;
	new_cr4.flags = guest_cr4;

	__cr3 guest_cr3;
	guest_cr3.all = hv::vmread(GUEST_CR3);

	auto const curr_cr0 = hv::read_effective_guest_cr0();
	auto const curr_cr4 = hv::read_effective_guest_cr4();

	if ((new_cr4.reserved1 != 0) ||
		(new_cr4.reserved2 != 0) ||
		(new_cr4.physical_address_extension == 0) ||
		(new_cr4.linear_addresses_57_bit) ||
		(new_cr4.control_flow_enforcement_enable && !curr_cr0.write_protect) ||
		((new_cr4.pcid_enable && !curr_cr4.pcid_enable) && guest_cr3.pcid != 0) ||
		(new_cr4.flags & 0xFFFFFFFF00000000))
	{
		hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
			INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
		return false;
	}

	if (new_cr4.page_global_enable != curr_cr4.page_global_enable ||
		!new_cr4.pcid_enable && curr_cr4.pcid_enable ||
		new_cr4.smep_enable && !curr_cr4.smep_enable)
	{
		invvpid_single_context_func(guest_vpid);
	}

	hv::vmwrite<unsigned __int64>(CR4_READ_SHADOW, new_cr4.flags);

	__cr4 cr4;
	__cr_fixed cr_fixed;
	cr_fixed.all = __readmsr(IA32_VMX_CR4_FIXED0);
	cr4.all = new_cr4.flags;
	cr4.all |= cr_fixed.split.low;
	cr_fixed.all = __readmsr(IA32_VMX_CR4_FIXED1);
	cr4.all &= cr_fixed.split.low;
	hv::vmwrite<unsigned __int64>(GUEST_CR4, cr4.all);
	return true;
}

void vmexit_cr_handler(__vcpu* vcpu)
{
	__cr0 guest_cr0;
	__cr3 guest_cr3;

	vmx_exit_qualification_mov_cr qualification;
	qualification.flags = vcpu->vmexit_info.qualification;

	union
	{
		__cr0 cr0;
		__cr3 cr3;
		__cr4 cr4;
		unsigned __int64 all;
	}cr_registers;

	cr_registers.all = hv::read_guest_gpr(vcpu->vmexit_info.guest_registers,
		qualification.general_purpose_register);
	switch (qualification.access_type)
	{
	case CR_ACCESS_MOV_TO_CR:
	{
		switch (qualification.control_register)
		{
		case 0:
		{
			if (mov_to_cr0_handler(vcpu, cr_registers.cr0.all) == false)
			{
				return;
			}
			break;
		}
		case 3:
		{
			if (mov_to_cr3_handler(vcpu, cr_registers.cr3.all) == false)
			{
				return;
			}
			break;
		}
		case 4:
		{
			if (mov_to_cr4_handler(cr_registers.cr4.all) == false)
			{
				return;
			}
			break;
		}
		}

		break;
	}
	case CR_ACCESS_MOV_FROM_CR:
	{
		switch (qualification.control_register)
		{
		case 3:
		{
			hv::write_guest_gpr(vcpu->vmexit_info.guest_registers,
				qualification.general_purpose_register,
				hv::vmread(GUEST_CR3));
			break;
		}
		}
		break;
	}
	case CR_ACCESS_CLTS:
	{
		hv::vmwrite<unsigned __int64>(CR0_READ_SHADOW,
			hv::vmread(CR0_READ_SHADOW) & ~CR0_TASK_SWITCHED_FLAG);

		hv::vmwrite<unsigned __int64>(GUEST_CR0,
			hv::vmread(GUEST_CR0) & ~CR0_TASK_SWITCHED_FLAG);

		break;
	}

	case CR_ACCESS_LMSW:
	{
		emulate_lmsw(vcpu, qualification.lmsw_source_data);
		break;
	}
	}

	set_hide_vm_exit_overhead(vcpu, true);
	adjust_rip(vcpu);
}

#include "VmExitEpt.h"

void vmexit_ept_misconfiguration_handler(__vcpu* vcpu)
{
	sLog("\n");
	auto const phys = hv::vmread(GUEST_PHYSICAL_ADDRESS);
}

void vmexit_ept_violation_handler(__vcpu* vcpu)
{
	__ept_violation ept_violation;

	ept_violation.all = vcpu->vmexit_info.qualification;
	unsigned __int64 guest_physical_adddress = hv::vmread(GUEST_PHYSICAL_ADDRESS);

	PLIST_ENTRY current = &vcpu->ept_state->hooked_page_list;
	while (&vcpu->ept_state->hooked_page_list != current->Flink)
	{
		current = current->Flink;
		__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

		if (hooked_page_info->pfn_of_hooked_page == GET_PFN(guest_physical_adddress))
		{
			if ((ept_violation.read_access || ept_violation.write_access) && (!ept_violation.ept_readable || !ept_violation.ept_writeable))
			{
				if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
				{
					int bpType = 0;
					if (ept::ept_handleWatchEvent(vcpu, ept_violation, hooked_page_info, guest_physical_adddress, bpType))
					{
						if (bpType == 3)
						{
							hooked_page_info->isInt3 = true;
							hooked_page_info->original_entry.execute = 1;
							if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
							{
								return;
							}
							hv::set_mtf(true);
							ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
								hooked_page_info->entry_address,
								hooked_page_info->original_entry,
								invept_type::invept_single_context);
						}
						else
						{
							if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
							{
								return;
							}
							_mm_mfence();
							invept_single_context_func(vcpu->ept_state->ept_pointer->all);
						}
					}
				}

				if (hooked_page_info->Options == EPTO_HOOK_FUNCTION)
				{
					hooked_page_info->original_entry.execute = 1;
					if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
					{
						return;
					}
					hv::set_mtf(true);
					ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
						hooked_page_info->entry_address,
						hooked_page_info->original_entry,
						invept_type::invept_single_context);
				}
			}
			else if (ept_violation.execute_access && (ept_violation.ept_readable || ept_violation.ept_writeable))
			{
				if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
				{
					ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
						hooked_page_info->entry_address,
						hooked_page_info->changed_entry,
						invept_type::invept_single_context);
				}

				if (hooked_page_info->Options == EPTO_HOOK_FUNCTION)
				{
					ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
						hooked_page_info->entry_address,
						hooked_page_info->changed_entry,
						invept_type::invept_single_context);
				}
			}

			break;
		}
	}
}

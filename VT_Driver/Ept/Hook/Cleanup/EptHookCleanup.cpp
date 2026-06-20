#include "../../EptHook.h"
#include "../../EptBuild.h"
#include "../../EptSync.h"
#include "../../../EPT.h"
#include "../../../hypervisor_routines.h"
#include "../../../poolmanager.h"
#include "../../../vmx.h"

namespace ept
{
	/// <summary>
	/// Unhook single function
	/// </summary>
	bool unhook_function(__ept_state& ept_state, unsigned __int64 virtual_address)
	{
		unsigned __int64 physical_address = MmGetPhysicalAddress((void*)virtual_address).QuadPart;
		if (physical_address == 0)
			return false;

		PLIST_ENTRY current_hooked_page = &ept_state.hooked_page_list;
		while (&ept_state.hooked_page_list != current_hooked_page->Flink)
		{
			current_hooked_page = current_hooked_page->Flink;
			__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current_hooked_page, __ept_hooked_page_info, hooked_page_list);

			if (hooked_page_info->pfn_of_hooked_page == GET_PFN(physical_address))
			{
				PLIST_ENTRY current_hooked_function = &hooked_page_info->hooked_functions_list;

				while (&hooked_page_info->hooked_functions_list != current_hooked_function->Flink)
				{
					current_hooked_function = current_hooked_function->Flink;
					__ept_hooked_function_info* hooked_function_info = CONTAINING_RECORD(current_hooked_function, __ept_hooked_function_info, hooked_function_list);

					unsigned __int64 function_page_offset = MASK_EPT_PML1_OFFSET(virtual_address);

					if (function_page_offset == MASK_EPT_PML1_OFFSET(hooked_function_info->virtual_address))
					{
						RtlCopyMemory(&hooked_function_info->fake_page_contents[function_page_offset], hooked_function_info->virtual_address, hooked_function_info->hook_size);

						if (hooked_function_info->second_trampoline_address != nullptr)
							unhook_function(ept_state, (unsigned __int64)hooked_function_info->second_trampoline_address);

						RemoveEntryList(current_hooked_function);

						if (hooked_function_info->first_trampoline_address != nullptr)
							pool_manager::release_pool(hooked_function_info->first_trampoline_address);
						pool_manager::release_pool(hooked_function_info);

						if (hooked_page_info->hooked_functions_list.Flink == hooked_page_info->hooked_functions_list.Blink)
						{
							__ept_dynamic_split* owner_split = hooked_page_info->owner_split;
							if (owner_split == nullptr)
								owner_split = ept_get_split_from_pml1(hooked_page_info->entry_address);

							hooked_page_info->original_entry.execute = 1;
							swap_pml1_and_invalidate_tlb(
								ept_state,
								hooked_page_info->entry_address,
								hooked_page_info->original_entry,
								invept_single_context);

							RemoveEntryList(current_hooked_page);
							pool_manager::release_pool(hooked_page_info);

							ept_split_ref_dec_and_maybe_release(ept_state, owner_split);
							return true;
						}

						_mm_mfence();
						invept_all_contexts_func();
						return true;
					}
				}
			}
		}

		return false;
	}

	/// <summary>
	/// Unhook all functions and invalidate tlb
	/// </summary>
	void unhook_all_functions(__ept_state& ept_state)
	{
		PLIST_ENTRY current_hooked_page = ept_state.hooked_page_list.Flink;
		while (&ept_state.hooked_page_list != current_hooked_page)
		{
			__ept_hooked_page_info* hooked_entry = CONTAINING_RECORD(current_hooked_page, __ept_hooked_page_info, hooked_page_list);
			PLIST_ENTRY next_hooked_page = current_hooked_page->Flink;

			__ept_dynamic_split* owner_split = hooked_entry->owner_split;
			if (owner_split == nullptr)
				owner_split = ept_get_split_from_pml1(hooked_entry->entry_address);

			if (ept_state.page_to_change == hooked_entry)
			{
				hv::clear_pending_mtf_metadata(&ept_state);
				hv::set_mtf(false);
			}

			PLIST_ENTRY current_hooked_function = hooked_entry->hooked_functions_list.Flink;
			while (&hooked_entry->hooked_functions_list != current_hooked_function)
			{
				__ept_hooked_function_info* hooked_function_info = CONTAINING_RECORD(current_hooked_function, __ept_hooked_function_info, hooked_function_list);
				PLIST_ENTRY next_hooked_function = current_hooked_function->Flink;

				if (hooked_function_info->fake_page_contents != nullptr &&
					hooked_function_info->virtual_address != nullptr &&
					hooked_function_info->hook_size != 0)
				{
					const unsigned __int64 function_page_offset =
						MASK_EPT_PML1_OFFSET(reinterpret_cast<unsigned __int64>(hooked_function_info->virtual_address));

					if (function_page_offset < PAGE_SIZE &&
						hooked_function_info->hook_size <= PAGE_SIZE - function_page_offset)
					{
						RtlCopyMemory(
							&hooked_function_info->fake_page_contents[function_page_offset],
							hooked_function_info->virtual_address,
							hooked_function_info->hook_size);
					}
				}

				RemoveEntryList(current_hooked_function);
				InitializeListHead(current_hooked_function);
				current_hooked_function = next_hooked_function;
			}

			__ept_pte restored_entry = hooked_entry->entry_address != nullptr
				? *hooked_entry->entry_address
				: __ept_pte{ 0 };
			if (hooked_entry->original_entry.page_frame_number != 0)
			{
				restored_entry = hooked_entry->original_entry;
			}

			restored_entry.read = 1;
			restored_entry.write = 1;
			restored_entry.execute = 1;

			swap_pml1_and_invalidate_tlb(
				ept_state,
				hooked_entry->entry_address,
				restored_entry,
				invept_single_context);

			RemoveEntryList(current_hooked_page);
			InitializeListHead(current_hooked_page);
			current_hooked_page = next_hooked_page;

			UNREFERENCED_PARAMETER(owner_split);
		}
	}
}

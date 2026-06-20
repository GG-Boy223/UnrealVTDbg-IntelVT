#include "EptWatchInternal.h"

namespace ept
{
	bool ept_watch_deactivate_internal(__vcpu* vcpu, __ept_state& ept_state, unsigned __int64 VirtualAddress, unsigned __int64 physical_address, unsigned __int64 Type,
		int bpType, unsigned char OriginalByte, int has, int ignoredID)
	{
		if (physical_address == NULL)
		{
			return false;
		}

		PLIST_ENTRY current_hooked_page = &ept_state.hooked_page_list;
		while (&ept_state.hooked_page_list != current_hooked_page->Flink)
		{
			current_hooked_page = current_hooked_page->Flink;
			__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current_hooked_page, __ept_hooked_page_info, hooked_page_list);

			if (hooked_page_info->pfn_of_hooked_page == GET_PFN(physical_address))
			{
				if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
				{
					if (hooked_page_info->last_watch_id == ignoredID)
					{
						hooked_page_info->last_watch_id = -1;
					}

					if (bpType == 3)
					{
						int offset = VirtualAddress & 0xFFF;
						hooked_page_info->fake_page_contents[offset] = OriginalByte;

						if (has == 0)
						{
							hooked_page_info->original_entry.execute = 1;
							swap_pml1_and_invalidate_tlb(ept_state, hooked_page_info->entry_address, hooked_page_info->original_entry, invept_single_context);
						}
						else
						{
							apply_active_watch_permissions_locked(hooked_page_info, ignoredID);
						}
					}
					else
					{
						if (has == 0 && Type == EPTW_WRITE)
						{
							hooked_page_info->entry_address->write = 1;
						}
						else if (has == 0 && Type == EPTW_READWRITE)
						{
							hooked_page_info->entry_address->read = 1;
							hooked_page_info->entry_address->write = 1;
						}
						else if (has == 0 && Type == EPTW_EXECUTE)
						{
							hooked_page_info->entry_address->execute = 1;
						}
						else if (has != 0)
						{
							apply_active_watch_permissions_locked(hooked_page_info, ignoredID);
						}
					}

					if (has == 0)
					{
						if (ept_state.page_to_change == hooked_page_info)
						{
							hv::clear_pending_mtf_metadata(vcpu);
						}

						hooked_page_info->ID = -1;
						hooked_page_info->isBp = false;
						hooked_page_info->isInt3 = false;
						hooked_page_info->last_watch_id = -1;
						hooked_page_info->fake_page_synced = false;
						hooked_page_info->fake_page_needs_full_sync = false;

						__ept_dynamic_split* owner_split = hooked_page_info->owner_split;
						if (owner_split == nullptr)
						{
							owner_split = ept_get_split_from_pml1(hooked_page_info->entry_address);
						}

						RemoveEntryList(current_hooked_page);
						pool_manager::release_pool(hooked_page_info);
						ept_split_ref_dec_and_maybe_release(ept_state, owner_split);
					}
					return true;
				}
			}
		}
		return false;
	}

	int ept_watch_deactivate(VT_BREAK_POINT vmcallinfo, int ID)
	{
		int status = 0;
		int has = 0;
		unsigned int cpu_count = normalize_vcpu_count(vmcallinfo.CPUCount);
		if (cpu_count == 0)
		{
			return 4;
		}

		spinlock::lock(&eptWatchList_lock);

		if (ID < 0 || ID >= EPTWATCHLISTSIZE)
		{
			spinlock::unlock(&eptWatchList_lock);
			return 1;
		}

		if (eptWatchList[ID].inuse == 0)
		{
			spinlock::unlock(&eptWatchList_lock);
			return 2;
		}

		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if ((i != ID) && ept_isWatchPage(GET_PFN(eptWatchList[ID].PhysicalAddress), i))
			{
				has = 1;
				if (eptWatchList[i].Type == eptWatchList[ID].Type)
				{
					has = 2;
					break;
				}
			}
		}

		if ((has != 2) || (eptWatchList[ID].bpType == 3))
		{
			for (unsigned int i = 0; i < cpu_count; i++)
			{
				__vcpu* vcpu = &g_vmm_context.vcpu[i];
				if (ept_watch_deactivate_internal(vcpu,
					*vcpu->ept_state,
					eptWatchList[ID].VirtualAddress,
					eptWatchList[ID].PhysicalAddress,
					eptWatchList[ID].Type,
					eptWatchList[ID].bpType,
					eptWatchList[ID].OriginalByte,
					has,
					ID))
				{
					status++;
				}
			}

			if (status == static_cast<int>(cpu_count))
			{
				_mm_mfence();
				invept_all_contexts_func();
			}
			else
			{
				spinlock::unlock(&eptWatchList_lock);
				return 3;
			}
		}

		clear_detected_breakpoint_for_watch_locked(eptWatchList[ID].VirtualAddress);
		eptWatchList[ID] = { 0 };
		spinlock::unlock(&eptWatchList_lock);
		return 0;
	}
}

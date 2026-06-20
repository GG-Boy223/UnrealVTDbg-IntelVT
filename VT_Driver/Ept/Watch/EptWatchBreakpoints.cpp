#include "EptWatchInternal.h"

namespace ept
{
	void prepare_int3_fake_page_entries(__ept_hooked_page_info* hooked_page_info)
	{
		if (hooked_page_info == nullptr || hooked_page_info->entry_address == nullptr)
		{
			return;
		}

		if (hooked_page_info->pfn_of_fake_page_contents == 0)
		{
			hooked_page_info->pfn_of_fake_page_contents = GET_PFN(MmGetPhysicalAddress(hooked_page_info->fake_page_contents).QuadPart);
		}

		hooked_page_info->original_entry = *hooked_page_info->entry_address;
		hooked_page_info->original_entry.read = 1;
		hooked_page_info->original_entry.write = 1;
		hooked_page_info->original_entry.execute = 0;
		hooked_page_info->original_entry.page_frame_number = hooked_page_info->pfn_of_hooked_page;

		hooked_page_info->changed_entry = hooked_page_info->original_entry;
		hooked_page_info->changed_entry.read = 0;
		hooked_page_info->changed_entry.write = 0;
		hooked_page_info->changed_entry.execute = 1;
		hooked_page_info->changed_entry.page_frame_number = hooked_page_info->pfn_of_fake_page_contents;
	}

	bool sync_int3_fake_page_from_guest(__ept_hooked_page_info* hooked_page_info, cr3 guest_cr3, unsigned __int64 virtual_address)
	{
		if (hooked_page_info == nullptr)
		{
			return false;
		}

		if (PAGE_SIZE != hv::read_guest_virtual_memory(guest_cr3, PAGE_ALIGN(virtual_address), &hooked_page_info->fake_page_contents, PAGE_SIZE))
		{
			hooked_page_info->fake_page_synced = false;
			hooked_page_info->fake_page_needs_full_sync = true;
			return false;
		}

		hooked_page_info->fake_page_synced = true;
		hooked_page_info->fake_page_needs_full_sync = false;
		return true;
	}

	bool get_hide_software_breakpoint(__ept_state& ept_state, PVT_BREAK_POINT vmcallinfo)
	{
		VT_BREAK_POINT tmp_vmcallinfo = { 0 };

		if (sizeof(VT_BREAK_POINT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT)))
		{
			return false;
		}

		if (tmp_vmcallinfo.Size == 0 || tmp_vmcallinfo.Size > sizeof(tmp_vmcallinfo.buffer))
		{
			return false;
		}

		if (tmp_vmcallinfo.PhysicalAddress == 0)
		{
			unsigned __int64 physical_address = hv::get_physical_address(tmp_vmcallinfo.cr3, (PVOID)tmp_vmcallinfo.VirtualAddress);
			if (physical_address == NULL)
			{
				return false;
			}

			tmp_vmcallinfo.PhysicalAddress = physical_address;
		}

		PLIST_ENTRY current = &ept_state.hooked_page_list;
		while (&ept_state.hooked_page_list != current->Flink)
		{
			current = current->Flink;
			__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

			if (hooked_page_info->pfn_of_hooked_page == GET_PFN(tmp_vmcallinfo.PhysicalAddress))
			{
				if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
				{
					unsigned __int64 offset = tmp_vmcallinfo.VirtualAddress & 0xFFF;
					if (tmp_vmcallinfo.Size > kEptPageSize - offset)
					{
						return false;
					}

					memcpy(&tmp_vmcallinfo.buffer[0], &hooked_page_info->fake_page_contents[offset], tmp_vmcallinfo.Size);

					if (sizeof(VT_BREAK_POINT) != hv::write_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT)))
					{
						return false;
					}

					return true;
				}
			}
		}

		return false;
	}

	bool hide_cc_breakpoint(__ept_state& ept_state, VT_BREAK_POINT vmcallinfo, unsigned __int64 physical_address, unsigned __int64 Type)
	{
		if (physical_address == NULL)
		{
			LogError("请求的虚拟内存在物理内存中不存在");
			return false;
		}

		PLIST_ENTRY current = &ept_state.hooked_page_list;
		while (&ept_state.hooked_page_list != current->Flink)
		{
			current = current->Flink;
			__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

			if (hooked_page_info->pfn_of_hooked_page == GET_PFN(physical_address))
			{
				if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
				{
					cr3 guest_cr3;
					guest_cr3.flags = vmcallinfo.cr3;

					prepare_int3_fake_page_entries(hooked_page_info);
					if (!hooked_page_info->fake_page_synced || hooked_page_info->fake_page_needs_full_sync)
					{
						if (!sync_int3_fake_page_from_guest(hooked_page_info, guest_cr3, vmcallinfo.VirtualAddress))
						{
							return false;
						}
					}

					int offset = vmcallinfo.VirtualAddress & 0xFFF;
					hooked_page_info->fake_page_contents[offset] = 0xCC;
					apply_active_watch_permissions_locked(hooked_page_info, -1);

					return true;
				}
			}
		}

		__ept_dynamic_split* runtime_split = nullptr;
		if (is_page_splitted(ept_state, physical_address) == false)
		{
			void* split_buffer = pool_manager::request_pool<void*>(pool_manager::INTENTION_SPLIT_PML2, true, sizeof(__ept_dynamic_split));
			if (split_buffer == nullptr)
			{
				LogError("There is no preallocated pool for split");
				return false;
			}

			if (split_pml2(g_vmm_context.mtrr_info.mtrrs, ept_state, split_buffer, physical_address) == false)
			{
				pool_manager::release_pool(split_buffer);
				LogError("Split failed");
				return false;
			}

			ept_mark_runtime_split(split_buffer);
			runtime_split = reinterpret_cast<__ept_dynamic_split*>(split_buffer);
		}

		__ept_pte* target_page = get_pml1_entry(ept_state, physical_address);
		if (target_page == nullptr)
		{
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}

			LogError("Failed to get PML1 entry of the target address");
			return false;
		}

		__ept_hooked_page_info* hooked_page_info = pool_manager::request_pool<__ept_hooked_page_info*>(pool_manager::INTENTION_TRACK_HOOKED_PAGES, true, sizeof(__ept_hooked_page_info));
		if (hooked_page_info == nullptr)
		{
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}

			LogError("There is no preallocated pool for hooked page info");
			return false;
		}

		InitializeListHead(&hooked_page_info->hooked_functions_list);
		initialize_hooked_page_runtime_fields(hooked_page_info);

		hooked_page_info->pfn_of_hooked_page = GET_PFN(physical_address);
		hooked_page_info->pfn_of_fake_page_contents = GET_PFN(MmGetPhysicalAddress(hooked_page_info->fake_page_contents).QuadPart);
		hooked_page_info->entry_address = target_page;
		hooked_page_info->owner_split = ept_get_split_from_pml1(target_page);
		hooked_page_info->entry_address->read = 1;
		hooked_page_info->entry_address->write = 1;
		hooked_page_info->entry_address->execute = 0;
		hooked_page_info->original_entry = *target_page;
		hooked_page_info->changed_entry = *target_page;
		hooked_page_info->changed_entry.read = 0;
		hooked_page_info->changed_entry.write = 0;
		hooked_page_info->changed_entry.execute = 1;
		hooked_page_info->changed_entry.page_frame_number = hooked_page_info->pfn_of_fake_page_contents;
		hooked_page_info->Options = EPTO_VIRTUAL_BREAKPOINT;

		cr3 guest_cr3;
		guest_cr3.flags = vmcallinfo.cr3;

		if (!sync_int3_fake_page_from_guest(hooked_page_info, guest_cr3, vmcallinfo.VirtualAddress))
		{
			pool_manager::release_pool(hooked_page_info);
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}
			return false;
		}

		int offset = vmcallinfo.VirtualAddress & 0xFFF;
		hooked_page_info->fake_page_contents[offset] = 0xCC;
		InsertHeadList(&ept_state.hooked_page_list, &hooked_page_info->hooked_page_list);
		return true;
	}
}

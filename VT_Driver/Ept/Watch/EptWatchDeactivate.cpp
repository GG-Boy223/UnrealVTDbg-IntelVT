#include "EptWatchInternal.h"

namespace ept
{
	bool ept_watch_deactivate_internal(__vcpu* vcpu, __ept_state& ept_state, unsigned __int64 VirtualAddress, unsigned __int64 physical_address, unsigned __int64 Type,
		int bpType, unsigned char OriginalByte, int has, int ignoredID)
	{
		UNREFERENCED_PARAMETER(Type);

		// 检测该函数是否存在于物理内存中
		// 如果该函数的物理地址为空
		// 说明该函数不存在于物理内存中
		//
		if (physical_address == NULL)
		{
			return false;
		}

		PLIST_ENTRY current_hooked_page = &ept_state.hooked_page_list;
		while (&ept_state.hooked_page_list != current_hooked_page->Flink)
		{
			current_hooked_page = current_hooked_page->Flink;
			__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current_hooked_page, __ept_hooked_page_info, hooked_page_list);

			// 检查函数 pfn 是否等于挂钩页面信息中保存的 pfn
			//
			if (hooked_page_info->pfn_of_hooked_page == GET_PFN(physical_address))
			{
				if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
				{
					if (hooked_page_info->last_watch_id == ignoredID)
					{
						hooked_page_info->last_watch_id = -1;
					}

					if (bpType == 3) // 原理：bpType 表示断点类型；作用：区分 INT3 等类型；结果：决定是否设置 MTF
					{
						int offset = VirtualAddress & 0xFFF;
						hooked_page_info->fake_page_contents[offset] = OriginalByte;

						if (has == 0) //如果has为0则说明没有存在其他的监视断点，那么我们可以将页进行回收
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
						if (has == 0)
						{
							hooked_page_info->entry_address->read = 1;
							hooked_page_info->entry_address->write = 1;
							hooked_page_info->entry_address->execute = 1;
						}
						else
						{
							apply_active_watch_permissions_locked(hooked_page_info, ignoredID);
						}
					}

					if (has == 0) //如果has为0则说明没有存在其他的监视断点，那么我们可以将页进行回收
					{
						InterlockedExchange(&hooked_page_info->removing, 1);

						if (ept_state.page_to_change == hooked_page_info)
						{
							hv::clear_pending_mtf_metadata(vcpu);
						}

						ept::wait_for_page_quiescence(hooked_page_info);

						hooked_page_info->ID = -1;
						hooked_page_info->isBp = false;
						hooked_page_info->isInt3 = false;
						hooked_page_info->last_watch_id = -1;
						hooked_page_info->fake_page_synced = false;
						hooked_page_info->fake_page_needs_full_sync = false;

						// 先取出 owner_split，再释放 hooked_page_info。
						__ept_dynamic_split* owner_split = hooked_page_info->owner_split;
						if (owner_split == nullptr)
						{
							// 兼容旧记录：若没填 owner_split，则按 entry_address 反推一次。
							owner_split = ept_get_split_from_pml1(hooked_page_info->entry_address);
						}

						ept::remove_hooked_page(ept_state, hooked_page_info);
						// 关键修复：失败路径要回收 hooked_page_info（旧代码这里直接 return false，导致池对象泄漏）。
						pool_manager::release_pool(hooked_page_info);
						// 关键：watch 页记录被删除后，对 split 做 ref--，必要时合并回 2MB 并回收。
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
		unsigned int cpu_count = normalize_vcpu_count(vmcallinfo.CPUCount);
		if (cpu_count == 0)
		{
			return 4;
		}

		int group_ids[EPTWATCHLISTSIZE] = { 0 };
		int group_count = 0;
		int group_id = -1;

		spinlock::lock(&eptWatchTransaction_lock);
		spinlock::lock(&eptWatchList_lock);

		if (ID < 0 || ID >= EPTWATCHLISTSIZE)
		{
			spinlock::unlock(&eptWatchList_lock);
			spinlock::unlock(&eptWatchTransaction_lock);
			return 1;
		}

		if (!ept_watch_entry_is_active(eptWatchList[ID]))
		{
			spinlock::unlock(&eptWatchList_lock);
			spinlock::unlock(&eptWatchTransaction_lock);
			return 2;
		}

		group_id = eptWatchList[ID].GroupId;
		if (group_id < 0 || group_id >= EPTWATCHLISTSIZE ||
			!ept_watch_entry_is_active(eptWatchList[group_id]) ||
			eptWatchList[group_id].GroupId != group_id)
		{
			spinlock::unlock(&eptWatchList_lock);
			spinlock::unlock(&eptWatchTransaction_lock);
			return 5;
		}

		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if (ept_watch_entry_is_active(eptWatchList[i]) &&
				eptWatchList[i].GroupId == group_id)
			{
				group_ids[group_count] = i;
				group_count++;
			}
		}
		spinlock::unlock(&eptWatchList_lock);

		for (int entry_index = 0; entry_index < group_count; entry_index++)
		{
			spinlock::lock(&eptWatchList_lock);
			EPTWatchEntry entry = eptWatchList[group_ids[entry_index]];
			spinlock::unlock(&eptWatchList_lock);

			for (unsigned int cpu = 0; cpu < cpu_count; cpu++)
			{
				__vcpu* vcpu = &g_vmm_context.vcpu[cpu];
				if (vcpu->ept_state == nullptr)
				{
					spinlock::unlock(&eptWatchTransaction_lock);
					return 3;
				}

				bool found = false;
				PLIST_ENTRY current = &vcpu->ept_state->hooked_page_list;
				while (&vcpu->ept_state->hooked_page_list != current->Flink)
				{
					current = current->Flink;
					auto hooked_page_info = CONTAINING_RECORD(
						current,
						__ept_hooked_page_info,
						hooked_page_list);
					if (hooked_page_info->pfn_of_hooked_page == GET_PFN(entry.PhysicalAddress) &&
						hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					spinlock::unlock(&eptWatchTransaction_lock);
					return 3;
				}
			}
		}

		spinlock::lock(&eptWatchList_lock);
		for (int entry_index = 0; entry_index < group_count; entry_index++)
		{
			const int entry_id = group_ids[entry_index];
			if (!ept_watch_entry_is_active(eptWatchList[entry_id]) ||
				eptWatchList[entry_id].GroupId != group_id)
			{
				spinlock::unlock(&eptWatchList_lock);
				spinlock::unlock(&eptWatchTransaction_lock);
				return 5;
			}

			eptWatchList[entry_id].inuse = EPT_WATCH_REMOVING;
		}
		spinlock::unlock(&eptWatchList_lock);

		for (int entry_index = group_count - 1; entry_index >= 0; entry_index--)
		{
			spinlock::lock(&eptWatchList_lock);
			EPTWatchEntry entry = eptWatchList[group_ids[entry_index]];
			spinlock::unlock(&eptWatchList_lock);
			int has = 0;
			bool page_already_removed = false;
			for (int previous = entry_index + 1; previous < group_count; previous++)
			{
				spinlock::lock(&eptWatchList_lock);
				const bool same_page = eptWatchList[group_ids[previous]].PhysicalAddress ==
					entry.PhysicalAddress;
				spinlock::unlock(&eptWatchList_lock);
				if (same_page)
				{
					page_already_removed = true;
					break;
				}
			}

			if (page_already_removed)
			{
				continue;
			}

			spinlock::lock(&eptWatchList_lock);
			for (int other = 0; other < EPTWATCHLISTSIZE; other++)
			{
				if (eptWatchList[other].GroupId != group_id &&
					ept_watch_entry_is_active(eptWatchList[other]) &&
					GET_PFN(eptWatchList[other].PhysicalAddress) == GET_PFN(entry.PhysicalAddress))
				{
					has = 1;
					if (eptWatchList[other].Type == entry.Type)
					{
						has = 2;
						break;
					}
				}
			}
			spinlock::unlock(&eptWatchList_lock);

			if (has == 2 && entry.bpType != 3)
			{
				continue;
			}

			for (unsigned int cpu = 0; cpu < cpu_count; cpu++)
			{
				__vcpu* vcpu = &g_vmm_context.vcpu[cpu];
				if (!ept_watch_deactivate_internal(vcpu,
					*vcpu->ept_state,
					entry.VirtualAddress,
					entry.PhysicalAddress,
					entry.Type,
					entry.bpType,
					entry.OriginalByte,
					has,
					group_ids[entry_index]))
				{
					KeBugCheckEx(
						0x00020001,
						0x45505457,
						2,
						group_ids[entry_index],
						cpu);
				}
			}
		}

		_mm_mfence();
		invept_all_contexts_func();

		for (int entry_index = 0; entry_index < group_count; entry_index++)
		{
			spinlock::lock(&eptWatchList_lock);
			EPTWatchEntry entry = eptWatchList[group_ids[entry_index]];
			clear_detected_breakpoint_for_watch_locked(entry.VirtualAddress);
			eptWatchList[group_ids[entry_index]] = { 0 };
			spinlock::unlock(&eptWatchList_lock);
			Cr3TrackerRelease(entry.Cr3TrackerId);
		}

		spinlock::unlock(&eptWatchTransaction_lock);
		return 0;
	}
}

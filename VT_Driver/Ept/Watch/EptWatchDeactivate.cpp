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

		__ept_hooked_page_info* hooked_page_info =
			ept::acquire_page_hot_reference(
				ept_state,
				GET_PFN(physical_address),
				EPTO_VIRTUAL_BREAKPOINT);
		if (hooked_page_info == nullptr)
		{
			return false;
		}

		// last_watch_id 由 VM-exit 在 eptWatchList_lock 下读取和更新。
		spinlock::lock(&eptWatchList_lock);
		if (hooked_page_info->last_watch_id == ignoredID)
		{
			hooked_page_info->last_watch_id = -1;
		}
		spinlock::unlock(&eptWatchList_lock);

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
				spinlock::lock(&eptWatchList_lock);
				apply_active_watch_permissions_locked(hooked_page_info, ignoredID);
				spinlock::unlock(&eptWatchList_lock);
			}
		}
		else
		{
			if (has == 0)
			{
				// 非 INT3 的 EPT watch 删除后同样必须把 entry 归一到原页。
				// 原实现只把三个权限位置 1、不写 page_frame_number：若该页同时挂过 INT3 watch，
				// entry 可能正指向伪页，删除后 guest 会继续在伪页上执行。
				hooked_page_info->entry_address->read = 1;
				hooked_page_info->entry_address->write = 1;
				hooked_page_info->entry_address->execute = 1;
				hooked_page_info->entry_address->page_frame_number = hooked_page_info->pfn_of_hooked_page;
			}
			else
			{
				spinlock::lock(&eptWatchList_lock);
				apply_active_watch_permissions_locked(hooked_page_info, ignoredID);
				spinlock::unlock(&eptWatchList_lock);
			}
		}

		if (has == 0) //如果has为0则说明没有存在其他的监视断点，那么我们可以将页进行回收
		{
			InterlockedExchange(&hooked_page_info->removing, 1);

			if (ept_state.page_to_change == hooked_page_info)
			{
				hv::clear_pending_mtf_metadata(vcpu);
			}

			// 摘链仍需要访问页对象，必须在释放热引用之前完成。
			ept::remove_hooked_page(ept_state, hooked_page_info);
			ept::release_page_hot_reference(ept_state, hooked_page_info);
			hooked_page_info = nullptr;
			ept::reclaim_retired_resources(ept_state);
		}
		else
		{
			ept::release_page_hot_reference(ept_state, hooked_page_info);
			hooked_page_info = nullptr;
		}

		return true;
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

		spinlock::lock(&eptWatchList_lock);

		if (ID < 0 || ID >= EPTWATCHLISTSIZE)
		{
			spinlock::unlock(&eptWatchList_lock);
			return 1;
		}

		if (!ept_watch_entry_is_active(eptWatchList[ID]))
		{
			spinlock::unlock(&eptWatchList_lock);
			return 2;
		}

		group_id = eptWatchList[ID].GroupId;
		if (group_id < 0 || group_id >= EPTWATCHLISTSIZE ||
			!ept_watch_entry_is_active(eptWatchList[group_id]) ||
			eptWatchList[group_id].GroupId != group_id)
		{
			spinlock::unlock(&eptWatchList_lock);
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
					return 3;
				}

				__ept_hooked_page_info* hooked_page_info =
					ept::acquire_page_hot_reference(
						*vcpu->ept_state,
						GET_PFN(entry.PhysicalAddress),
						EPTO_VIRTUAL_BREAKPOINT);
				const bool found = hooked_page_info != nullptr;
				ept::release_page_hot_reference(*vcpu->ept_state, hooked_page_info);

				if (!found)
				{
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

		return 0;
	}
}

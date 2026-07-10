#include "EptWatchInternal.h"

namespace ept
{
	bool ept_watch_deactivate_internal(__vcpu* vcpu, __ept_state& ept_state, unsigned __int64 VirtualAddress, unsigned __int64 physical_address, unsigned __int64 Type,
		int bpType, unsigned char OriginalByte, int has, int ignoredID)
	{
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
						if (has == 0 && Type == EPTW_WRITE) //撤销写监视
						{
							hooked_page_info->entry_address->write = 1;
						}
						//else if (Type == EPTW_READ)  //撤销读监视
						//{
						//	if (bpType == 3)  //int3
						//	{
						//		//换回原页
						//		hooked_page_info->entry_address->all = hooked_page_info->original_entry.all;
						//	}
						//	hooked_page_info->entry_address->read = 1;
						//}
						else if (has == 0 && Type == EPTW_READWRITE) //撤销读写监视
						{
							//还原ept页的属性，让guest程序能正常执行，正常读写
							hooked_page_info->entry_address->read = 1;
							hooked_page_info->entry_address->write = 1;
						}
						else if (has == 0 && Type == EPTW_EXECUTE) //撤销执行监视
						{
							hooked_page_info->entry_address->execute = 1;
						}
						else if (has != 0)
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

						while (InterlockedCompareExchange(&hooked_page_info->mtf_reference_count, 0, 0) != 0)
						{
							_mm_pause();
						}

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

						RemoveEntryList(current_hooked_page);
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
		int status = 0; // 预留状态变量（当前未使用）
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

		if (!ept_watch_entry_is_active(eptWatchList[ID]))
		{
			spinlock::unlock(&eptWatchList_lock);
			return 2;
		}

		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if ((i != ID) && ept_isWatchPage(GET_PFN(eptWatchList[ID].PhysicalAddress), i))
			{
				has = 1; //属于同一页，存在其他监视断点
				if (eptWatchList[i].Type == eptWatchList[ID].Type)
				{
					has = 2; //不能撤销
					break;
				}
			}
		}

		//没有其他的监视断点了，现在可以把页撤销了
		if ((has != 2) || (eptWatchList[ID].bpType == 3))
		{
			//设置每个cpu
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
		eptWatchList[ID] = { 0 }; //将当前entry的inuse标记为未使用
		spinlock::unlock(&eptWatchList_lock);
		return 0;
	}
}

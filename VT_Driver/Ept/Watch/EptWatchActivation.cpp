#include "EptWatchInternal.h"

namespace ept
{
	void set_ept_watch(__ept_hooked_page_info* hooked_page_info, unsigned __int64 Type)
	{
		if (Type == EPTW_WRITE) //监视写
		{
			hooked_page_info->entry_address->write = 0;
		}
		//else if (Type == EPTW_READ) //监视读
		//{
		//	hooked_page_info->entry_address->read = 0;
		//}
		else if (Type == EPTW_READWRITE) //监视读写
		{
			hooked_page_info->entry_address->read = 0;
			hooked_page_info->entry_address->write = 0;
		}
		else if (Type == EPTW_EXECUTE) //监视执行
		{
			//先改原页为不可执行 回到guest后执行就触发vmexit
			//进入vmm后交换伪造页
			hooked_page_info->entry_address->execute = 0;
		}

		hooked_page_info->Options = EPTO_VIRTUAL_BREAKPOINT;
	}

	void apply_active_watch_permissions_locked(__ept_hooked_page_info* hooked_page_info, int ignoredID)
	{
		if (hooked_page_info == nullptr || hooked_page_info->entry_address == nullptr)
		{
			return;
		}

		hooked_page_info->entry_address->read = 1;
		hooked_page_info->entry_address->write = 1;
		hooked_page_info->entry_address->execute = 1;

		const unsigned __int64 pfn = hooked_page_info->pfn_of_hooked_page;
		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if (i == ignoredID || !ept_watch_entry_is_active(eptWatchList[i]) || GET_PFN(eptWatchList[i].PhysicalAddress) != pfn)
			{
				continue;
			}

			if (eptWatchList[i].bpType == 3)
			{
				//先改原页为不可执行 回到guest后执行就触发vmexit
				//进入vmm后交换伪造页
				hooked_page_info->entry_address->execute = 0;
				continue;
			}

			if (eptWatchList[i].Type == EPTW_WRITE)
			{
				hooked_page_info->entry_address->write = 0;
			}
			else if (eptWatchList[i].Type == EPTW_READWRITE)
			{
				hooked_page_info->entry_address->read = 0;
				hooked_page_info->entry_address->write = 0;
			}
			else if (eptWatchList[i].Type == EPTW_EXECUTE)
			{
				//先改原页为不可执行 回到guest后执行就触发vmexit
				//进入vmm后交换伪造页
				hooked_page_info->entry_address->execute = 0;
			}
		}
	}

	bool ept_watch_activate_internal(__ept_state& ept_state, unsigned __int64 physical_address, unsigned __int64 Type)
	{
		// 检测该函数是否存在于物理内存中
		// 如果该函数的物理地址为空
		// 说明该函数不存在于物理内存中
		//
		if (physical_address == NULL)
		{
			LogError("请求的虚拟内存在物理内存中不存在");
			return false;
		}

		PLIST_ENTRY current = &ept_state.hooked_page_list;
		while (&ept_state.hooked_page_list != current->Flink) // 遍历链表直到回到表头
		{
			current = current->Flink;
			__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

			if (hooked_page_info->pfn_of_hooked_page == GET_PFN(physical_address))
			{
				if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
				{
					set_ept_watch(hooked_page_info, Type);
					return true;
				}
			}
		}

		// 仅当 PDE 还是 2MB 大页时，才申请 split 缓冲并拆页。
		__ept_dynamic_split* runtime_split = nullptr;
		//判断2mb pde是否已经进行分割
		if (is_page_splitted(ept_state, physical_address) == false)
		{
			void* split_buffer = pool_manager::request_pool<void*>(pool_manager::INTENTION_SPLIT_PML2, true, sizeof(__ept_dynamic_split));
			if (split_buffer == nullptr)
			{
				LogError("There is no preallocated pool for split");
				return false;
			}

			//将2MB页面分割为512个4KB页面
			if (split_pml2(g_vmm_context.mtrr_info.mtrrs, ept_state, split_buffer, physical_address) == false)
			{
				pool_manager::release_pool(split_buffer);
				LogError("Split failed");
				return false;
			}

			ept_mark_runtime_split(split_buffer);
			runtime_split = reinterpret_cast<__ept_dynamic_split*>(split_buffer);
		}

		//得到目标页
		__ept_pte* target_page = get_pml1_entry(ept_state, physical_address);
		if (target_page == nullptr)
		{
			// 如果本次刚做了 runtime split 但未成功建立监视记录，则尝试回收 split。
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
			// 如果本次刚做了 runtime split 但未成功建立监视记录，则尝试回收 split。
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}

			LogError("There is no preallocated pool for hooked page info");
			return false;
		}

		InitializeListHead(&hooked_page_info->hooked_functions_list);
		initialize_hooked_page_runtime_fields(hooked_page_info);
		hooked_page_info->pfn_of_hooked_page = GET_PFN(physical_address); //获取guest物理页面的页帧号
		hooked_page_info->entry_address = target_page; //ept_pte
		// 建立 owner_split 关系：后续撤销 watch 时用来 ref-- 与尝试合并回 2MB。
		hooked_page_info->owner_split = ept_get_split_from_pml1(target_page);
		set_ept_watch(hooked_page_info, Type);
		// 引用计数 +1：表示该 split 现在被一个 watch 页面记录占用。
		ept_split_ref_inc(hooked_page_info->owner_split);
		// Track all hooked pages
		InsertHeadList(&ept_state.hooked_page_list, &hooked_page_info->hooked_page_list);
		return true;
	}

	bool ept_watch_activate(VT_BREAK_POINT vmcallinfo, unsigned __int64 Type, int* outID, int& errorCode)
	{
		unsigned int cpu_count = normalize_vcpu_count(vmcallinfo.CPUCount);
		if (cpu_count == 0 || !is_valid_watch_range(vmcallinfo.VirtualAddress, vmcallinfo.Size))
		{
			errorCode = 99;
			return false;
		}

		unsigned __int64 va = vmcallinfo.VirtualAddress;
		unsigned __int64 left = vmcallinfo.Size;
		int firstID = -1;
		int ids[EPTWATCHLISTSIZE] = { 0 };
		int id_count = 0;

		spinlock::lock(&eptWatchList_lock);

		while (left > 0)
		{
			unsigned __int64 page_offset = va & 0xFFF;
			unsigned __int64 chunk = 0x1000 - page_offset;
			if (chunk > left)
			{
				chunk = left;
			}

			unsigned __int64 physical_address = hv::get_physical_address(vmcallinfo.cr3, (PVOID)va);
			// 检测该函数是否存在于物理内存中
			// 如果该函数的物理地址为空
			// 说明该函数不存在于物理内存中
			//
			if (physical_address == NULL)
			{
				errorCode = 100;
				spinlock::unlock(&eptWatchList_lock);
				goto rollback;
			}

			int ID = hv::getIdleWatchID();
			if (ID == -1)
			{
				errorCode = 101;
				spinlock::unlock(&eptWatchList_lock);
				goto rollback;
			}

			if (firstID == -1)
			{
				firstID = ID;
				vmcallinfo.PhysicalAddress = physical_address;
			}

			if (id_count >= EPTWATCHLISTSIZE)
			{
				errorCode = 103;
				spinlock::unlock(&eptWatchList_lock);
				goto rollback;
			}
			ids[id_count++] = ID;

			eptWatchList[ID].Size = static_cast<int>(chunk);
			eptWatchList[ID].cr3 = vmcallinfo.cr3;
			eptWatchList[ID].VirtualAddress = va;
			eptWatchList[ID].PhysicalAddress = physical_address;
			eptWatchList[ID].LoopUserMode = vmcallinfo.LoopUserMode;
			eptWatchList[ID].Type = Type;
			eptWatchList[ID].Options = EPTO_VIRTUAL_BREAKPOINT;
			eptWatchList[ID].bpType = 1;
			eptWatchList[ID].inuse = EPT_WATCH_ACTIVE;

			int status = 0; // 预留状态变量（当前未使用）
			//设置每个cpu
			for (unsigned int i = 0; i < cpu_count; i++)
			{
				__vcpu* vcpu = &g_vmm_context.vcpu[i];
				if (ept::ept_watch_activate_internal(*vcpu->ept_state, physical_address, Type))
				{
					status++;
				}
			}

			if (status != static_cast<int>(cpu_count))
			{
				errorCode = 102;
				spinlock::unlock(&eptWatchList_lock);
				goto rollback;
			}

			va += chunk;
			left -= chunk;
		}

		if (outID)
		{
			*outID = firstID;
		}

		spinlock::unlock(&eptWatchList_lock);
		_mm_mfence();
		invept_all_contexts_func(); // 刷新 EPT 缓存，避免使用旧映射
		return true;

	rollback:
		for (int i = 0; i < id_count; i++)
		{
			ept::ept_watch_deactivate(vmcallinfo, ids[i]);
		}
		return false;
	}
}

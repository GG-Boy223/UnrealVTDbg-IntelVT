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

	//读取隐形软件断点
	bool get_hide_software_breakpoint(__ept_state& ept_state, PVT_BREAK_POINT vmcallinfo)
	{
		VT_BREAK_POINT tmp_vmcallinfo = { 0 }; // 本地缓冲保存断点请求

		if (sizeof(VT_BREAK_POINT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT))) // 读取来宾断点参数
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
			// 检测该函数是否存在于物理内存中
			// 如果该函数的物理地址为空
			// 说明该函数不存在于物理内存中
			//
			if (physical_address == NULL)
			{
				return false;
			}

			tmp_vmcallinfo.PhysicalAddress = physical_address;
		}

		PLIST_ENTRY current = &ept_state.hooked_page_list;
		while (&ept_state.hooked_page_list != current->Flink) // 遍历链表直到回到表头
		{
			current = current->Flink;
			//从列表中取出挂钩页
			__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

			//判断目标地址是否是被挂钩的页
			//仅比较页帧号
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

					//返回给调试器
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
					cr3 guest_cr3; // 临时 CR3
					guest_cr3.flags = vmcallinfo.cr3;

					prepare_int3_fake_page_entries(hooked_page_info);
					if (!hooked_page_info->fake_page_synced || hooked_page_info->fake_page_needs_full_sync)
					{
						//备份目标进程原页内容
						if (!sync_int3_fake_page_from_guest(hooked_page_info, guest_cr3, vmcallinfo.VirtualAddress))
						{
							return false;
						}
					}

					//写入cc断点
					int offset = vmcallinfo.VirtualAddress & 0xFFF;
					hooked_page_info->fake_page_contents[offset] = 0xCC;
					// Installing 条目尚未对 VM-exit 可见，但本 VCPU 的 EPT commit 必须立即包含 INT3 执行权限。
					apply_active_watch_permissions_locked(hooked_page_info, -1);
					hooked_page_info->entry_address->execute = 0;

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
		hooked_page_info->pfn_of_fake_page_contents = GET_PFN(MmGetPhysicalAddress(hooked_page_info->fake_page_contents).QuadPart);
		hooked_page_info->entry_address = target_page; //ept_pte
		hooked_page_info->owner_split = ept_get_split_from_pml1(target_page); // 获取运行期分割的 split
		//将原页修改为不可执行
		hooked_page_info->entry_address->read = 1;
		hooked_page_info->entry_address->write = 1;
		//先改原页为不可执行 回到guest后执行就触发vmexit
		//进入vmm后交换伪造页
		hooked_page_info->entry_address->execute = 0;
		hooked_page_info->original_entry = *target_page; //指向原
		hooked_page_info->changed_entry = *target_page;
		hooked_page_info->changed_entry.read = 0;
		hooked_page_info->changed_entry.write = 0;
		hooked_page_info->changed_entry.execute = 1;
		//指向伪页pfn
		hooked_page_info->changed_entry.page_frame_number = hooked_page_info->pfn_of_fake_page_contents;
		hooked_page_info->Options = EPTO_VIRTUAL_BREAKPOINT;

		cr3 guest_cr3; // 临时 CR3
		guest_cr3.flags = vmcallinfo.cr3;

		//备份目标进程原页内容
		if (!sync_int3_fake_page_from_guest(hooked_page_info, guest_cr3, vmcallinfo.VirtualAddress))
		{
			// 关键修复：失败路径要回收 hooked_page_info（旧代码这里直接 return false，导致池对象泄漏）。
			pool_manager::release_pool(hooked_page_info);
			// 如果本次刚做了 runtime split 但未成功建立监视记录，则尝试回收 split。
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}
			return false;
		}

		//写入cc断点
		int offset = vmcallinfo.VirtualAddress & 0xFFF;
		hooked_page_info->fake_page_contents[offset] = 0xCC;
		// Track all hooked pages
		InsertHeadList(&ept_state.hooked_page_list, &hooked_page_info->hooked_page_list);
		return true;
	}
}

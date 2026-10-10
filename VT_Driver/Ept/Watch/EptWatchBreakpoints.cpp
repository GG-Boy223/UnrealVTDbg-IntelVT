#include "EptWatchInternal.h"

namespace ept
{
	namespace
	{
		bool copy_software_breakpoint_buffer(
			__ept_state& ept_state,
			unsigned __int64 target_address,
			void* buffer,
			unsigned __int64 buffer_size,
			unsigned __int64 current_cr3,
			bool writing_to_fake_page)
		{
			unsigned char transfer_buffer[kEptPageSize];

			if (target_address == 0)
			{
				return false;
			}

			if (buffer == nullptr ||
				buffer_size == 0 ||
				buffer_size > kEptPageSize ||
				current_cr3 == 0)
			{
				return false;
			}

			const unsigned __int64 page_offset = target_address & 0xFFF;
			const bool in_range =
				page_offset < kEptPageSize &&
				buffer_size <= kEptPageSize - page_offset;
			if (!in_range)
			{
				return false;
			}

			cr3 guest_cr3;
			guest_cr3.flags = current_cr3;

			// 写入方向先从 guest 缓冲区安全读取,读取方向最后安全写回 guest 缓冲区。
			// 不能把 guest 虚拟地址当作宿主地址直接 RtlCopyMemory,否则在根模式下会访问错误地址。
			if (writing_to_fake_page &&
				hv::read_guest_virtual_memory(
					guest_cr3,
					buffer,
					&transfer_buffer[0],
					static_cast<SIZE_T>(buffer_size)) != buffer_size)
			{
				return false;
			}

			const unsigned __int64 physical_address =
				hv::get_physical_address(current_cr3, reinterpret_cast<PVOID>(target_address));
			if (physical_address == 0)
			{
				return false;
			}

			// 只允许操作隐形 INT3 的 fake page,避免 VMCALL 23 误改函数 hook 伪页。
			__ept_hooked_page_info* const hooked_page_info =
				ept::acquire_page_hot_reference(
					ept_state,
					GET_PFN(physical_address),
					EPTO_VIRTUAL_BREAKPOINT);
			if (hooked_page_info == nullptr)
			{
				return false;
			}

			bool copied = false;
			if (writing_to_fake_page)
			{
				RtlCopyMemory(
					&hooked_page_info->fake_page_contents[page_offset],
					&transfer_buffer[0],
					static_cast<SIZE_T>(buffer_size));
				copied = true;
			}
			else
			{
				RtlCopyMemory(
					&transfer_buffer[0],
					&hooked_page_info->fake_page_contents[page_offset],
					static_cast<SIZE_T>(buffer_size));
				copied = hv::write_guest_virtual_memory(
					guest_cr3,
					buffer,
					&transfer_buffer[0],
					static_cast<SIZE_T>(buffer_size)) == buffer_size;
			}

			ept::release_page_hot_reference(ept_state, hooked_page_info);
			return copied;
		}
	}

	bool get_software_breakpoint_buffer(
		__ept_state& ept_state,
		unsigned __int64 target_address,
		void* buffer,
		unsigned __int64 buffer_size,
		unsigned __int64 current_cr3)
	{
		return copy_software_breakpoint_buffer(
			ept_state,
			target_address,
			buffer,
			buffer_size,
			current_cr3,
			false);
	}

	bool set_software_breakpoint_buffer(
		__ept_state& ept_state,
		unsigned __int64 target_address,
		void* buffer,
		unsigned __int64 buffer_size,
		unsigned __int64 current_cr3)
	{
		return copy_software_breakpoint_buffer(
			ept_state,
			target_address,
			buffer,
			buffer_size,
			current_cr3,
			true);
	}

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
		// 隐藏 INT3 页必须同时拦写：伪页只是安装时刻的快照，原页的写入必须被拦截，
		// 才能在 MTF 收尾时整页重新同步伪页（见 docs/23 与 apply_active_watch_permissions_locked）。
		hooked_page_info->original_entry.write = 0;
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

		if (PAGE_SIZE != hv::read_guest_virtual_memory(
			guest_cr3,
			reinterpret_cast<void*>(ept::page_align_down(virtual_address)),
			&hooked_page_info->fake_page_contents,
			PAGE_SIZE))
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

		__ept_hooked_page_info* hooked_page_info =
			ept::acquire_page_hot_reference(
				ept_state,
				GET_PFN(tmp_vmcallinfo.PhysicalAddress),
				EPTO_VIRTUAL_BREAKPOINT);
		if (hooked_page_info == nullptr)
		{
			return false;
		}

		const unsigned __int64 offset = tmp_vmcallinfo.VirtualAddress & 0xFFF;
		if (tmp_vmcallinfo.Size > kEptPageSize - offset)
		{
			ept::release_page_hot_reference(ept_state, hooked_page_info);
			return false;
		}

		memcpy(
			&tmp_vmcallinfo.buffer[0],
			&hooked_page_info->fake_page_contents[offset],
			tmp_vmcallinfo.Size);
		ept::release_page_hot_reference(ept_state, hooked_page_info);

		//返回给调试器
		if (sizeof(VT_BREAK_POINT) != hv::write_guest_virtual_memory(
			vmcallinfo,
			&tmp_vmcallinfo,
			sizeof(VT_BREAK_POINT)))
		{
			return false;
		}

		return true;
	}

	bool hide_cc_breakpoint(__ept_state& ept_state, VT_BREAK_POINT vmcallinfo, unsigned __int64 physical_address, unsigned __int64 Type, int* failure_detail)
	{
		if (failure_detail != nullptr)
		{
			*failure_detail = EPT_WATCH_DETAIL_NONE;
		}

		// 检测该函数是否存在于物理内存中
		// 如果该函数的物理地址为空
		// 说明该函数不存在于物理内存中
		//
		if (physical_address == NULL)
		{
			if (failure_detail != nullptr)
			{
				*failure_detail = EPT_WATCH_DETAIL_PHYSICAL_ADDRESS;
			}
			LogError("请求的虚拟内存在物理内存中不存在");
			return false;
		}

		// 与 ept_watch_activate_internal 同一不变量：函数 hook 页与 watch 页
		// 不能共存于同一 PFN，否则两条记录会争用同一 EPT PTE 并互相遮蔽。
		__ept_hooked_page_info* conflicting_hook_page =
			ept::acquire_page_hot_reference(
				ept_state,
				GET_PFN(physical_address),
				EPTO_HOOK_FUNCTION);
		if (conflicting_hook_page != nullptr)
		{
			ept::release_page_hot_reference(ept_state, conflicting_hook_page);
			if (failure_detail != nullptr)
			{
				*failure_detail = EPT_WATCH_DETAIL_OPTION_CONFLICT;
			}
			LogError("目标页已有函数 EPT hook，拒绝叠加 INT3 watch");
			return false;
		}

		__ept_hooked_page_info* existing_hooked_page_info =
			ept::acquire_page_hot_reference(
				ept_state,
				GET_PFN(physical_address),
				EPTO_VIRTUAL_BREAKPOINT);
		if (existing_hooked_page_info != nullptr)
		{
			cr3 guest_cr3; // 临时 CR3
			guest_cr3.flags = vmcallinfo.cr3;

			prepare_int3_fake_page_entries(existing_hooked_page_info);
			if (!existing_hooked_page_info->fake_page_synced ||
				existing_hooked_page_info->fake_page_needs_full_sync)
			{
				//备份目标进程原页内容
				if (!sync_int3_fake_page_from_guest(
					existing_hooked_page_info,
					guest_cr3,
					vmcallinfo.VirtualAddress))
				{
					ept::release_page_hot_reference(ept_state, existing_hooked_page_info);
					if (failure_detail != nullptr)
					{
						*failure_detail = EPT_WATCH_DETAIL_FAKE_PAGE_SYNC;
					}
					return false;
				}
			}

			// 安装阶段只准备 fake page 与 EPT 权限；0xCC 在来宾收到 watch ID 后统一提交。
			spinlock::lock(&eptWatchList_lock);
			apply_active_watch_permissions_locked(existing_hooked_page_info, -1);
			existing_hooked_page_info->entry_address->execute = 0;
			spinlock::unlock(&eptWatchList_lock);

			ept::release_page_hot_reference(ept_state, existing_hooked_page_info);
			return true;
		}

		// 仅当 PDE 还是 2MB 大页时，才申请 split 缓冲并拆页。
		__ept_dynamic_split* runtime_split = nullptr;
		if (!prepare_pml2_for_address(ept_state, physical_address))
		{
			if (failure_detail != nullptr)
			{
				*failure_detail = EPT_WATCH_DETAIL_PREPARE_PML2;
			}
			LogError("Failed to prepare PML2 for breakpoint");
			return false;
		}

		//判断2mb pde是否已经进行分割
		if (is_page_splitted(ept_state, physical_address) == false)
		{
			void* split_buffer = pool_manager::request_pool<void*>(pool_manager::INTENTION_SPLIT_PML2, true, sizeof(__ept_dynamic_split));
			if (split_buffer == nullptr)
			{
				if (failure_detail != nullptr)
				{
					*failure_detail = EPT_WATCH_DETAIL_SPLIT_POOL_EMPTY;
				}
				LogError("There is no preallocated pool for split");
				return false;
			}

			//将2MB页面分割为512个4KB页面
			const auto mtrrs = hv::read_mtrr_data();
			if (split_pml2(mtrrs, ept_state, split_buffer, physical_address) == false)
			{
				pool_manager::release_pool(split_buffer);
				if (failure_detail != nullptr)
				{
					*failure_detail = EPT_WATCH_DETAIL_SPLIT_FAILED;
				}
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

			if (failure_detail != nullptr)
			{
				*failure_detail = EPT_WATCH_DETAIL_PML1_MISSING;
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

			if (failure_detail != nullptr)
			{
				*failure_detail = EPT_WATCH_DETAIL_PAGE_POOL_EMPTY;
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
		// 隐藏 INT3 页同时拦写（伪页是快照，写入必须被拦截才能在 MTF 收尾时整页重新同步，见 docs/23）；
		// original_entry 紧随其后从 *target_page 复制，因此隐藏态的 write 也一并是 0。
		hooked_page_info->entry_address->write = 0;
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
			target_page->read = 1;
			target_page->write = 1;
			target_page->execute = 1;
			pool_manager::release_pool(hooked_page_info);
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}
			_mm_mfence();
			invept_single_context_func(ept_state.ept_pointer->all);
			if (failure_detail != nullptr)
			{
				*failure_detail = EPT_WATCH_DETAIL_FAKE_PAGE_SYNC;
			}
			return false;
		}

		ept_split_ref_inc(hooked_page_info->owner_split);
		ept::insert_hooked_page(ept_state, hooked_page_info);
		return true;
	}

	bool commit_hide_cc_breakpoint(
		__ept_state& ept_state,
		unsigned __int64 virtual_address,
		unsigned __int64 physical_address)
	{
		__ept_hooked_page_info* hooked_page_info =
			ept::acquire_page_hot_reference(
				ept_state,
				GET_PFN(physical_address),
				EPTO_VIRTUAL_BREAKPOINT);
		if (hooked_page_info == nullptr)
		{
			return false;
		}

		const int offset = static_cast<int>(virtual_address & 0xFFF);
		hooked_page_info->fake_page_contents[offset] = 0xCC;
		ept::release_page_hot_reference(ept_state, hooked_page_info);
		return true;
	}
}

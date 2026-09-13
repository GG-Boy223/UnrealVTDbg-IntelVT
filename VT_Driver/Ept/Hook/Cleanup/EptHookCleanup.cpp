#include "../../EptHook.h"
#include "../../EptBuild.h"
#include "../../EptSync.h"
#include "../../EptCommon.h"
#include "../../../EPT.h"
#include "../../../hypervisor_routines.h"
#include "../../../poolmanager.h"
#include "../../../vmx.h"

namespace ept
{
	/// <summary>
	/// Unhook single function
	/// </summary>
	/// <param name="virtual_address"></param>
	/// <returns></returns>
	bool unhook_function(__ept_state& ept_state, unsigned __int64 virtual_address)
	{
		unsigned __int64 physical_address = MmGetPhysicalAddress((void*)virtual_address).QuadPart;
		if (physical_address == 0)
		{
			return false;
		}

		// 安装/卸载事务由调用方串行化;这里只做 hot path 与删除路径之间的引用隔离。
		// 页面正在删除时 find_hooked_page_for_pfn 不会返回记录,热路径也不可能再取得引用。
		__ept_hooked_page_info* hooked_page_info =
			ept::find_hooked_page_for_pfn(ept_state, GET_PFN(physical_address), EPTO_HOOK_FUNCTION);
		if (hooked_page_info == nullptr)
		{
			return false;
		}

		__ept_hooked_function_info* hooked_function_info = nullptr;
		spinlock::lock(&hooked_page_info->functions_lock);
		for (PLIST_ENTRY current_hooked_function = hooked_page_info->hooked_functions_list.Flink;
			&hooked_page_info->hooked_functions_list != current_hooked_function;
			current_hooked_function = current_hooked_function->Flink)
		{
			__ept_hooked_function_info* candidate =
				CONTAINING_RECORD(current_hooked_function, __ept_hooked_function_info, hooked_function_list);

			if (candidate->removing == 0 &&
				MASK_EPT_PML1_OFFSET(virtual_address) ==
					MASK_EPT_PML1_OFFSET(reinterpret_cast<unsigned __int64>(candidate->virtual_address)))
			{
				hooked_function_info = candidate;
				InterlockedExchange(&hooked_function_info->removing, 1);
				break;
			}
		}
		spinlock::unlock(&hooked_page_info->functions_lock);

		if (hooked_function_info == nullptr)
		{
			return false;
		}

		ept::wait_for_function_quiescence(hooked_function_info);

		const unsigned __int64 function_page_offset = MASK_EPT_PML1_OFFSET(virtual_address);
		if (hooked_function_info->fake_page_contents != nullptr &&
			hooked_function_info->virtual_address != nullptr &&
			hooked_function_info->hook_size != 0 &&
			function_page_offset < PAGE_SIZE &&
			hooked_function_info->hook_size <= PAGE_SIZE - function_page_offset)
		{
			RtlCopyMemory(
				&hooked_function_info->fake_page_contents[function_page_offset],
				hooked_function_info->virtual_address,
				hooked_function_info->hook_size);
		}
		void* const second_trampoline_address = hooked_function_info->second_trampoline_address;

		ept::remove_hooked_function(hooked_page_info, hooked_function_info);

		const unsigned __int8* first_trampoline_address = hooked_function_info->first_trampoline_address;
		pool_manager::release_pool(hooked_function_info);

		if (!IsListEmpty(&hooked_page_info->hooked_functions_list))
		{
			if (first_trampoline_address != nullptr)
			{
				pool_manager::release_pool(const_cast<unsigned __int8*>(first_trampoline_address));
			}

			// 修改了伪页内容,刷新当前 EPT 上下文,避免旧指令缓存继续使用已恢复的字节。
			_mm_mfence();
			if (ept_state.ept_pointer != nullptr)
			{
				invept_single_context_func(ept_state.ept_pointer->all);
			}

			if (second_trampoline_address != nullptr &&
				reinterpret_cast<unsigned __int64>(second_trampoline_address) != virtual_address)
			{
				(void)unhook_function(ept_state, reinterpret_cast<unsigned __int64>(second_trampoline_address));
			}
			return true;
		}

		// 先取出 owner_split,再释放 hooked_page_info。
		__ept_dynamic_split* owner_split = hooked_page_info->owner_split;
		if (owner_split == nullptr)
		{
			owner_split = ept_get_split_from_pml1(hooked_page_info->entry_address);
		}

		InterlockedExchange(&hooked_page_info->removing, 1);
		if (ept_state.page_to_change == hooked_page_info)
		{
			hv::clear_pending_mtf_metadata(&ept_state);
		}
		ept::wait_for_page_quiescence(hooked_page_info);

		if (hooked_page_info->entry_address != nullptr)
		{
			hooked_page_info->original_entry.execute = 1;
			swap_pml1_and_invalidate_tlb(
				ept_state,
				hooked_page_info->entry_address,
				hooked_page_info->original_entry,
				invept_single_context);
		}

		ept::remove_hooked_page(ept_state, hooked_page_info);

		if (first_trampoline_address != nullptr)
		{
			pool_manager::release_pool(const_cast<unsigned __int8*>(first_trampoline_address));
		}
		pool_manager::release_pool(hooked_page_info);

		// watch 页记录被删除后,对 split 做 ref--,必要时合并回 2MB 并回收。
		ept_split_ref_dec_and_maybe_release(ept_state, owner_split);

		if (second_trampoline_address != nullptr &&
			reinterpret_cast<unsigned __int64>(second_trampoline_address) != virtual_address)
		{
			(void)unhook_function(ept_state, reinterpret_cast<unsigned __int64>(second_trampoline_address));
		}
		return true;
	}

	/// <summary>
	/// Unhook all functions and invalidate tlb
	/// </summary>
	void unhook_all_functions(__ept_state& ept_state)
	{
		PLIST_ENTRY current_hooked_page = ept_state.hooked_page_list.Flink;
		while (&ept_state.hooked_page_list != current_hooked_page)
		{
			__ept_hooked_page_info* hooked_entry =
				CONTAINING_RECORD(current_hooked_page, __ept_hooked_page_info, hooked_page_list);
			PLIST_ENTRY next_hooked_page = current_hooked_page->Flink;

			// 先禁止新的 hot path 引用和 MTF 引用,再等待已有引用退出。
			InterlockedExchange(&hooked_entry->removing, 1);
			if (ept_state.page_to_change == hooked_entry)
			{
				hv::clear_pending_mtf_metadata(&ept_state);
				hv::set_mtf(false);
			}
			ept::wait_for_page_quiescence(hooked_entry);

			// 页面引用已经归零后,函数引用必然也已归零。这里仍显式标记 removing,
			// 防止 future 热路径在页面上建立新的函数引用。
			{
				spinlock::lock(&hooked_entry->functions_lock);
				for (PLIST_ENTRY current_hooked_function = hooked_entry->hooked_functions_list.Flink;
					&hooked_entry->hooked_functions_list != current_hooked_function;
					current_hooked_function = current_hooked_function->Flink)
				{
					__ept_hooked_function_info* hooked_function_info =
						CONTAINING_RECORD(current_hooked_function, __ept_hooked_function_info, hooked_function_list);
					InterlockedExchange(&hooked_function_info->removing, 1);
				}
				spinlock::unlock(&hooked_entry->functions_lock);
			}

			PLIST_ENTRY current_hooked_function = hooked_entry->hooked_functions_list.Flink;
			while (&hooked_entry->hooked_functions_list != current_hooked_function)
			{
				__ept_hooked_function_info* hooked_function_info =
					CONTAINING_RECORD(current_hooked_function, __ept_hooked_function_info, hooked_function_list);
				PLIST_ENTRY next_hooked_function = current_hooked_function->Flink;

				ept::wait_for_function_quiescence(hooked_function_info);

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

				ept::remove_hooked_function(hooked_entry, hooked_function_info);
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

			if (hooked_entry->entry_address != nullptr)
			{
				swap_pml1_and_invalidate_tlb(
					ept_state,
					hooked_entry->entry_address,
					restored_entry,
					invept_single_context);
			}

			ept::remove_hooked_page(ept_state, hooked_entry);

			__ept_dynamic_split* owner_split = hooked_entry->owner_split;
			if (owner_split == nullptr)
			{
				owner_split = ept_get_split_from_pml1(hooked_entry->entry_address);
			}

			// 批量卸载不能释放单页记录/跳板/伪页:driver unload 后 pool manager 会整池释放,
			// 且其它 CPU 可能仍有陈旧指令取指或 pending MTF。split 引用则跟随页面记录撤销。
			ept_split_ref_dec_and_maybe_release(ept_state, owner_split);

			current_hooked_page = next_hooked_page;
		}
	}
}

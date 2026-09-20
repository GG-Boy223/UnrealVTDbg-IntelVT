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

		if (!IsListEmpty(&hooked_page_info->hooked_functions_list))
		{
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
			// 函数记录已在待回收链上；引用归零后由安全点统一释放。
			ept::reclaim_retired_resources(ept_state);
			return true;
		}

		InterlockedExchange(&hooked_page_info->removing, 1);
		if (ept_state.page_to_change == hooked_page_info)
		{
			hv::clear_pending_mtf_metadata(&ept_state);
			hv::set_mtf(false);
		}

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

		ept::reclaim_retired_resources(ept_state);

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

			// 先禁止新的 hot path 引用和 MTF 引用，再把记录摘到待回收链。
			InterlockedExchange(&hooked_entry->removing, 1);
			if (ept_state.page_to_change == hooked_entry)
			{
				hv::clear_pending_mtf_metadata(&ept_state);
				hv::set_mtf(false);
			}
			spinlock::lock(&hooked_entry->functions_lock);
			PLIST_ENTRY current_hooked_function = hooked_entry->hooked_functions_list.Flink;
			while (&hooked_entry->hooked_functions_list != current_hooked_function)
			{
				__ept_hooked_function_info* hooked_function_info =
					CONTAINING_RECORD(current_hooked_function, __ept_hooked_function_info, hooked_function_list);
				PLIST_ENTRY next_hooked_function = current_hooked_function->Flink;

				InterlockedExchange(&hooked_function_info->removing, 1);
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

				RemoveEntryList(&hooked_function_info->hooked_function_list);
				InsertTailList(
					&hooked_entry->retired_functions_list,
					&hooked_function_info->hooked_function_list);
				current_hooked_function = next_hooked_function;
			}
			spinlock::unlock(&hooked_entry->functions_lock);

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

			current_hooked_page = next_hooked_page;
		}

		// 引用归零的记录在安全点回收；仍有热引用的记录留在待回收链，
		// 由后续 VM-exit 再次尝试，根态不再无上限等待。
		ept::reclaim_retired_resources(ept_state);
	}
}

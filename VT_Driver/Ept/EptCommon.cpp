#include "../Driver.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../msr.h"
#include "../AllocateMem.h"
#include "../vmx.h"
#include "../hypervisor_routines.h"
#include "../Vmcs/VmcsApi.h"
#include "EptCommon.h"

namespace ept
{
	// 常量已在EptCommon.h中定义，此处删除重复定义

	unsigned int normalize_vcpu_count(int requested_count)
	{
		if (requested_count <= 0 || g_vmm_context.vcpu == nullptr || g_vmm_context.processor_count == 0)
		{
			return 0;
		}

		unsigned int count = static_cast<unsigned int>(requested_count);
		if (count != g_vmm_context.processor_count)
		{
			return 0;
		}

		return count;
	}

	bool is_valid_watch_range(unsigned __int64 virtual_address, unsigned __int64 size)
	{
		if (size == 0 || size > kMaxWatchBytes)
		{
			return false;
		}

		return virtual_address + size >= virtual_address;
	}

	void initialize_hooked_page_runtime_fields(__ept_hooked_page_info* hooked_page_info)
	{
		if (hooked_page_info == nullptr)
		{
			return;
		}

		hooked_page_info->ID = -1;
		hooked_page_info->isBp = false;
		hooked_page_info->isInt3 = false;
		hooked_page_info->last_watch_id = -1;
		hooked_page_info->fake_page_synced = false;
		hooked_page_info->fake_page_needs_full_sync = false;
		hooked_page_info->mtf_reference_count = 0;
		hooked_page_info->removing = 0;
		hooked_page_info->hot_reference_count = 0;
		hooked_page_info->functions_lock = 0;
		InitializeListHead(&hooked_page_info->hooked_functions_list);
		InitializeListHead(&hooked_page_info->retired_functions_list);
	}

	void initialize_hooked_function_runtime_fields(__ept_hooked_function_info* hooked_function_info)
	{
		if (hooked_function_info == nullptr)
		{
			return;
		}

		hooked_function_info->removing = 0;
		hooked_function_info->hot_reference_count = 0;
	}

	bool is_watch_id_valid(int ID)
	{
		return ID >= 0 && ID < EPTWATCHLISTSIZE;
	}

	__ept_hooked_page_info* acquire_page_hot_reference(
		__ept_state& ept_state,
		unsigned __int64 pfn,
		int required_options)
	{
		spinlock::lock(&ept_state.hooked_page_list_lock);

		for (PLIST_ENTRY current = ept_state.hooked_page_list.Flink;
			current != &ept_state.hooked_page_list;
			current = current->Flink)
		{
			__ept_hooked_page_info* hooked_page_info =
				CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

			if (hooked_page_info->pfn_of_hooked_page != pfn ||
				(required_options != 0 &&
					hooked_page_info->Options != required_options) ||
				hooked_page_info->removing != 0)
			{
				continue;
			}

			InterlockedIncrement(&hooked_page_info->hot_reference_count);
			if (hooked_page_info->removing == 0)
			{
				spinlock::unlock(&ept_state.hooked_page_list_lock);
				return hooked_page_info;
			}

			InterlockedDecrement(&hooked_page_info->hot_reference_count);
			spinlock::unlock(&ept_state.hooked_page_list_lock);
			return nullptr;
		}

		spinlock::unlock(&ept_state.hooked_page_list_lock);
		return nullptr;
	}

	void release_page_hot_reference(
		__ept_state& ept_state,
		__ept_hooked_page_info* hooked_page_info)
	{
		if (hooked_page_info == nullptr)
		{
			return;
		}

		UNREFERENCED_PARAMETER(ept_state);
		InterlockedDecrement(&hooked_page_info->hot_reference_count);
	}
	__ept_hooked_page_info* find_hooked_page_for_pfn(
		__ept_state& ept_state,
		unsigned __int64 pfn,
		int required_options)
	{
		spinlock::lock(&ept_state.hooked_page_list_lock);
		for (PLIST_ENTRY current = ept_state.hooked_page_list.Flink;
			current != &ept_state.hooked_page_list;
			current = current->Flink)
		{
			__ept_hooked_page_info* hooked_page_info =
				CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

			if (hooked_page_info->pfn_of_hooked_page != pfn ||
				(required_options != 0 &&
					hooked_page_info->Options != required_options) ||
				hooked_page_info->removing != 0)
			{
				continue;
			}

			spinlock::unlock(&ept_state.hooked_page_list_lock);
			return hooked_page_info;
		}

		spinlock::unlock(&ept_state.hooked_page_list_lock);
		return nullptr;
	}

	void insert_hooked_page(
		__ept_state& ept_state,
		__ept_hooked_page_info* hooked_page_info)
	{
		if (hooked_page_info == nullptr)
		{
			return;
		}

		spinlock::lock(&ept_state.hooked_page_list_lock);
		InsertHeadList(&ept_state.hooked_page_list, &hooked_page_info->hooked_page_list);
		spinlock::unlock(&ept_state.hooked_page_list_lock);
	}

	void remove_hooked_page(
		__ept_state& ept_state,
		__ept_hooked_page_info* hooked_page_info)
	{
		if (hooked_page_info == nullptr)
		{
			return;
		}

		spinlock::lock(&ept_state.hooked_page_list_lock);
		RemoveEntryList(&hooked_page_info->hooked_page_list);
		InsertTailList(&ept_state.retired_page_list, &hooked_page_info->hooked_page_list);
		spinlock::unlock(&ept_state.hooked_page_list_lock);
	}
	__ept_hooked_function_info* acquire_function_hot_reference(
		__ept_hooked_page_info* hooked_page_info,
		unsigned __int64 virtual_address)
	{
		if (hooked_page_info == nullptr)
		{
			return nullptr;
		}

		spinlock::lock(&hooked_page_info->functions_lock);
		for (PLIST_ENTRY current = hooked_page_info->hooked_functions_list.Flink;
			current != &hooked_page_info->hooked_functions_list;
			current = current->Flink)
		{
			__ept_hooked_function_info* hooked_function_info =
				CONTAINING_RECORD(
					current,
					__ept_hooked_function_info,
					hooked_function_list);

			if (hooked_function_info->virtual_address != (PVOID)virtual_address ||
				hooked_function_info->removing != 0)
			{
				continue;
			}

			InterlockedIncrement(&hooked_function_info->hot_reference_count);
			if (hooked_function_info->removing == 0)
			{
				spinlock::unlock(&hooked_page_info->functions_lock);
				return hooked_function_info;
			}

			InterlockedDecrement(&hooked_function_info->hot_reference_count);
			spinlock::unlock(&hooked_page_info->functions_lock);
			return nullptr;
		}

		spinlock::unlock(&hooked_page_info->functions_lock);
		return nullptr;
	}

	void release_function_hot_reference(
		__ept_hooked_function_info* hooked_function_info)
	{
		if (hooked_function_info == nullptr)
		{
			return;
		}

		InterlockedDecrement(&hooked_function_info->hot_reference_count);
	}

	__ept_hooked_page_info* acquire_any_page_hot_reference(
		__ept_state& ept_state,
		unsigned __int64 pfn)
	{
		spinlock::lock(&ept_state.hooked_page_list_lock);

		for (PLIST_ENTRY current = ept_state.hooked_page_list.Flink;
			current != &ept_state.hooked_page_list;
			current = current->Flink)
		{
			__ept_hooked_page_info* hooked_page_info =
				CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

			if (hooked_page_info->pfn_of_hooked_page != pfn ||
				hooked_page_info->removing != 0)
			{
				continue;
			}

			InterlockedIncrement(&hooked_page_info->hot_reference_count);
			if (hooked_page_info->removing == 0)
			{
				spinlock::unlock(&ept_state.hooked_page_list_lock);
				return hooked_page_info;
			}

			InterlockedDecrement(&hooked_page_info->hot_reference_count);
			spinlock::unlock(&ept_state.hooked_page_list_lock);
			return nullptr;
		}

		spinlock::unlock(&ept_state.hooked_page_list_lock);
		return nullptr;
	}

	void insert_hooked_function(
		__ept_hooked_page_info* hooked_page_info,
		__ept_hooked_function_info* hooked_function_info)
	{
		if (hooked_page_info == nullptr || hooked_function_info == nullptr)
		{
			return;
		}

		spinlock::lock(&hooked_page_info->functions_lock);
		InsertHeadList(
			&hooked_page_info->hooked_functions_list,
			&hooked_function_info->hooked_function_list);
		spinlock::unlock(&hooked_page_info->functions_lock);
	}

	void remove_hooked_function(
		__ept_hooked_page_info* hooked_page_info,
		__ept_hooked_function_info* hooked_function_info)
	{
		if (hooked_page_info == nullptr || hooked_function_info == nullptr)
		{
			return;
		}

		spinlock::lock(&hooked_page_info->functions_lock);
		RemoveEntryList(&hooked_function_info->hooked_function_list);
		InsertTailList(
			&hooked_page_info->retired_functions_list,
			&hooked_function_info->hooked_function_list);
		spinlock::unlock(&hooked_page_info->functions_lock);
	}

	static void reclaim_retired_functions(__ept_hooked_page_info* hooked_page_info)
	{
		if (hooked_page_info == nullptr)
		{
			return;
		}

		for (;;)
		{
			__ept_hooked_function_info* victim = nullptr;
			spinlock::lock(&hooked_page_info->functions_lock);
			for (PLIST_ENTRY current = hooked_page_info->retired_functions_list.Flink;
				current != &hooked_page_info->retired_functions_list;
				current = current->Flink)
			{
				__ept_hooked_function_info* candidate =
					CONTAINING_RECORD(
						current,
						__ept_hooked_function_info,
						hooked_function_list);

				if (InterlockedCompareExchange(
					&candidate->hot_reference_count, 0, 0) == 0)
				{
					RemoveEntryList(&candidate->hooked_function_list);
					victim = candidate;
					break;
				}
			}
			spinlock::unlock(&hooked_page_info->functions_lock);

			if (victim == nullptr)
			{
				return;
			}

			unsigned __int8* const first_trampoline_address =
				victim->first_trampoline_address;
			pool_manager::release_pool(victim);
			if (first_trampoline_address != nullptr)
			{
				pool_manager::release_pool(first_trampoline_address);
			}
		}
	}

	void reclaim_retired_resources(__ept_state& ept_state)
	{
		for (;;)
		{
			__ept_hooked_page_info* victim = nullptr;
			__ept_dynamic_split* owner_split = nullptr;

			spinlock::lock(&ept_state.hooked_page_list_lock);

			// 活动页上的函数记录也可能已摘到待回收链，先按页清理它们。
			for (PLIST_ENTRY current = ept_state.hooked_page_list.Flink;
				current != &ept_state.hooked_page_list;
				current = current->Flink)
			{
				__ept_hooked_page_info* hooked_page_info =
					CONTAINING_RECORD(
						current,
						__ept_hooked_page_info,
						hooked_page_list);
				reclaim_retired_functions(hooked_page_info);
			}

			for (PLIST_ENTRY current = ept_state.retired_page_list.Flink;
				current != &ept_state.retired_page_list;
				current = current->Flink)
			{
				__ept_hooked_page_info* hooked_page_info =
					CONTAINING_RECORD(
						current,
						__ept_hooked_page_info,
						hooked_page_list);

				reclaim_retired_functions(hooked_page_info);

				if (InterlockedCompareExchange(
					&hooked_page_info->hot_reference_count, 0, 0) != 0 ||
					InterlockedCompareExchange(
						&hooked_page_info->mtf_reference_count, 0, 0) != 0)
				{
					continue;
				}

				spinlock::lock(&hooked_page_info->functions_lock);
				const bool functions_empty =
					IsListEmpty(&hooked_page_info->hooked_functions_list) &&
					IsListEmpty(&hooked_page_info->retired_functions_list);
				spinlock::unlock(&hooked_page_info->functions_lock);

				if (!functions_empty)
				{
					continue;
				}

				RemoveEntryList(&hooked_page_info->hooked_page_list);
				owner_split = hooked_page_info->owner_split;
				victim = hooked_page_info;
				break;
			}

			spinlock::unlock(&ept_state.hooked_page_list_lock);

			if (victim == nullptr)
			{
				return;
			}

			pool_manager::release_pool(victim);
			ept_split_ref_dec_and_maybe_release(ept_state, owner_split);
		}
	}
}

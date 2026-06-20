#include "../../EptHook.h"
#include "../../EptBuild.h"
#include "../../EptSync.h"
#include "../../EptCommon.h"
#include "../../../Globals.h"
#include "../../../EPT.h"
#include "../../../poolmanager.h"

namespace ept
{
	//int3 hook
	bool cc_hook_function(__ept_state& ept_state, void* target_function/*被挂钩的函数地址*/, void* proxy_function/*新函数地址*/, void** origin_function)
	{
		unsigned __int64 physical_address = MmGetPhysicalAddress(target_function).QuadPart;

		//
		// 检测该函数是否存在于物理内存中
		// 如果该函数的物理地址为空
		// 说明该函数不存在于物理内存中
		//
		if (physical_address == NULL)
		{
			LogError("请求的虚拟内存在物理内存中不存在");
			return false;
		}

		//
		// 检查页面是否尚未挂钩
		//
		PLIST_ENTRY current = &ept_state.hooked_page_list;
		while (&ept_state.hooked_page_list != current->Flink)
		{
			current = current->Flink;
			//从列表中取出挂钩页
			__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

			//判断目标地址是否是被挂钩的页
			//仅比较页帧号
			if (hooked_page_info->pfn_of_hooked_page == GET_PFN(physical_address))
			{
				__ept_hooked_function_info* hooked_function_info = pool_manager::request_pool<__ept_hooked_function_info*>(pool_manager::INTENTION_TRACK_HOOKED_FUNCTIONS, TRUE, sizeof(__ept_hooked_function_info));
				if (hooked_function_info == nullptr)
				{
					LogError("挂钩函数结构没有预先分配池内存");
					return false;
				}

				//
				// If we are hooking code cave for second trampoline
				// then origin function in null and we don't have to get pool for trampoline
				//
				if (origin_function != nullptr)
				{
					hooked_function_info->first_trampoline_address = pool_manager::request_pool<unsigned __int8*>(pool_manager::INTENTION_EXEC_TRAMPOLINE, TRUE, 100);
					if (hooked_function_info->first_trampoline_address == nullptr)
					{
						pool_manager::release_pool(hooked_function_info);
						LogError("There is no pre-allocated pool for trampoline");
						return false;
					}
				}

				hooked_function_info->virtual_address = target_function;
				hooked_function_info->breakpoint_address = NULL;

				hooked_function_info->fake_page_contents = hooked_page_info->fake_page_contents;

				if (write_cc_instruction_to_memory(hooked_function_info, target_function, proxy_function, origin_function) == false)
				{
					if (hooked_function_info->first_trampoline_address != nullptr)
						pool_manager::release_pool(hooked_function_info->first_trampoline_address);
					pool_manager::release_pool(hooked_function_info);
					LogError("Hook failed");
					return false;
				}

				// Track all hooked functions within page
				InsertHeadList(&hooked_page_info->hooked_functions_list, &hooked_function_info->hooked_function_list);

				return true;
			}
		}

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
		}

		__ept_pte* target_page = get_pml1_entry(ept_state, physical_address);
		if (target_page == nullptr)
		{
			LogError("Failed to get PML1 entry of the target address");
			return false;
		}

		__ept_hooked_page_info* hooked_page_info = pool_manager::request_pool<__ept_hooked_page_info*>(pool_manager::INTENTION_TRACK_HOOKED_PAGES, true, sizeof(__ept_hooked_page_info));
		if (hooked_page_info == nullptr)
		{
			LogError("There is no preallocated pool for hooked page info");
			return false;
		}

		InitializeListHead(&hooked_page_info->hooked_functions_list);
		initialize_hooked_page_runtime_fields(hooked_page_info);

		__ept_hooked_function_info* hooked_function_info = pool_manager::request_pool<__ept_hooked_function_info*>(pool_manager::INTENTION_TRACK_HOOKED_FUNCTIONS, true, sizeof(__ept_hooked_function_info));
		if (hooked_function_info == nullptr)
		{
			pool_manager::release_pool(hooked_page_info);
			LogError("hook函数时预分配的缓冲已耗尽");
			return false;
		}

		//
		// If we are hooking code cave for second trampoline
		// then origin function in null and we don't have to get pool for trampoline
		//
		if (origin_function != nullptr)
		{
			hooked_function_info->first_trampoline_address = pool_manager::request_pool<unsigned __int8*>(pool_manager::INTENTION_EXEC_TRAMPOLINE, TRUE, 100);
			if (hooked_function_info->first_trampoline_address == nullptr)
			{
				pool_manager::release_pool(hooked_page_info);
				pool_manager::release_pool(hooked_function_info);
				LogError("There is no pre-allocated pool for trampoline");
				return false;
			}
		}

		hooked_page_info->pfn_of_hooked_page = GET_PFN(physical_address);
		hooked_page_info->pfn_of_fake_page_contents = GET_PFN(MmGetPhysicalAddress(hooked_page_info->fake_page_contents).QuadPart);
		hooked_page_info->entry_address = target_page;

		//先改原页为不可执行 回到guest后执行就触发vmexit
		//进入vmm后交换伪造页
		hooked_page_info->entry_address->execute = 0;
		hooked_page_info->entry_address->read = 1;
		hooked_page_info->entry_address->write = 1;

		hooked_page_info->original_entry = *target_page;
		hooked_page_info->changed_entry = *target_page;

		//伪造页不得读写
		hooked_page_info->changed_entry.read = 0;
		hooked_page_info->changed_entry.write = 0;
		hooked_page_info->changed_entry.execute = 1;

		//将伪造页换上去
		hooked_page_info->changed_entry.page_frame_number = hooked_page_info->pfn_of_fake_page_contents;

		RtlCopyMemory(&hooked_page_info->fake_page_contents, PAGE_ALIGN(target_function), PAGE_SIZE);

		hooked_function_info->virtual_address = target_function;
		hooked_function_info->breakpoint_address = NULL;

		hooked_function_info->fake_page_contents = hooked_page_info->fake_page_contents;

		//开始hook
		if (write_cc_instruction_to_memory(hooked_function_info, target_function, proxy_function, origin_function) == false)
		{
			if (hooked_function_info->first_trampoline_address != nullptr)
				pool_manager::release_pool(hooked_function_info->first_trampoline_address);
			pool_manager::release_pool(hooked_function_info);
			pool_manager::release_pool(hooked_page_info);
			LogError("Hook failed");
			return false;
		}

		//记录hook的函数和页 以便日后释放
		// Track all hooked functions
		InsertHeadList(&hooked_page_info->hooked_functions_list, &hooked_function_info->hooked_function_list);

		// Track all hooked pages
		InsertHeadList(&ept_state.hooked_page_list, &hooked_page_info->hooked_page_list);

		_mm_mfence();
		invept_single_context_func(ept_state.ept_pointer->all);

		return true;
	}
}

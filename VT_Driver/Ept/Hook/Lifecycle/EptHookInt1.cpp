#include "../../EptHook.h"
#include "../../EptBuild.h"
#include "../../EptSync.h"
#include "../../EptCommon.h"
#include "../../../Globals.h"
#include "../../../EPT.h"
#include "../../../poolmanager.h"

namespace ept
{
	//#DB hook
	bool int1_hook_function(__ept_state& ept_state, void* target_function, void* proxy_function, void** origin_function)
	{
		//unsigned __int64 physical_address = hv::get_physical_address(target_cr3, (PVOID)target_function);
		unsigned __int64 physical_address = MmGetPhysicalAddress(target_function).QuadPart;
		// 检测该函数是否存在于物理内存中
		// 如果该函数的物理地址为空
		// 说明该函数不存在于物理内存中
		//
		if (physical_address == NULL)
		{
			LogError("请求的虚拟内存在物理内存中不存在");
			return false;
		}

		// 已挂钩页面通过热引用固定记录，池分配和指令写入不再依赖页面链锁。
		__ept_hooked_page_info* existing_hooked_page_info =
			ept::acquire_any_page_hot_reference(ept_state, GET_PFN(physical_address));
		if (existing_hooked_page_info != nullptr)
		{
			__ept_hooked_function_info* hooked_function_info = pool_manager::request_pool<__ept_hooked_function_info*>(
				pool_manager::INTENTION_TRACK_HOOKED_FUNCTIONS,
				TRUE,
				sizeof(__ept_hooked_function_info));
			if (hooked_function_info == nullptr)
			{
				LogError("挂钩函数结构没有预先分配池内存");
				ept::release_page_hot_reference(ept_state, existing_hooked_page_info);
				return false;
			}

			// If we are hooking code cave for second trampoline
			// then origin function in null and we don't have to get pool for trampoline
			//
			if (origin_function != nullptr)
			{
				hooked_function_info->first_trampoline_address = pool_manager::request_pool<unsigned __int8*>(
					pool_manager::INTENTION_EXEC_TRAMPOLINE,
					TRUE,
					kTrampolineBufferSize);
				if (hooked_function_info->first_trampoline_address == nullptr)
				{
					pool_manager::release_pool(hooked_function_info);
					LogError("There is no pre-allocated pool for trampoline");
					ept::release_page_hot_reference(ept_state, existing_hooked_page_info);
					return false;
				}
			}

			initialize_hooked_function_runtime_fields(hooked_function_info);
			hooked_function_info->virtual_address = target_function;
			hooked_function_info->breakpoint_address = NULL;
			hooked_function_info->fake_page_contents = existing_hooked_page_info->fake_page_contents;

			//开始hook
			if (write_int1_instruction_to_memory(hooked_function_info, target_function, proxy_function, origin_function) == false)
			{
				if (hooked_function_info->first_trampoline_address != nullptr)
				{
					pool_manager::release_pool(hooked_function_info->first_trampoline_address);
				}
				pool_manager::release_pool(hooked_function_info);
				ept::release_page_hot_reference(ept_state, existing_hooked_page_info);
				LogError("Hook failed");
				return false;
			}

			// Track all hooked functions within page
			ept::insert_hooked_function(existing_hooked_page_info, hooked_function_info);
			ept::release_page_hot_reference(ept_state, existing_hooked_page_info);
			return true;
		}

		//判断2mb pde是否已经进行分割
		__ept_dynamic_split* runtime_split = nullptr;
		if (!prepare_pml2_for_address(ept_state, physical_address))
		{
			LogError("Failed to prepare PML2 for INT1 hook");
			return false;
		}

		if (is_page_splitted(ept_state, physical_address) == false)
		{
			void* split_buffer = pool_manager::request_pool<void*>(
				pool_manager::INTENTION_SPLIT_PML2,
				true,
				sizeof(__ept_dynamic_split));
			if (split_buffer == nullptr)
			{
				LogError("There is no preallocated pool for split");
				return false;
			}

			//将2MB页面分割为512个4KB页面
			const auto mtrrs = hv::read_mtrr_data();
			if (split_pml2(mtrrs, ept_state, split_buffer, physical_address) == false)
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
			// 如果本次刚做了 runtime split 但未成功建立监视记录,则尝试回收 split。
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}

			LogError("Failed to get PML1 entry of the target address");
			return false;
		}

		__ept_hooked_page_info* hooked_page_info = pool_manager::request_pool<__ept_hooked_page_info*>(
			pool_manager::INTENTION_TRACK_HOOKED_PAGES,
			true,
			sizeof(__ept_hooked_page_info));
		if (hooked_page_info == nullptr)
		{
			// 如果本次刚做了 runtime split 但未成功建立监视记录,则尝试回收 split。
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}

			LogError("There is no preallocated pool for hooked page info");
			return false;
		}

		InitializeListHead(&hooked_page_info->hooked_functions_list);
		initialize_hooked_page_runtime_fields(hooked_page_info);

		__ept_hooked_function_info* hooked_function_info = pool_manager::request_pool<__ept_hooked_function_info*>(
			pool_manager::INTENTION_TRACK_HOOKED_FUNCTIONS,
			true,
			sizeof(__ept_hooked_function_info));
		if (hooked_function_info == nullptr)
		{
			// 关键修复：失败路径要回收 hooked_page_info（旧代码这里直接 return false，导致池对象泄漏）。
			pool_manager::release_pool(hooked_page_info);

			// 如果本次刚做了 runtime split 但未成功建立监视记录,则尝试回收 split。
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}

			LogError("hook函数时预分配的缓冲已耗尽");
			return false;
		}

		// If we are hooking code cave for second trampoline
		// then origin function in null and we don't have to get pool for trampoline
		//
		if (origin_function != nullptr)
		{
			hooked_function_info->first_trampoline_address = pool_manager::request_pool<unsigned __int8*>(
				pool_manager::INTENTION_EXEC_TRAMPOLINE,
				TRUE,
				kTrampolineBufferSize);
			if (hooked_function_info->first_trampoline_address == nullptr)
			{
				// 关键修复：失败路径要回收 hooked_page_info（旧代码这里直接 return false，导致池对象泄漏）。
				pool_manager::release_pool(hooked_page_info);
				pool_manager::release_pool(hooked_function_info);

				// 如果本次刚做了 runtime split 但未成功建立监视记录,则尝试回收 split。
				if (runtime_split != nullptr)
				{
					ept_split_try_merge_and_release(ept_state, runtime_split);
				}

				LogError("There is no pre-allocated pool for trampoline");
				return false;
			}
		}

		hooked_page_info->pfn_of_hooked_page = GET_PFN(physical_address); //获取guest物理页面的页帧号
		hooked_page_info->pfn_of_fake_page_contents = GET_PFN(MmGetPhysicalAddress(hooked_page_info->fake_page_contents).QuadPart);
		hooked_page_info->entry_address = target_page; //ept_pte
		hooked_page_info->owner_split = ept_get_split_from_pml1(target_page);
		//先改原页为不可执行 回到guest后执行就触发vmexit
		//进入vmm后交换伪造页
		hooked_page_info->entry_address->execute = 0;
		hooked_page_info->entry_address->read = 1;
		hooked_page_info->entry_address->write = 1;
		hooked_page_info->original_entry = *target_page; //指向原
		hooked_page_info->changed_entry = *target_page;
		hooked_page_info->changed_entry.read = 0;
		hooked_page_info->changed_entry.write = 0;
		hooked_page_info->changed_entry.execute = 1;
		//将伪造页换上去
		hooked_page_info->changed_entry.page_frame_number = hooked_page_info->pfn_of_fake_page_contents;

		RtlCopyMemory(&hooked_page_info->fake_page_contents, PAGE_ALIGN(target_function), PAGE_SIZE);

		initialize_hooked_function_runtime_fields(hooked_function_info);
		hooked_function_info->virtual_address = target_function;
		hooked_function_info->breakpoint_address = NULL;
		hooked_function_info->fake_page_contents = hooked_page_info->fake_page_contents;

		//开始hook
		if (write_int1_instruction_to_memory(hooked_function_info, target_function, proxy_function, origin_function) == false)
		{
			target_page->read = 1;
			target_page->write = 1;
			target_page->execute = 1;
			if (hooked_function_info->first_trampoline_address != nullptr)
			{
				pool_manager::release_pool(hooked_function_info->first_trampoline_address);
			}
			pool_manager::release_pool(hooked_function_info);
			// 关键修复：失败路径要回收 hooked_page_info（旧代码这里直接 return false，导致池对象泄漏）。
			pool_manager::release_pool(hooked_page_info);

			// 如果本次刚做了 runtime split 但未成功建立监视记录,则尝试回收 split。
			if (runtime_split != nullptr)
			{
				ept_split_try_merge_and_release(ept_state, runtime_split);
			}

			_mm_mfence();
			invept_single_context_func(ept_state.ept_pointer->all);
			LogError("Hook failed");
			return false;
		}

		//记录hook的函数和页 以便日后释放
		// Track all hooked functions
		hooked_page_info->Options = EPTO_HOOK_FUNCTION;
		ept::insert_hooked_function(hooked_page_info, hooked_function_info);

		// 引用计数 +1:表示该 split 现在被一个 watch 页面记录占用。
		ept_split_ref_inc(hooked_page_info->owner_split);

		// Track all hooked pages
		ept::insert_hooked_page(ept_state, hooked_page_info);

		_mm_mfence();
		invept_single_context_func(ept_state.ept_pointer->all);
		return true;
	}
}

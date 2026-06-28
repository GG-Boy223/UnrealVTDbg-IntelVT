#include "EptBuildInternal.h"

namespace ept
{
	// 通过任意 PML1 表项地址，反推出所在的 split 结构地址。
	// 原理：__ept_dynamic_split 的首字段就是 4KB 对齐的 pml1[512]，
	// PAGE_ALIGN(pml1_entry) 后得到的页首地址就是 split 结构体起始地址。
	__ept_dynamic_split* ept_get_split_from_pml1(__ept_pte* pml1_entry)
	{
		if (pml1_entry == nullptr)
		{
			return nullptr; // 失败返回空指针
		}

		return reinterpret_cast<__ept_dynamic_split*>(PAGE_ALIGN(pml1_entry));
	}

	// 标记"该 split 是运行期产生的，可回收"。
	// 只应在运行期 watch/hook 路径调用，初始化建表阶段不要设为 true。
	void ept_mark_runtime_split(void* split_buffer)
	{
		if (split_buffer == nullptr)
		{
			return;
		}

		__ept_dynamic_split* split = reinterpret_cast<__ept_dynamic_split*>(split_buffer);
		split->reclaimable_runtime_split = true;
		split->ref_count = 0;
	}

	// 对 split 引用 +1。
	// 只有可回收 split 才参与计数，避免动到初始化阶段固定 split。
	void ept_split_ref_inc(__ept_dynamic_split* split)
	{
		if (split == nullptr)
		{
			return;
		}

		if (!split->reclaimable_runtime_split)
		{
			return;
		}

		InterlockedIncrement(&split->ref_count);
	}

	// 当引用计数归零时，尝试把 4KB split 合并回 2MB 大页并回收到池。
	// 返回 true 表示发生了"合并并回收"；false 表示未回收（还在用或不可回收）。
	// 返回 true 表示发生了“合并并回收”；false 表示未回收（还在用或不可回收）。
	bool ept_split_try_merge_and_release(__ept_state& ept_state, __ept_dynamic_split* split)
	{
		if (split == nullptr)
		{
			return false;
		}

		if (!split->reclaimable_runtime_split)
		{
			return false;
		}

		if (split->ref_count != 0)
		{
			return false;
		}

		__ept_pde* pde = split->entry;
		if (pde == nullptr)
		{
			pool_manager::release_pool(split);
			return true;
		}

		// 把 split 的第 0 个 4KB PTE 还原为 2MB PFN 基址：pfn_2mb = pfn_4kb >> 9。
		__ept_pde merged = { 0 };
		merged.page_directory_entry.read = 1;
		merged.page_directory_entry.write = 1;
		merged.page_directory_entry.execute = 1;
		merged.page_directory_entry.large_page = 1;
		merged.page_directory_entry.memory_type = split->pml1[0].ept_memory_type;
		merged.page_directory_entry.ignore_pat = split->pml1[0].ignore_pat;
		merged.page_directory_entry.suppressve = split->pml1[0].suppress_ve;
		merged.page_directory_entry.page_frame_number = split->pml1[0].page_frame_number >> 9;

		RtlCopyMemory(pde, &merged, sizeof(merged));

		_mm_mfence();
		invept_single_context_func(ept_state.ept_pointer->all);

		pool_manager::release_pool(split);
		return true;
	}

	// 对 split 引用 -1，并在计数归零时触发回收。
	void ept_split_ref_dec_and_maybe_release(__ept_state& ept_state, __ept_dynamic_split* split)
	{
		if (split == nullptr)
		{
			return;
		}

		if (!split->reclaimable_runtime_split)
		{
			return;
		}

		long after = InterlockedDecrement(&split->ref_count);

		// 防御：避免异常路径导致计数跌穿。
		if (after < 0)
		{
			split->ref_count = 0;
			return;
		}

		if (after == 0)
		{
			ept_split_try_merge_and_release(ept_state, split);
		}
	}
}

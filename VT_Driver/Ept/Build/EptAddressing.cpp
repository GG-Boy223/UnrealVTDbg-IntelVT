#include "EptBuildInternal.h"

namespace ept
{
	/// <summary>
	/// Get pml2 entry
	/// </summary>
	/// <param name="physical_address"></param>
	/// <returns> pointer to pml2 </returns>
	__ept_pde* get_pml2_entry(__ept_state& ept_state, unsigned __int64 physical_address)
	{
		unsigned __int64 pml4_index = MASK_EPT_PML4_INDEX(physical_address);
		unsigned __int64 pml3_index = MASK_EPT_PML3_INDEX(physical_address);
		unsigned __int64 pml2_index = MASK_EPT_PML2_INDEX(physical_address);
		unsigned __int64 flat_pdpt_index = (pml4_index << 9) + pml3_index;

		if (ept_state.pml2_pages == nullptr || flat_pdpt_index >= ept_state.mapped_pdpte_count)
		{
			LogError("EPT address is outside identity map: pa=0x%llX flat_pdpt=%llu mapped=%u",
				physical_address,
				flat_pdpt_index,
				ept_state.mapped_pdpte_count);
			return nullptr; // 失败返回空指针
		}

		// 该 1GB 区域由 PDPTE 大页直接映射，没有 PD：不能解引用空指针。
		// 调用方（hook/watch 安装）会按“拿不到 PDE”处理，不会误改页表。
		if (ept_state.pml2_pages[flat_pdpt_index] == nullptr)
		{
			return nullptr; // 失败返回空指针
		}

		//从ept页表中取pde
		return &ept_state.pml2_pages[flat_pdpt_index][pml2_index];
	}

	/// <summary>
	/// 解析GPA取得ept的pte
	/// </summary>
	/// <param name="physical_address"></param>
	/// <returns></returns>
	__ept_pte* get_pml1_entry(__ept_state& ept_state, unsigned __int64 physical_address)
	{
		__ept_pde* pml2 = get_pml2_entry(ept_state, physical_address);
		if (pml2 == nullptr)
		{
			return nullptr; // 失败返回空指针
		}

		if (pml2->page_directory_entry.large_page == 1)
		{
			return nullptr; // 失败返回空指针
		}

		PHYSICAL_ADDRESS pfn;
		pfn.QuadPart = pml2->large_page.page_frame_number << PAGE_SHIFT;
		__ept_pte* pml1 = (__ept_pte*)MmGetVirtualForPhysical(pfn);

		if (pml1 == nullptr)
		{
			return nullptr; // 失败返回空指针
		}

		pml1 = &pml1[MASK_EPT_PML1_INDEX(physical_address)];
		return pml1;
	}

	/// <summary>
	/// Split pml2 into 512 pml1 entries (From one 2MB page to 512 4KB pages)
	/// 将pml2拆分为512个pml1项 (从一个2MB页面到512个4KB页面)
	/// </summary>
	/// <param name="pre_allocated_buffer"> Pre allocated buffer for split </param>
	/// <param name="physical_address"></param>
	/// <returns> status </returns>
	bool split_pml2(mtrr_data const& mtrrs, __ept_state& ept_state, void* pre_allocated_buffer, unsigned __int64 physical_address)
	{
		__ept_pde* pde = get_pml2_entry(ept_state, physical_address);
		if (pde == NULL)
		{
			LogError("Invalid address passed");
			return false;
		}

		//初始化pt页表  // 初始化 split 缓冲：该缓冲由 pool_manager 预分配池提供。
		__ept_dynamic_split* new_split = (__ept_dynamic_split*)pre_allocated_buffer;
		RtlSecureZeroMemory(new_split, sizeof(__ept_dynamic_split));

		// 将所有页面设置为 rwx，以防止不必要的 ept 违规

		 // 记录"这个 split 对应哪个 PDE"，回收合并时需要写回这里。
		// 记录“这个 split 对应哪个 PDE”，回收合并时需要写回这里。
		new_split->entry = pde;

		// 默认按"不可回收"处理，只有运行期 watch/hook 路径会显式标记为可回收。
		// 默认按“不可回收”处理，只有运行期 watch/hook 路径会显式标记为可回收。
		new_split->ref_count = 0;
		new_split->reclaimable_runtime_split = false;

		__ept_pte entry_template = { 0 };
		entry_template.read = 1;
		entry_template.write = 1;
		entry_template.execute = 1;
		entry_template.ept_memory_type = pde->page_directory_entry.memory_type;
		entry_template.ignore_pat = pde->page_directory_entry.ignore_pat;
		entry_template.execute_for_usermode = pde->large_page.execute_for_usermode;
		entry_template.suppress_ve = pde->page_directory_entry.suppressve;

		__stosq((unsigned __int64*)&new_split->pml1[0], entry_template.all, 512);
		for (int i = 0; i < 512; i++)
		{
			//等价pte.page_frame_number = (pde_2mb->page_frame_number << 9) + i;
			unsigned __int64 pfn = ((pde->page_directory_entry.page_frame_number * LARGE_PAGE_SIZE) >> PAGE_SHIFT) + i;
			new_split->pml1[i].page_frame_number = pfn;
			// new_split->pml1[i].ept_memory_type = get_memory_type(pfn, false); // 旧逻辑：4KB split后仍可能把非RAM/MMIO页按默认WB映射。
			new_split->pml1[i].ept_memory_type = get_ept_memory_type_for_range(mtrrs, ept_state, pfn << PAGE_SHIFT, PAGE_SIZE);
			//new_split->pml1[i].ept_memory_type = hv::calc_mtrr_mem_type(mtrrs, pfn << 21, 0x1000 << 9);
		}

		__ept_pde new_entry = { 0 };
		new_entry.large_page.read = 1;
		new_entry.large_page.write = 1;
		new_entry.large_page.execute = 1;
		new_entry.large_page.execute_for_usermode = pde->large_page.execute_for_usermode;

		new_entry.large_page.page_frame_number = MmGetPhysicalAddress(&new_split->pml1[0]).QuadPart >> PAGE_SHIFT;

		// 用"指向 4KB PML1 表"的新条目覆盖原 2MB PDE。
		// 用“指向 4KB PML1 表”的新条目覆盖原 2MB PDE。
		RtlCopyMemory(pde, &new_entry, sizeof(new_entry));

		return true;
	}

	bool is_page_splitted(__ept_state& ept_state, unsigned __int64 physical_address)
	{
		__ept_pde* entry = get_pml2_entry(ept_state, physical_address);
		if (entry == nullptr)
		{
			return false;
		}

		return !entry->page_directory_entry.large_page;
	}
}

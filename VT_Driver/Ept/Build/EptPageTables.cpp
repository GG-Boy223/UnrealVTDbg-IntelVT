#include "EptBuildInternal.h"

namespace ept
{
	/// <summary>
	/// Setup page memory type
	/// 设置页内存类型
	/// </summary>
	/// <param name="entry"> Pointer to pml2 entry </param>
	/// <param name="pfn"> Page frame number </param>
	bool setup_pml2_entry(mtrr_data const& mtrrs, __ept_state& ept_state, __ept_pde& entry, unsigned __int64 pfn)
	{
		entry.page_directory_entry.page_frame_number = pfn;

		//判断是否是大页面
		if (is_valid_for_large_page(pfn) == true)
		{
			entry.page_directory_entry.memory_type = get_memory_type(pfn, true);
			//entry.page_directory_entry.memory_type = hv::calc_mtrr_mem_type(mtrrs, pfn << 21, 0x1000 << 9);
			return true;
		}
		else
		{
			//不是大页面则 说明是PT页表
			//分配PT页表
			void* split_buffer = pool_manager::request_pool<void*>(pool_manager::INTENTION_SPLIT_PML2, true, sizeof(__ept_dynamic_split));
			if (split_buffer == nullptr)
			{
				LogError("Failed to allocate split buffer");
				return false;
			}

			//将2MB页面分割为512个4KB页面
			return split_pml2(mtrrs, ept_state, split_buffer, pfn * LARGE_PAGE_SIZE);
		}
	}

	/// <summary>
	/// Create ept page table
	/// 创建ept页表
	/// </summary>
	/// <returns> status </returns>
	bool create_ept_page_table(__ept_state& ept_state)
	{
		PHYSICAL_ADDRESS max_size;
		max_size.QuadPart = MAXULONG64;

		ept_state.ept_page_table = allocate_pool<__vmm_ept_page_table>();
		if (ept_state.ept_page_table == NULL)
		{
			outDebug("PageTable 内存分配失败。\n");
			return false;
		}

		__vmm_ept_page_table* page_table = ept_state.ept_page_table;
		RtlSecureZeroMemory(page_table, sizeof(__vmm_ept_page_table));

		//
		// Set all pages as rwx to prevent unwanted ept violation
		// 将所有页面设置为 rwx，以防止不必要的 ept 违规
		// 设置第一个 PML4E，使其指向我们的 PDPT
		//
		page_table->pml4[0].page_frame_number = GET_PFN(MmGetPhysicalAddress(&page_table->pml3[0]).QuadPart);
		page_table->pml4[0].read = 1;
		page_table->pml4[0].write = 1;
		page_table->pml4[0].execute = 1;
		page_table->pml4[0].execute_for_usermode = 0;

		__ept_pdpte pdpte_template = { 0 };

		pdpte_template.read = 1;
		pdpte_template.write = 1;
		pdpte_template.execute = 1;
		pdpte_template.execute_for_usermode = 0;

		__stosq((unsigned __int64*)&page_table->pml3[0], pdpte_template.all, 512);

		for (int i = 0; i < 512; i++)
		{
			page_table->pml3[i].page_frame_number = GET_PFN(MmGetPhysicalAddress(&page_table->pml2[i][0]).QuadPart);
		}

		__ept_pde pde_template = { 0 };

		pde_template.page_directory_entry.read = 1;
		pde_template.page_directory_entry.write = 1;
		pde_template.page_directory_entry.execute = 1;

		pde_template.page_directory_entry.large_page = 1;  //使用大页面 2mb
		pde_template.page_directory_entry.execute_for_usermode = 0;

		__stosq((unsigned __int64*)&page_table->pml2[0], pde_template.all, 512 * 512);

		// MTRR data for setting memory types
		// 用于设置内存类型的 MTRR 数据
		auto const mtrrs = hv::read_mtrr_data();
		g_vmm_context.mtrr_info.mtrrs = mtrrs;
		for (int i = 0; i < 512; i++)
		{
			for (int j = 0; j < 512; j++)
			{
				if (setup_pml2_entry(mtrrs, ept_state, page_table->pml2[i][j], (i * 512) + j) == false)
				{
					outDebug("setup_pml2_entry 失败\n");
					return false;
				}
			}
		}

		return true;
	}

	bool create_ept_page_table2(__ept_state& ept_state)
	{
		PHYSICAL_ADDRESS max_size;
		max_size.QuadPart = MAXULONG64;

		ept_state.ept_page_table = allocate_pool<__vmm_ept_page_table>();
		if (ept_state.ept_page_table == NULL)
		{
			LogError("Failed to allocate memory for PageTable");
			return false;
		}

		__vmm_ept_page_table* page_table = ept_state.ept_page_table;
		RtlSecureZeroMemory(page_table, sizeof(__vmm_ept_page_table));

		//
		// Set all pages as rwx to prevent unwanted ept violation
		// 将所有页面设置为 rwx，以防止不必要的 ept 违规
		// 设置第一个 PML4E，使其指向我们的 PDPT
		//
		page_table->pml4[0].page_frame_number = GET_PFN(MmGetPhysicalAddress(&page_table->pml3[0]).QuadPart);
		page_table->pml4[0].read = 1;
		page_table->pml4[0].write = 1;
		page_table->pml4[0].execute = 1;
		page_table->pml4[0].execute_for_usermode = 1;

		auto const mtrrs = hv::read_mtrr_data();

		// TODO: allocate a PT for the fixed MTRRs region so that we can get
		// more accurate memory typing in that area (as opposed to just
		// mapping the whole PDE as UC).
		// 为固定 MTRR 区域分配一个 PT页表，以便我们可以在该区域获得更准确的内存类型（而不是仅将整个 PDE 映射为 UC）。

		for (size_t i = 0; i < EPT_PD_COUNT; ++i)
		{
			// point each PDPTE to the corresponding PD
			// 将每个 PDPTE 指向相应的 PD
			auto& pdpte = page_table->pml3[i];
			pdpte.read = 1;
			pdpte.write = 1;
			pdpte.execute = 1;
			pdpte.accessed = 0;
			pdpte.execute_for_usermode = 1;
			pdpte.page_frame_number = GET_PFN(MmGetPhysicalAddress(&page_table->pml2[i][0]).QuadPart);

			for (size_t j = 0; j < 512; ++j)
			{
				// identity-map every GPA to the corresponding HPA
				auto& pde = page_table->pml2[i][j];
				pde.page_directory_entry.read = 1;
				pde.page_directory_entry.write = 1;
				pde.page_directory_entry.execute = 1;
				pde.page_directory_entry.ignore_pat = 0;
				pde.page_directory_entry.large_page = 1;  //表示pde未分割
				pde.page_directory_entry.accessed = 0;
				pde.page_directory_entry.dirty = 0;
				pde.page_directory_entry.execute_for_usermode = 1;
				pde.page_directory_entry.page_frame_number = (i << 9) + j;
				pde.page_directory_entry.memory_type = hv::calc_mtrr_mem_type(mtrrs, pde.page_directory_entry.page_frame_number << 21/*2mb对齐*/, 0x1000 << 9/*2mb大小*/);
			}
		}

		return true;
	}

	/// <summary>
	/// Initialize ept structure
	/// </summary>
	/// <returns></returns>
	bool initialize(__ept_state& ept_state)
	{
		__eptp* ept_pointer = allocate_pool<__eptp*>(PAGE_SIZE);
		if (ept_pointer == NULL)
		{
			outDebug("ept_pointer 空指针\n");
			return false;
		}

		RtlSecureZeroMemory(ept_pointer, PAGE_SIZE);

		if (create_ept_page_table(ept_state) == false)
		{
			outDebug("create_ept_page_table失败\n");
			return false;
		}

		//ept_pointer->memory_type = g_vmm_context.mtrr_info.default_memory_type;
		//当前 VMX 架构只支持 UC和WB
		ept_pointer->memory_type = MEMORY_TYPE_WRITE_BACK;

		// Indicates 4 level paging
		// 使用4级分页
		ept_pointer->page_walk_length = 3;

		//注意因为我们是映射的pml4[0]
		//所以这里&ept_state.ept_page_table->pml4，可以取到pml4[0]的地址
		ept_pointer->pml4_address = GET_PFN(MmGetPhysicalAddress(&ept_state.ept_page_table->pml4).QuadPart);

		ept_state.ept_pointer = ept_pointer;

		return true;
	}
}

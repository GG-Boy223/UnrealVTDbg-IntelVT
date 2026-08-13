#include "EptBuildInternal.h"
#include <Ring0/Hvm/EptCoveragePolicy.h>

namespace ept
{
	static constexpr unsigned __int32 ept_default_pdpt_count = coverage_policy::minimum_pdpte_count;
	static_assert(EPT_PD_COUNT == ept_default_pdpt_count, "Embedded EPT table must match the minimum coverage policy");

	static bool add_overflow_u64(unsigned __int64 left, unsigned __int64 right, unsigned __int64& result)
	{
		result = left + right;
		return result < left;
	}

	static unsigned __int32 round_up_pdpt_count(unsigned __int64 highest_physical_end)
	{
		const auto calculation = coverage_policy::calculate(highest_physical_end);
		switch (calculation.calculation_status)
		{
		case coverage_policy::status::ok:
			outFile("[信息][EPT覆盖范围] 物理地址上界=0x%llX 所需PDPT数量=%u",
				highest_physical_end,
				calculation.pdpte_count);
			return calculation.pdpte_count;
		case coverage_policy::status::missing_physical_ranges:
			LogError("EPT identity map cannot be sized without physical memory ranges");
			outFile("[失败][EPT覆盖范围] 缺少有效物理内存上界");
			return 0;
		case coverage_policy::status::arithmetic_overflow:
			LogError("EPT identity map size overflow: highest_end=0x%llX", highest_physical_end);
			outFile("[失败][EPT覆盖范围] 物理地址上界计算溢出：HighestEnd=0x%llX", highest_physical_end);
			return 0;
		case coverage_policy::status::exceeds_four_level_limit:
			LogError("EPT identity map exceeds the 4-level EPT limit: highest_end=0x%llX", highest_physical_end);
			outFile("[失败][EPT覆盖范围] 物理地址上界超过4级EPT限制：HighestEnd=0x%llX MaxPDPT=%u",
				highest_physical_end,
				coverage_policy::maximum_pdpte_count);
			return 0;
		default:
			LogError("Unknown EPT coverage policy status");
			return 0;
		}
	}

	static bool cache_physical_memory_ranges(__ept_state& ept_state, unsigned __int64& highest_physical_end)
	{
		highest_physical_end = 0;
		PPHYSICAL_MEMORY_RANGE ranges = MmGetPhysicalMemoryRanges();
		if (ranges == nullptr)
		{
			LogError("MmGetPhysicalMemoryRanges failed; refusing unsafe MTRR-only EPT map");
			outFile("[失败][EPT覆盖范围] MmGetPhysicalMemoryRanges失败，拒绝把未知RAM/MMIO范围按MTRR-only映射");
			return false;
		}

		unsigned __int32 range_count = 0;
		for (; ranges[range_count].NumberOfBytes.QuadPart != 0; ++range_count)
		{
		}

		if (range_count == 0)
		{
			ExFreePool(ranges);
			outFile("[失败][EPT覆盖范围] 物理内存范围为空，无法安全区分RAM与MMIO");
			return false;
		}

		ept_state.physical_memory_ranges =
			allocate_pool<__ept_state::__ept_physical_memory_range*>(sizeof(__ept_state::__ept_physical_memory_range) * range_count);
		if (ept_state.physical_memory_ranges == nullptr)
		{
			ExFreePool(ranges);
			LogError("Failed to cache physical memory ranges");
			outFile("[失败][EPT覆盖范围] RAM范围缓存分配失败，拒绝退回不安全的MTRR-only策略");
			return false;
		}

		RtlSecureZeroMemory(ept_state.physical_memory_ranges, sizeof(__ept_state::__ept_physical_memory_range) * range_count);

		for (unsigned __int32 i = 0; i < range_count; ++i)
		{
			unsigned __int64 base = ranges[i].BaseAddress.QuadPart;
			unsigned __int64 size = ranges[i].NumberOfBytes.QuadPart;
			unsigned __int64 end = 0;
			if (add_overflow_u64(base, size, end))
			{
				end = MAXULONG64;
			}

			ept_state.physical_memory_ranges[i].base = base;
			ept_state.physical_memory_ranges[i].end = end;
			if (end > highest_physical_end)
			{
				highest_physical_end = end;
			}
		}

		ept_state.physical_memory_range_count = range_count;
		ExFreePool(ranges);
		outFile("[信息][EPT覆盖范围] RAM范围缓存完成：RangeCount=%u HighestRAMEnd=0x%llX",
			range_count,
			highest_physical_end);
		return true;
	}

	static unsigned __int32 query_required_pdpt_count(__ept_state& ept_state)
	{
		unsigned __int64 highest_physical_end = 0;
		// PPHYSICAL_MEMORY_RANGE ranges = MmGetPhysicalMemoryRanges(); // 旧逻辑：只临时读取最高RAM地址，后续EPT类型无法区分MMIO空洞。
		if (!cache_physical_memory_ranges(ept_state, highest_physical_end))
		{
			return 0;
		}

		return round_up_pdpt_count(highest_physical_end);
	}

	static __ept_pdpte* get_pdpte_entry(__ept_state& ept_state, unsigned __int32 flat_pdpt_index)
	{
		if (ept_state.ept_page_table == nullptr || flat_pdpt_index >= ept_state.mapped_pdpte_count)
		{
			return nullptr; // 失败返回空指针
		}

		if (flat_pdpt_index < 512)
		{
			return &ept_state.ept_page_table->pml3[flat_pdpt_index];
		}

		unsigned __int32 pml4_index = flat_pdpt_index >> 9;
		unsigned __int32 pml3_index = flat_pdpt_index & 0x1ff;
		if (ept_state.extended_pml3_pages == nullptr || pml4_index == 0 || pml4_index >= ept_state.mapped_pml4_count)
		{
			return nullptr; // 失败返回空指针
		}

		__ept_pdpte_page* pdpt_page = ept_state.extended_pml3_pages[pml4_index - 1];
		if (pdpt_page == nullptr)
		{
			return nullptr; // 失败返回空指针
		}

		return &pdpt_page->entries[pml3_index];
	}

	void cleanup_page_table(__ept_state& ept_state)
	{
		if (ept_state.physical_memory_ranges != nullptr)
		{
			free_pool(ept_state.physical_memory_ranges);
			ept_state.physical_memory_ranges = nullptr;
		}
		ept_state.physical_memory_range_count = 0;

		if (ept_state.pml2_pages != nullptr)
		{
			for (unsigned __int32 i = ept_default_pdpt_count; i < ept_state.mapped_pdpte_count; ++i)
			{
				if (ept_state.pml2_pages[i] != nullptr)
				{
					free_pool(ept_state.pml2_pages[i]);
					ept_state.pml2_pages[i] = nullptr;
				}
			}

			free_pool(ept_state.pml2_pages);
			ept_state.pml2_pages = nullptr;
		}

		if (ept_state.extended_pml3_pages != nullptr)
		{
			unsigned __int32 ext_count = ept_state.mapped_pml4_count > 1 ? ept_state.mapped_pml4_count - 1 : 0;
			for (unsigned __int32 i = 0; i < ext_count; ++i)
			{
				if (ept_state.extended_pml3_pages[i] != nullptr)
				{
					free_pool(ept_state.extended_pml3_pages[i]);
					ept_state.extended_pml3_pages[i] = nullptr;
				}
			}

			free_pool(ept_state.extended_pml3_pages);
			ept_state.extended_pml3_pages = nullptr;
		}

		ept_state.mapped_pml4_count = 0;
		ept_state.mapped_pdpte_count = 0;
	}

	static bool physical_range_overlaps_ram(__ept_state const& ept_state, unsigned __int64 physical_address, unsigned __int64 size)
	{
		if (size == 0 || ept_state.physical_memory_ranges == nullptr || ept_state.physical_memory_range_count == 0)
		{
			return false;
		}

		unsigned __int64 end = 0;
		if (add_overflow_u64(physical_address, size, end))
		{
			end = MAXULONG64;
		}

		for (unsigned __int32 i = 0; i < ept_state.physical_memory_range_count; ++i)
		{
			auto const& range = ept_state.physical_memory_ranges[i];
			if (physical_address < range.end && end > range.base)
			{
				return true;
			}
		}

		return false;
	}

	bool is_physical_range_ram(__ept_state const& ept_state, unsigned __int64 physical_address, unsigned __int64 size)
	{
		if (size == 0)
		{
			return false;
		}

		if (ept_state.physical_memory_ranges == nullptr || ept_state.physical_memory_range_count == 0)
		{
			return false; // 未知范围按非RAM处理；初始化阶段正常情况下会在此前直接失败。
		}

		unsigned __int64 end = 0;
		if (add_overflow_u64(physical_address, size, end))
		{
			return false;
		}

		unsigned __int64 cursor = physical_address;
		while (cursor < end)
		{
			bool advanced = false;
			for (unsigned __int32 i = 0; i < ept_state.physical_memory_range_count; ++i)
			{
				auto const& range = ept_state.physical_memory_ranges[i];
				if (cursor >= range.base && cursor < range.end)
				{
					cursor = range.end < end ? range.end : end;
					advanced = true;
					break;
				}
			}

			if (!advanced)
			{
				return false;
			}
		}

		return true;
	}

	unsigned __int8 get_ept_memory_type_for_range(mtrr_data const& mtrrs, __ept_state const& ept_state, unsigned __int64 physical_address, unsigned __int64 size)
	{
		if (!is_physical_range_ram(ept_state, physical_address, size))
		{
			return MEMORY_TYPE_UNCACHEABLE; // 非RAM/MMIO必须保守映射为UC，避免设备空间被WB缓存造成整机卡死。
		}

		return hv::calc_mtrr_mem_type(mtrrs, physical_address, size);
	}

	/// <summary>
	/// Setup page memory type
	/// 设置页内存类型
	/// </summary>
	/// <param name="entry"> Pointer to pml2 entry </param>
	/// <param name="pfn"> Page frame number </param>
	bool setup_pml2_entry(mtrr_data const& mtrrs, __ept_state& ept_state, __ept_pde& entry, unsigned __int64 pfn)
	{
		entry.page_directory_entry.page_frame_number = pfn;
		unsigned __int64 physical_address = pfn * LARGE_PAGE_SIZE;
		bool const fully_ram = is_physical_range_ram(ept_state, physical_address, LARGE_PAGE_SIZE);
		bool const overlaps_ram = physical_range_overlaps_ram(ept_state, physical_address, LARGE_PAGE_SIZE);

		//判断是否是大页面
		if (is_valid_for_large_page(pfn) == true && (fully_ram || !overlaps_ram))
		{
			// entry.page_directory_entry.memory_type = get_memory_type(pfn, true); // 旧逻辑：只看MTRR，RAM上界内的MMIO空洞可能被默认WB覆盖。
			entry.page_directory_entry.memory_type = get_ept_memory_type_for_range(mtrrs, ept_state, physical_address, LARGE_PAGE_SIZE);
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
			return split_pml2(mtrrs, ept_state, split_buffer, physical_address);
		}
	}

	/// <summary>
	/// Create ept page table
	/// 创建ept页表
	/// </summary>
	/// <returns> status </returns>
	bool create_ept_page_table(__ept_state& ept_state)
	{
		outFile("---------- EPT页表构建开始 ----------");
		ept_state.ept_page_table = allocate_pool<__vmm_ept_page_table>();
		if (ept_state.ept_page_table == NULL)
		{
			outDebug("PageTable 内存分配失败。\n");
			outFile("[失败][EPT页表] EPT页表主结构分配失败");
			return false;
		}
		outFile("[成功][EPT页表] EPT页表主结构分配完成：VA=0x%p Size=%llu",
			ept_state.ept_page_table,
			static_cast<unsigned __int64>(sizeof(__vmm_ept_page_table)));

		__vmm_ept_page_table* page_table = ept_state.ept_page_table;
		RtlSecureZeroMemory(page_table, sizeof(__vmm_ept_page_table));

		unsigned __int32 required_pdpt_count = query_required_pdpt_count(ept_state);
		if (required_pdpt_count == 0)
		{
			outFile("[失败][EPT页表] 所需PDPT数量为0，停止EPT构建");
			cleanup_page_table(ept_state); // 清理已缓存的RAM范围，避免极端物理地址上界失败时泄漏。
			free_pool(ept_state.ept_page_table);
			ept_state.ept_page_table = nullptr;
			return false;
		}

		unsigned __int32 required_pml4_count = (required_pdpt_count + 511) >> 9;
		ept_state.mapped_pdpte_count = required_pdpt_count;
		ept_state.mapped_pml4_count = required_pml4_count;
		outFile("[信息][EPT页表] 映射规模：PDPT=%u PML4=%u CoverageGB=%u",
			required_pdpt_count,
			required_pml4_count,
			required_pdpt_count);

		ept_state.pml2_pages = allocate_pool<__ept_pde**>(sizeof(__ept_pde*) * required_pdpt_count);
		if (ept_state.pml2_pages == nullptr)
		{
			LogError("Failed to allocate EPT PD page pointer table");
			outFile("[失败][EPT页表] PML2页指针表分配失败：Bytes=%llu",
				static_cast<unsigned __int64>(sizeof(__ept_pde*) * required_pdpt_count));
			cleanup_page_table(ept_state);
			free_pool(ept_state.ept_page_table);
			ept_state.ept_page_table = nullptr;
			return false;
		}
		RtlSecureZeroMemory(ept_state.pml2_pages, sizeof(__ept_pde*) * required_pdpt_count);

		if (required_pml4_count > 1)
		{
			ept_state.extended_pml3_pages = allocate_pool<__ept_pdpte_page**>(sizeof(__ept_pdpte_page*) * (required_pml4_count - 1));
			if (ept_state.extended_pml3_pages == nullptr)
			{
				LogError("Failed to allocate EPT extended PDPT pointer table");
				outFile("[失败][EPT页表] 扩展PDPT指针表分配失败：ExtCount=%u", required_pml4_count - 1);
				cleanup_page_table(ept_state);
				free_pool(ept_state.ept_page_table);
				ept_state.ept_page_table = nullptr;
				return false;
			}
			RtlSecureZeroMemory(ept_state.extended_pml3_pages, sizeof(__ept_pdpte_page*) * (required_pml4_count - 1));
			outFile("[成功][EPT页表] 扩展PDPT指针表分配完成：ExtCount=%u", required_pml4_count - 1);
		}

		//
		// Set all pages as rwx to prevent unwanted ept violation
		// 将所有页面设置为 rwx，以防止不必要的 ept 违规
		// 设置第一个 PML4E，使其指向我们的 PDPT
		//
		for (unsigned __int32 pml4_index = 0; pml4_index < required_pml4_count; ++pml4_index)
		{
			__ept_pdpte* pdpt_entries = nullptr;
			if (pml4_index == 0)
			{
				pdpt_entries = &page_table->pml3[0];
			}
			else
			{
				__ept_pdpte_page* pdpt_page = allocate_pool<__ept_pdpte_page*>(sizeof(__ept_pdpte_page));
				if (pdpt_page == nullptr)
				{
					LogError("Failed to allocate EPT extended PDPT page %u", pml4_index);
					outFile("[失败][EPT页表] 扩展PDPT页分配失败：PML4Index=%u", pml4_index);
					cleanup_page_table(ept_state);
					free_pool(ept_state.ept_page_table);
					ept_state.ept_page_table = nullptr;
					return false;
				}

				RtlSecureZeroMemory(pdpt_page, sizeof(__ept_pdpte_page));
				ept_state.extended_pml3_pages[pml4_index - 1] = pdpt_page;
				pdpt_entries = &pdpt_page->entries[0];
			}

			page_table->pml4[pml4_index].page_frame_number = GET_PFN(MmGetPhysicalAddress(pdpt_entries).QuadPart);
			page_table->pml4[pml4_index].read = 1;
			page_table->pml4[pml4_index].write = 1;
			page_table->pml4[pml4_index].execute = 1;
			page_table->pml4[pml4_index].execute_for_usermode = 0;
		}

		__ept_pdpte pdpte_template = { 0 };

		pdpte_template.read = 1;
		pdpte_template.write = 1;
		pdpte_template.execute = 1;
		pdpte_template.execute_for_usermode = 0;

		for (unsigned __int32 i = 0; i < required_pdpt_count; ++i)
		{
			__ept_pde* pde_page = nullptr;
			if (i < ept_default_pdpt_count)
			{
				pde_page = &page_table->pml2[i][0];
			}
			else
			{
				__ept_pde_page* dynamic_pde_page = allocate_pool<__ept_pde_page*>(sizeof(__ept_pde_page));
				if (dynamic_pde_page == nullptr)
				{
					LogError("Failed to allocate EPT dynamic PD page %u", i);
					outFile("[失败][EPT页表] 动态PD页分配失败：FlatPDPT=%u", i);
					cleanup_page_table(ept_state);
					free_pool(ept_state.ept_page_table);
					ept_state.ept_page_table = nullptr;
					return false;
				}

				RtlSecureZeroMemory(dynamic_pde_page, sizeof(__ept_pde_page));
				pde_page = &dynamic_pde_page->entries[0];
			}

			ept_state.pml2_pages[i] = pde_page;

			__ept_pdpte* pdpte = get_pdpte_entry(ept_state, i);
			if (pdpte == nullptr)
			{
				LogError("EPT PDPTE lookup failed for index %u", i);
				outFile("[失败][EPT页表] PDPTE查找失败：FlatPDPT=%u", i);
				cleanup_page_table(ept_state);
				free_pool(ept_state.ept_page_table);
				ept_state.ept_page_table = nullptr;
				return false;
			}

			pdpte->all = pdpte_template.all;
			pdpte->page_frame_number = GET_PFN(MmGetPhysicalAddress(pde_page).QuadPart);
		}

		__ept_pde pde_template = { 0 };

		pde_template.page_directory_entry.read = 1;
		pde_template.page_directory_entry.write = 1;
		pde_template.page_directory_entry.execute = 1;

		pde_template.page_directory_entry.large_page = 1;  //使用大页面 2mb
		pde_template.page_directory_entry.execute_for_usermode = 0;

		for (unsigned __int32 i = 0; i < required_pdpt_count; ++i)
		{
			__stosq((unsigned __int64*)ept_state.pml2_pages[i], pde_template.all, 512);
		}

		// MTRR data for setting memory types
		// 用于设置内存类型的 MTRR 数据
		auto const mtrrs = hv::read_mtrr_data();
		g_vmm_context.mtrr_info.mtrrs = mtrrs;
		for (unsigned __int32 i = 0; i < required_pdpt_count; i++)
		{
			for (int j = 0; j < 512; j++)
			{
				unsigned __int64 pde_pfn = (static_cast<unsigned __int64>(i) * 512) + j;
				if (setup_pml2_entry(mtrrs, ept_state, ept_state.pml2_pages[i][j], pde_pfn) == false)
				{
					outDebug("setup_pml2_entry 失败\n");
					outFile("[失败][EPT页表] PML2条目设置失败：FlatPDPT=%u PDE=%d PFN=%llu", i, j, pde_pfn);
					cleanup_page_table(ept_state);
					free_pool(ept_state.ept_page_table);
					ept_state.ept_page_table = nullptr;
					return false;
				}
			}
		}

		outFile("---------- EPT页表构建完成：MappedPDPT=%u MappedPML4=%u ----------", ept_state.mapped_pdpte_count, ept_state.mapped_pml4_count);
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
		page_table->pml4[0].execute_for_usermode = 0;

		// MTRR data for setting memory types
		// 用于设置内存类型的 MTRR 数据
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
			pdpte.accessed = 0; // 清零访问位
			pdpte.execute_for_usermode = 0;
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
				pde.page_directory_entry.execute_for_usermode = 0;
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
		outFile("---------- EPT初始化开始 ----------");
		__eptp* ept_pointer = allocate_pool<__eptp*>(PAGE_SIZE);
		if (ept_pointer == NULL)
		{
			outDebug("ept_pointer 空指针\n");
			outFile("[失败][EPT初始化] EPTP结构分配失败");
			return false;
		}
		outFile("[成功][EPT初始化] EPTP结构分配完成：VA=0x%p", ept_pointer);

		RtlSecureZeroMemory(ept_pointer, PAGE_SIZE);

		if (create_ept_page_table(ept_state) == false)
		{
			outDebug("create_ept_page_table失败\n");
			free_pool(ept_pointer);
			ept_state.ept_pointer = nullptr;
			outFile("[失败][EPT初始化] EPT页表构建失败");
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
		outFile("---------- EPT初始化完成：EPTP=0x%llX PML4_PA=0x%llX MappedPDPT=%u MappedPML4=%u ----------",
			ept_pointer->all,
			ept_pointer->pml4_address << PAGE_SHIFT,
			ept_state.mapped_pdpte_count,
			ept_state.mapped_pml4_count);

		return true;
	}
}

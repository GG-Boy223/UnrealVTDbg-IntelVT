#include "EptBuildInternal.h"
#include <Ring0/Hvm/EptCoveragePolicy.h>

namespace ept
{
	// HyperDbg 式布局：内嵌 pml3[512] 承载前 512 个 1GB 区域（0 ~ 512GB），
	// 每个区域一张 PD 页（EPT_PD_COUNT 个 2MB 条目），更高区域由扩展 PDPT 页承载。
	static constexpr unsigned __int32 ept_low_identity_region_count = EPT_LOW_IDENTITY_PD_PAGE_COUNT;
	static_assert(EPT_LOW_IDENTITY_PD_PAGE_COUNT == 512, "First 512GB must be covered by 512 one-GB regions");
	static_assert(EPT_PD_COUNT == EPT_LOW_IDENTITY_PD_PAGE_COUNT, "Each PD page must cover exactly one 1GB region");

	// 1GB EPT 大页（PDPTE.PS=1）不是所有 CPU 都支持，必须查 IA32_VMX_EPT_VPID_CAP[17]。
	// 在不支持该位的 CPU 上把 PDPTE 的 PS 置 1，硬件会判为 EPT misconfiguration
	// （VM-exit reason 49，该退出下 qualification 无意义、恒为 0），
	// 表现为加载后立即蓝屏 HYPERVISOR_ERROR(0x20001) Arg1=8。
	// 支持则用 1GB 页一次映射完无 RAM 的高地址 MMIO 区域；不支持则退回逐 PDPTE 分配 PD、用 2MB 页覆盖。
	static bool query_1gb_ept_page_support()
	{
		__vmx_ept_vpid_cap capabilities = {};
		capabilities.all = __readmsr(IA32_VMX_EPT_VPID_CAP);
		outFile("[信息][CPU%u][EPT能力] IA32_VMX_EPT_VPID_CAP=0x%llX 2MB大页=%s 1GB大页=%s",
			KeGetCurrentProcessorNumber(),
			capabilities.all,
			capabilities.ept_support_2mb_page != 0 ? "支持" : "不支持",
			capabilities.ept_support_1gb_page != 0 ? "支持" : "不支持");
		return capabilities.ept_support_1gb_page != 0;
	}

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
			// HyperDbg 式：恒定覆盖整个 4 级 EPT 空间（512 * 512 个 1GB 区域 = 2^48 = 256TB）。
			outFile("[信息][CPU%u][EPT覆盖范围] RAM上界=0x%llX PDPT数量=%u 覆盖=256TB(0x%llX)",
				KeGetCurrentProcessorNumber(),
				highest_physical_end,
				calculation.pdpte_count,
				coverage_policy::identity_map_bytes);
			return calculation.pdpte_count;
		case coverage_policy::status::missing_physical_ranges:
			LogError("EPT identity map cannot be sized without physical memory ranges");
			outFile("[失败][EPT覆盖范围] 缺少有效物理内存上界");
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

		// 覆盖范围对齐 HyperDbg EptAllocateAndCreateIdentityPageTable：不再按 MAXPHYADDR 裁剪，
		// 恒定建满整个 4 级 EPT 空间（2^48 = 256TB）。Core Ultra 等平台的显卡 BAR / PCIe MMIO /
		// DMA 缓冲常落在远高于 RAM 上界的 GPA 窗口（HvVmxCore 的 Core Ultra 复盘里是 ≈512GB 以上），
		// 构建期建满后来宾访问这些窗口永远命中现成映射，不会落到 not-present 条目上
		// 引发 EPT violation 反复重试、显卡提交超时 TDR(0x116)。
		// 这里仍取“全平台最大物理地址上界”（各逻辑处理器 CPUID.80000008H 的最大值，
		// 由 query_platform_physical_address_limit() 预先扫描得到）用于日志与范围有效性校验。
		if (highest_physical_end < g_vmm_context.platform_max_physical_end)
		{
			highest_physical_end = g_vmm_context.platform_max_physical_end;
		}

		outFile("[信息][CPU%u][EPT覆盖范围] 全平台MAXPHYADDR物理上界=0x%llX（仅用于诊断；EPT 恒定覆盖 256TB）",
			KeGetCurrentProcessorNumber(),
			g_vmm_context.platform_max_physical_end);
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
		if (pml4_index == 0 || pml4_index >= ept_state.mapped_pml4_count)
		{
			return nullptr; // 失败返回空指针
		}

		// 512GB 以上：内嵌 pml3_rsvd[511][512]（等价 HyperDbg PML3_RSVD[i - 1]）
		return &ept_state.ept_page_table->pml3_rsvd[pml4_index - 1][pml3_index];
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
			// pml2_pages 只是指向内嵌 pml2[512][512] 的行指针表，PD 页本身随 ept_page_table 一起释放。
			free_pool(ept_state.pml2_pages);
			ept_state.pml2_pages = nullptr;
		}
		ept_state.pml2_pages_count = 0;

		ept_state.ept_1gb_pages_supported = false;

		ept_state.mapped_pml4_count = 0;
		ept_state.mapped_pdpte_count = 0;
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
		UNREFERENCED_PARAMETER(mtrrs); // 改用全局 MTRR map（g_vmm_context.mtrr_info）做逐页类型查询。
		UNREFERENCED_PARAMETER(ept_state);

		// 对齐 HyperDbg EptGetMemoryType：按 MTRR 范围逐 4KB 查询并合并内存类型。
		// UC 最高优先级；WT 与 WB 重叠取 WT；无覆盖取 MTRR 默认类型。
		physical_address &= ~0xFFFull; // 对齐到 4KB
		size = (size + 0xFFF) & ~0xFFFull; // 对齐大小到 4KB

		uint8_t curr_mem_type = MEMORY_TYPE_INVALID;
		for (uint64_t curr = physical_address; curr < physical_address + size; curr += 0x1000)
		{
			uint8_t const type = hv::calc_mtrr_mem_type_for_address(curr);
			if (type == MEMORY_TYPE_UNCACHEABLE)
			{
				return MEMORY_TYPE_UNCACHEABLE; // UC 优先级最高，直接返回
			}

			if (curr_mem_type == MEMORY_TYPE_INVALID)
			{
				curr_mem_type = type;
			}
			else if (curr_mem_type == MEMORY_TYPE_WRITE_BACK && type == MEMORY_TYPE_WRITE_THROUGH)
			{
				curr_mem_type = MEMORY_TYPE_WRITE_THROUGH; // WT 与 WB 重叠取 WT
			}
			else if (curr_mem_type != type &&
				!(curr_mem_type == MEMORY_TYPE_WRITE_THROUGH && type == MEMORY_TYPE_WRITE_BACK))
			{
				curr_mem_type = type; // 其余情况取后值
			}
		}

		if (curr_mem_type == MEMORY_TYPE_INVALID)
		{
			curr_mem_type = hv::calc_mtrr_mem_type_for_address(physical_address);
		}
		return curr_mem_type;
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

		// MTRR 逐页（对齐 HyperDbg EptSetupPML2Entry）：2MB 页不跨 MTRR 范围边界就用 2MB 大页，
		// 内存类型按该页物理地址做 MTRR 点查询；跨 MTRR 边界才拆成 4KB 逐页设类型。
		if (hv::is_valid_for_large_page(physical_address))
		{
			// 对齐 HyperDbg EptSetupPML2Entry -> EptGetMemoryType(PFN, TRUE)：
			// 已经确认这个 2MB 页不跨 MTRR 范围边界，整页内存类型一致，查一次即可
			// （逐 4KB 组合查询在这里是纯浪费：512GB identity 有 262144 个 2MB 页）。
			entry.page_directory_entry.memory_type =
				hv::calc_mtrr_mem_type_for_address(physical_address);
			return true;
		}
		else
		{
			// 跨 MTRR 边界：分配 PT 页表，把 2MB 页拆成 512 个 4KB 页。
			void* split_buffer = pool_manager::request_pool<void*>(pool_manager::INTENTION_SPLIT_PML2, true, sizeof(__ept_dynamic_split));
			if (split_buffer == nullptr)
			{
				LogError("Failed to allocate split buffer");
				return false;
			}

			// 将2MB页面分割为512个4KB页面
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
		ept_state.ept_page_table = allocate_page_aligned_pool<__vmm_ept_page_table*>(sizeof(__vmm_ept_page_table));
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

		bool const use_1gb_ept_pages = query_1gb_ept_page_support();
		ept_state.ept_1gb_pages_supported = use_1gb_ept_pages; // 512GB 以上按它决定是否建 1GB identity 条目

		unsigned __int32 required_pdpt_count = query_required_pdpt_count(ept_state);
		if (required_pdpt_count == 0)
		{
			outFile("[失败][EPT页表] 所需PDPT数量为0，停止EPT构建");
			cleanup_page_table(ept_state); // 清理已缓存的RAM范围，避免极端物理地址上界失败时泄漏。
			free_vmm_page_aligned(ept_state.ept_page_table);
			ept_state.ept_page_table = nullptr;
			return false;
		}

		unsigned __int32 required_pml4_count = (required_pdpt_count + 511) >> 9;
		ept_state.mapped_pdpte_count = required_pdpt_count;
		ept_state.mapped_pml4_count = required_pml4_count;
		outFile("[信息][CPU%u][EPT页表] 映射规模：PDPT=%u PML4=%u CoverageGB=%u（HyperDbg 式全 4 级空间 identity）",
			KeGetCurrentProcessorNumber(),
			required_pdpt_count,
			required_pml4_count,
			required_pdpt_count);

		// 前 512GB 的 PD 行全部来自内嵌 pml2[512][512]，这里只建立行指针索引（无逐页分配）。
		ept_state.pml2_pages_count = ept_low_identity_region_count;

		ept_state.pml2_pages = allocate_pool<__ept_pde**>(sizeof(__ept_pde*) * ept_state.pml2_pages_count);
		if (ept_state.pml2_pages == nullptr)
		{
			LogError("Failed to allocate EPT PD row pointer table");
			outFile("[失败][EPT页表] PML2行指针表分配失败：Bytes=%llu",
				static_cast<unsigned __int64>(sizeof(__ept_pde*) * ept_state.pml2_pages_count));
			cleanup_page_table(ept_state);
			free_vmm_page_aligned(ept_state.ept_page_table);
			ept_state.ept_page_table = nullptr;
			return false;
		}
		RtlSecureZeroMemory(ept_state.pml2_pages, sizeof(__ept_pde*) * ept_state.pml2_pages_count);

		//
		// Set all pages as rwx to prevent unwanted ept violation
		// 将所有页面设置为 rwx，以防止不必要的 ept 违规
		// 对齐 HyperDbg EptAllocateAndCreateIdentityPageTable：
		//   PML4[0]      -> pml3（前 512GB 的 512 张 PD）
		//   PML4[1..511] -> pml3_rsvd[pml4_index - 1]（512GB ~ 256TB 的 1GB identity）
		//
		for (unsigned __int32 pml4_index = 0; pml4_index < required_pml4_count; ++pml4_index)
		{
			__ept_pdpte* pdpt_entries =
				pml4_index == 0
				? &page_table->pml3[0]
				: &page_table->pml3_rsvd[pml4_index - 1][0];

			page_table->pml4[pml4_index].page_frame_number = GET_PFN(MmGetPhysicalAddress(pdpt_entries).QuadPart);
			page_table->pml4[pml4_index].read = 1;
			page_table->pml4[pml4_index].write = 1;
			page_table->pml4[pml4_index].execute = 1;
			page_table->pml4[pml4_index].execute_for_usermode = 0;
		}

		__ept_pde pde_template = { 0 };

		pde_template.page_directory_entry.read = 1;
		pde_template.page_directory_entry.write = 1;
		pde_template.page_directory_entry.execute = 1;
		pde_template.page_directory_entry.large_page = 1;  //使用大页面 2mb
		pde_template.page_directory_entry.execute_for_usermode = 0;

		__ept_pdpte pdpte_template = { 0 };

		pdpte_template.read = 1;
		pdpte_template.write = 1;
		pdpte_template.execute = 1;
		pdpte_template.execute_for_usermode = 0;

		// 0 ~ 512GB：pml3[i] -> &pml2[i][0]，并把行指针登记进 pml2_pages（hook/watch/MTRR 遍历用）。
		for (unsigned __int32 i = 0; i < ept_low_identity_region_count; ++i)
		{
			__ept_pde* pde_page = &page_table->pml2[i][0];
			ept_state.pml2_pages[i] = pde_page;

			// 与建立入口同一个循环完成整行模板填充：pml3[i] 一旦发布即为完整条目
			__stosq((unsigned __int64*)pde_page, pde_template.all, 512);

			__ept_pdpte* pdpte = &page_table->pml3[i];
			pdpte->all = pdpte_template.all;
			pdpte->page_frame_number = GET_PFN(MmGetPhysicalAddress(pde_page).QuadPart);
		}

		// 512GB ~ 256TB：pml3_rsvd[i][j] 逐个填 1GB UC identity 条目。
		// 地址字段在 bit 30 起，PFN = 物理地址 >> 30 = flat PDPT 序号 (i + 1) * 512 + j。
		// Core Ultra 等平台的显卡 BAR / PCIe MMIO / DMA 缓冲就落在这一段，
		// 构建期建满后来宾访问高位地址永远命中现成映射，不会落到 not-present 条目上引发
		// EPT violation 反复重试（HvVmxCore 的 Core Ultra 复盘：TDR -> 0x116）。
		// CPU 不支持 1GB EPT 大页（IA32_VMX_EPT_VPID_CAP[17]=0）时按 HvVmxCore 的做法
		// 保留 not-present：该能力缺失只出现在老平台，硬件本身也访问不到这些高位窗口。
		unsigned __int32 identity_1gb_region_count = 0;
		unsigned __int32 not_present_region_count = 0;
		for (unsigned __int32 pml4_index = 1; pml4_index < required_pml4_count; ++pml4_index)
		{
			for (unsigned __int32 pml3_index = 0; pml3_index < 512; ++pml3_index)
			{
				__ept_pdpte* pdpte = &page_table->pml3_rsvd[pml4_index - 1][pml3_index];
				if (!use_1gb_ept_pages)
				{
					pdpte->all = 0; // 不支持 1GB 大页：留 not-present，与 HvVmxCore 一致
					++not_present_region_count;
					continue;
				}

				__ept_pdpte entry = { 0 };
				entry.large_page.read = 1;
				entry.large_page.write = 1;
				entry.large_page.execute = 1;
				entry.large_page.memory_type = MEMORY_TYPE_UNCACHEABLE; // MMIO 必须 UC，否则整机卡死
				entry.large_page.ignore_pat = 0;
				entry.large_page.large_page = 1; // PS=1：本条目直接映射 1GB 大页
				entry.large_page.execute_for_usermode = 0;
				entry.large_page.page_frame_number =
					(static_cast<unsigned __int64>(pml4_index) * 512) + pml3_index;
				pdpte->all = entry.all;
				++identity_1gb_region_count;
			}
		}

		// MTRR data for setting memory types
		// 用于设置内存类型的 MTRR 数据
		auto const mtrrs = hv::read_mtrr_data();
		unsigned __int32 pd_page_count = 0;
		for (unsigned __int32 i = 0; i < ept_low_identity_region_count; i++)
		{
			++pd_page_count;
			for (int j = 0; j < 512; j++)
			{
				unsigned __int64 pde_pfn = (static_cast<unsigned __int64>(i) * 512) + j;
				if (setup_pml2_entry(mtrrs, ept_state, ept_state.pml2_pages[i][j], pde_pfn) == false)
				{
					outDebug("setup_pml2_entry 失败\n");
					outFile("[失败][EPT页表] PML2条目设置失败：FlatPDPT=%u PDE=%d PFN=%llu", i, j, pde_pfn);
					cleanup_page_table(ept_state);
					free_vmm_page_aligned(ept_state.ept_page_table);
					ept_state.ept_page_table = nullptr;
					return false;
				}
			}
		}

		outFile("[信息][CPU%u][EPT页表] 前512GB=%u 张PD页（每页512个2MB条目，类型按MTRR逐页）",
			KeGetCurrentProcessorNumber(),
			pd_page_count);
		if (use_1gb_ept_pages)
		{
			outFile("[信息][EPT页表] 512GB~256TB=%u 个1GB EPT大页 UC identity 条目（等价 HyperDbg PML3_RSVD）",
				identity_1gb_region_count);
		}
		else
		{
			outFile("[警告][EPT页表] CPU不支持1GB EPT大页：512GB~256TB 的 %u 个1GB区域保持 not-present（HvVmxCore 式跳过高位映射）",
				not_present_region_count);
		}

		outFile("---------- EPT页表构建完成：MappedPDPT=%u MappedPML4=%u ----------", ept_state.mapped_pdpte_count, ept_state.mapped_pml4_count);
		return true;
	}

	bool create_ept_page_table2(__ept_state& ept_state)
	{
		return create_ept_page_table(ept_state);
	}

	/// <summary>
	/// Initialize ept structure
	/// </summary>
	/// <returns></returns>
	bool initialize(__ept_state& ept_state)
	{
		outFile("---------- EPT初始化开始 ----------");
		__eptp* ept_pointer = allocate_page_aligned_pool<__eptp*>(PAGE_SIZE);
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
			free_vmm_page_aligned(ept_pointer);
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

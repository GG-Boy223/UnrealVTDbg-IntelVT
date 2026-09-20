#include "EptBuildInternal.h"
#include <Ring0/Hvm/EptCoveragePolicy.h>

namespace ept
{
	static constexpr unsigned __int32 ept_default_pdpt_count = coverage_policy::minimum_pdpte_count;
	static_assert(EPT_PD_COUNT == ept_default_pdpt_count, "Embedded EPT table must match the minimum coverage policy");

	static constexpr unsigned __int64 ept_1gb_page_size = 0x40000000ull; // 1GB EPT 大页

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
			outFile("[信息][CPU%u][EPT覆盖范围] 物理地址上界=0x%llX 所需PDPT数量=%u",
				KeGetCurrentProcessorNumber(),
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
		case coverage_policy::status::clamped_to_four_level_limit:
			// 4 级 EPT 只能翻译 2^48（256TB）以内的 GPA。部分 CPU（带 MKTME/TDX 的 Xeon 等）
			// 报告 MAXPHYADDR=52，按 1<<MAXPHYADDR 推导覆盖范围必然超过该上限；超出部分硬件
			// 本来也无法翻译，因此裁剪到上限继续加载，而不是让驱动整体拒绝加载。
			LogInfo("EPT identity map clamped to the 4-level EPT limit: highest_end=0x%llX limit=0x%llX",
				highest_physical_end,
				coverage_policy::four_level_limit_bytes);
			outFile("[警告][EPT覆盖范围] 物理地址上界=0x%llX 超过4级EPT可翻译上限=0x%llX(256TB)，已裁剪到上限，超出部分无法翻译",
				highest_physical_end,
				coverage_policy::four_level_limit_bytes);
			return calculation.pdpte_count;
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

		// 来宾可以合法访问 MAXPHYADDR 以内的任意 GPA —— 尤其是落在高地址窗口的 MMIO
		// （显卡/PCIe BAR 可以远高于 4GB；实测 Ultra 9 275HX 上 nvlddmkm 会访问 ≈4TB 的 GPA）。
		// 只按 RAM 上界取整的话 EPT 仅覆盖 512GB：来宾一旦碰到 512GB 以上的 MMIO，
		// 就会命中 not-present 的 EPT 条目 → EPT violation → 反复重试 → 被判为
		// vmx_fatal_unhandled_ept_violation 直接蓝屏。所以覆盖范围必须拉到 MAXPHYADDR。
		// 注意：4 级 EPT 的分页结构最多表达 2^48（256TB）的 GPA，MAXPHYADDR 更大时由
		// coverage_policy 裁剪到该硬上限（多出的 GPA 硬件本来就无法翻译）。
		// 覆盖范围用“全平台最大物理地址上界”（各逻辑处理器 CPUID.80000008H 的最大值，
		// 由 query_platform_physical_address_limit() 预先扫描得到），而不是当前核报的值：
		// MMIO/BAR 由操作系统按全平台解码能力分配，按单核较小值建表会让该核访问高地址 BAR 时缺映射。
		if (highest_physical_end < g_vmm_context.platform_max_physical_end)
		{
			highest_physical_end = g_vmm_context.platform_max_physical_end;
		}

		outFile("[信息][CPU%u][EPT覆盖范围] 全平台MAXPHYADDR物理上界=0x%llX，4级EPT可翻译上限=256TB(0x%llX)",
			KeGetCurrentProcessorNumber(),
			g_vmm_context.platform_max_physical_end,
			coverage_policy::four_level_limit_bytes);
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

	static bool pml2_page_is_owned_by_mmio_pool(__ept_state const& ept_state, __ept_pde* page)
	{
		if (page == nullptr || ept_state.mmio_pd_pool == nullptr)
		{
			return false;
		}

		for (unsigned __int32 i = 0; i < ept_state.mmio_pd_pool_capacity; ++i)
		{
			if (ept_state.mmio_pd_pool[i].entries == page)
			{
				return true;
			}
		}

		return false;
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
			for (unsigned __int32 i = 0; i < ept_state.mapped_pdpte_count; ++i)
			{
				if (ept_state.pml2_pages[i] != nullptr)
				{
					// MMIO 池页由池描述表统一释放；普通 RAM PD 页才在这里释放，避免双重释放。
					if (!pml2_page_is_owned_by_mmio_pool(ept_state, ept_state.pml2_pages[i]))
					{
						free_vmm_page_aligned(ept_state.pml2_pages[i]);
					}
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
					free_vmm_page_aligned(ept_state.extended_pml3_pages[i]);
					ept_state.extended_pml3_pages[i] = nullptr;
				}
			}

			free_pool(ept_state.extended_pml3_pages);
			ept_state.extended_pml3_pages = nullptr;
		}

		if (ept_state.mmio_pd_pool != nullptr)
		{
			// capacity 记录的是已成功分配的页数：初始化中途失败时也只释放真实存在的页。
			for (unsigned __int32 i = 0; i < ept_state.mmio_pd_pool_capacity; ++i)
			{
				if (ept_state.mmio_pd_pool[i].entries != nullptr)
				{
					free_vmm_page_aligned(ept_state.mmio_pd_pool[i].entries);
					ept_state.mmio_pd_pool[i].entries = nullptr;
				}
			}

			free_pool(ept_state.mmio_pd_pool);
			ept_state.mmio_pd_pool = nullptr;
		}
		ept_state.mmio_pd_pool_capacity = 0;
		ept_state.mmio_pd_pool_used = 0;

		if (ept_state.mmio_mapped_region_count != 0 || ept_state.mmio_mapping_failure_count != 0)
		{
			// VM-exit 里只累加计数，真实日志在这里（安全上下文）补输出。
			outFile("[信息][EPT按需映射] 运行期统计：已补映射区域=%llu 失败次数=%llu 最近GPA=0x%llX 1GB大页=%s",
				ept_state.mmio_mapped_region_count,
				ept_state.mmio_mapping_failure_count,
				ept_state.last_mmio_mapped_guest_physical,
				ept_state.ept_1gb_pages_supported ? "支持" : "不支持");
		}
		ept_state.mmio_mapped_region_count = 0;
		ept_state.mmio_mapping_failure_count = 0;
		ept_state.last_mmio_mapped_guest_physical = 0;
		ept_state.ept_1gb_pages_supported = false;

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
		UNREFERENCED_PARAMETER(mtrrs); // EPT 下 guest MTRR 不参与 GPA 访问的最终内存类型计算。
		if (!is_physical_range_ram(ept_state, physical_address, size))
		{
			return MEMORY_TYPE_UNCACHEABLE; // 非RAM/MMIO必须保守映射为UC，避免设备空间被WB缓存造成整机卡死。
		}

		return MEMORY_TYPE_WRITE_BACK; // RAM 固定使用 WB EPT 类型；guest PAT 仍可按 VMCS GUEST_PAT 参与组合。
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

	// 非 RAM 区域按需映射/降级使用的 PD 页池：池页全部在构建期分配，并缓存自身物理页帧号。
	// EPT violation 与 VMCALL hook/watch 安装都不能在热路径重新分配内存，因此这里一次性把资源备齐。
	// 返回 false 时 mmio_pd_pool_capacity 只包含已成功分配的页数，由 cleanup_page_table 负责释放。
	static bool allocate_mmio_pd_pool(__ept_state& ept_state)
	{
		ept_state.mmio_pd_pool =
			allocate_pool<__ept_mmio_pd_page*>(sizeof(__ept_mmio_pd_page) * EPT_MMIO_PD_POOL_PAGE_COUNT);
		if (ept_state.mmio_pd_pool == nullptr)
		{
			LogError("Failed to allocate EPT on-demand MMIO PD pool descriptors");
			outFile("[失败][EPT页表] MMIO按需PD池描述表分配失败：Pages=%u", EPT_MMIO_PD_POOL_PAGE_COUNT);
			return false;
		}

		RtlSecureZeroMemory(ept_state.mmio_pd_pool, sizeof(__ept_mmio_pd_page) * EPT_MMIO_PD_POOL_PAGE_COUNT);
		ept_state.mmio_pd_pool_used = 0;

		for (unsigned __int32 i = 0; i < EPT_MMIO_PD_POOL_PAGE_COUNT; ++i)
		{
			__ept_pde_page* pd_page = allocate_page_aligned_pool<__ept_pde_page*>(sizeof(__ept_pde_page));
			if (pd_page == nullptr)
			{
				LogError("Failed to allocate EPT on-demand MMIO PD page %u of %u", i, EPT_MMIO_PD_POOL_PAGE_COUNT);
				outFile("[失败][EPT页表] MMIO按需PD池页分配失败：Index=%u Total=%u", i, EPT_MMIO_PD_POOL_PAGE_COUNT);
				return false;
			}

			RtlSecureZeroMemory(pd_page, sizeof(__ept_pde_page));
			ept_state.mmio_pd_pool[i].entries = &pd_page->entries[0];
			ept_state.mmio_pd_pool[i].page_frame_number = GET_PFN(MmGetPhysicalAddress(pd_page).QuadPart);
			ept_state.mmio_pd_pool_capacity = i + 1; // 逐个提交：初始化中途失败时只释放已经分配成功的页
		}

		outFile("[成功][EPT页表] MMIO按需PD池分配完成：Pages=%u Bytes=%llu 可覆盖MMIO窗口=%u个1GB区域",
			ept_state.mmio_pd_pool_capacity,
			static_cast<unsigned __int64>(ept_state.mmio_pd_pool_capacity) * PAGE_SIZE,
			ept_state.mmio_pd_pool_capacity);
		return true;
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
		ept_state.ept_1gb_pages_supported = use_1gb_ept_pages; // 运行期按需映射按它选粒度，构建期只查一次

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
		outFile("[信息][CPU%u][EPT页表] 映射规模：PDPT=%u PML4=%u CoverageGB=%u",
			KeGetCurrentProcessorNumber(),
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
			free_vmm_page_aligned(ept_state.ept_page_table);
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
				free_vmm_page_aligned(ept_state.ept_page_table);
				ept_state.ept_page_table = nullptr;
				return false;
			}
			RtlSecureZeroMemory(ept_state.extended_pml3_pages, sizeof(__ept_pdpte_page*) * (required_pml4_count - 1));
			outFile("[成功][EPT页表] 扩展PDPT指针表分配完成：ExtCount=%u", required_pml4_count - 1);
		}

		// 即使 CPU 支持 1GB EPT 大页，运行期 hook/watch 仍可能要求把 1GB MMIO 降级到 2MB，
		// 所以两类 CPU 都必须预分配 PD 页池；1GB 路径只在首次需要 4KB 粒度时取用池页。
		if (!allocate_mmio_pd_pool(ept_state))
		{
			cleanup_page_table(ept_state);
			free_vmm_page_aligned(ept_state.ept_page_table);
			ept_state.ept_page_table = nullptr;
			return false;
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
				__ept_pdpte_page* pdpt_page = allocate_page_aligned_pool<__ept_pdpte_page*>(sizeof(__ept_pdpte_page));
				if (pdpt_page == nullptr)
				{
					LogError("Failed to allocate EPT extended PDPT page %u", pml4_index);
					outFile("[失败][EPT页表] 扩展PDPT页分配失败：PML4Index=%u", pml4_index);
					cleanup_page_table(ept_state);
					free_vmm_page_aligned(ept_state.ept_page_table);
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

		for (unsigned __int32 i = 0; i < required_pdpt_count; ++i)
		{
			__ept_pdpte* pdpte = get_pdpte_entry(ept_state, i);
			if (pdpte == nullptr)
			{
				LogError("EPT PDPTE lookup failed for index %u", i);
				outFile("[失败][EPT页表] PDPTE查找失败：FlatPDPT=%u", i);
				cleanup_page_table(ept_state);
				free_vmm_page_aligned(ept_state.ept_page_table);
				ept_state.ept_page_table = nullptr;
				return false;
			}

			// 该 1GB 区域完全不含 RAM（纯 MMIO / 地址空洞）：构建期只留 not-present 条目，
			// 首次访问时由 ensure_mmio_mapping 按需补映射 —— 支持 1GB EPT 大页的 CPU 填一个 1GB UC 条目，
			// 否则从预分配 PD 池取一页填 512 个 2MB UC 条目。
			// 必须按需的原因：若构建期给每个空洞区域都分配 PD 页，MAXPHYADDR=46 的 CPU 需要 65536 个
			// PD 页（≈256MB/每 vCPU），既耗尽非分页池，也让 VT 加载时间不可接受。
			// EPT 条目的 not-present 编码是 R/W/X 三位全 0，因此这里把 PDPTE 整体清零。
			unsigned __int64 const region_base = static_cast<unsigned __int64>(i) * ept_1gb_page_size;
			if (!physical_range_overlaps_ram(ept_state, region_base, ept_1gb_page_size))
			{
				pdpte->all = 0; // 非 RAM 区域：留空，等首次访问补映射
				ept_state.pml2_pages[i] = nullptr; // 没有 PD 页，hook/MTRR 遍历会跳过该区域
				continue;
			}

			__ept_pde_page* dynamic_pde_page = allocate_page_aligned_pool<__ept_pde_page*>(sizeof(__ept_pde_page));
			if (dynamic_pde_page == nullptr)
			{
				LogError("Failed to allocate EPT PD page %u", i);
				outFile("[失败][EPT页表] PD页分配失败:FlatPDPT=%u", i);
				cleanup_page_table(ept_state);
				free_vmm_page_aligned(ept_state.ept_page_table);
				ept_state.ept_page_table = nullptr;
				return false;
			}

			RtlSecureZeroMemory(dynamic_pde_page, sizeof(__ept_pde_page));
			__ept_pde* pde_page = &dynamic_pde_page->entries[0];

			ept_state.pml2_pages[i] = pde_page;

			// 与分配同一个循环完成整页大页模板填充：pml2_pages[i] 一旦发布即为完整条目
			__stosq((unsigned __int64*)pde_page, pde_template.all, 512);

			pdpte->all = pdpte_template.all;
			pdpte->page_frame_number = GET_PFN(MmGetPhysicalAddress(pde_page).QuadPart);
		}

		// MTRR data for setting memory types
		// 用于设置内存类型的 MTRR 数据
		auto const mtrrs = hv::read_mtrr_data();
		unsigned __int32 pd_page_count = 0;
		unsigned __int32 on_demand_region_count = 0;
		for (unsigned __int32 i = 0; i < required_pdpt_count; i++)
		{
			if (ept_state.pml2_pages[i] == nullptr)
			{
				++on_demand_region_count; // 非 RAM 区域：构建期不建 PD，首次访问时由 ensure_mmio_mapping 补映射
				continue;
			}

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

		outFile("[信息][CPU%u][EPT页表] 页表构成：含RAM的1GB区域=%u 个（PD页），非RAM按需映射区域=%u 个",
			KeGetCurrentProcessorNumber(),
			pd_page_count,
			on_demand_region_count);
		if (use_1gb_ept_pages)
		{
			outFile("[信息][EPT页表] 非RAM按需映射：粒度=1GB EPT大页（每个区域首次访问时补一个1GB条目）");
		}
		else
		{
			outFile("[信息][EPT页表] 非RAM按需映射：粒度=2MB页（CPU不支持1GB EPT大页） MMIO_PD池=%u页（每页覆盖1个1GB窗口）",
				ept_state.mmio_pd_pool_capacity);
		}

		outFile("---------- EPT页表构建完成：MappedPDPT=%u MappedPML4=%u ----------", ept_state.mapped_pdpte_count, ept_state.mapped_pml4_count);
		return true;
	}

	bool create_ept_page_table2(__ept_state& ept_state)
	{
		return create_ept_page_table(ept_state);
	}

	// 从预分配 PD 池取一页，填 512 个 UC 的 2MB EPT 条目，最后发布 PDPTE。
	// 写序要求：PD 页内容必须先对页表遍历可见，PDPTE 才能指向它（mfence 保证写序）。
	// 本函数运行在 VM-exit 路径：只做纯内存写入，不分配内存、不写文件日志。
	static bool publish_mmio_2mb_mapping(__ept_state& ept_state, __ept_pdpte* pdpte, unsigned __int64 flat_pdpt_index)
	{
		if (ept_state.mmio_pd_pool == nullptr ||
			ept_state.mmio_pd_pool_used >= ept_state.mmio_pd_pool_capacity)
		{
			return false; // PD 池耗尽：上层按未处理 violation 走限次保护，bugcheck 现场会带出 GPA
		}

		__ept_mmio_pd_page const& slot = ept_state.mmio_pd_pool[ept_state.mmio_pd_pool_used];
		__ept_pde* pd_page = slot.entries;

		__ept_pde entry_template = { 0 };
		entry_template.page_directory_entry.read = 1;
		entry_template.page_directory_entry.write = 1;
		entry_template.page_directory_entry.execute = 1;
		entry_template.page_directory_entry.memory_type = MEMORY_TYPE_UNCACHEABLE; // MMIO 必须 UC，否则整机卡死
		entry_template.page_directory_entry.large_page = 1; // PS=1：2MB 大页
		entry_template.page_directory_entry.execute_for_usermode = 0;

		unsigned __int64 const first_2mb_pfn = flat_pdpt_index * 512; // 2MB 页帧号 = 物理地址 >> 21
		for (unsigned __int32 j = 0; j < 512; ++j)
		{
			__ept_pde entry = entry_template;
			entry.page_directory_entry.page_frame_number = first_2mb_pfn + j;
			pd_page[j] = entry;
		}

		++ept_state.mmio_pd_pool_used;

		_mm_mfence(); // 先把 PD 页内容写穿，再让 PDPTE 指向它

		__ept_pdpte new_pdpte = { 0 };
		new_pdpte.read = 1;
		new_pdpte.write = 1;
		new_pdpte.execute = 1;
		new_pdpte.execute_for_usermode = 0;
		new_pdpte.page_frame_number = slot.page_frame_number;
		ept_state.pml2_pages[flat_pdpt_index] = pd_page;
		pdpte->all = new_pdpte.all;

		return true;
	}

	// 将空 PDPTE 或 1GB EPT 大页转换为可拆分的 2MB PD。
	// 这里不负责 2MB -> 4KB 拆分；调用方在拿到 PDE 后继续走原有 split_pml2 路径。
	bool prepare_pml2_for_address(__ept_state& ept_state, unsigned __int64 physical_address)
	{
		unsigned __int64 const flat_pdpt_index =
			(MASK_EPT_PML4_INDEX(physical_address) << 9) + MASK_EPT_PML3_INDEX(physical_address);
		if (ept_state.pml2_pages == nullptr || flat_pdpt_index >= ept_state.mapped_pdpte_count)
		{
			LogError("EPT PML2 preparation outside identity map: pa=0x%llX flat_pdpt=%llu mapped=%u",
				physical_address,
				flat_pdpt_index,
				ept_state.mapped_pdpte_count);
			return false;
		}

		if (ept_state.pml2_pages[flat_pdpt_index] != nullptr)
		{
			return true;
		}

		__ept_pdpte* pdpte = get_pdpte_entry(ept_state, static_cast<unsigned __int32>(flat_pdpt_index));
		if (pdpte == nullptr)
		{
			LogError("EPT PDPTE lookup failed during PML2 preparation: flat_pdpt=%llu", flat_pdpt_index);
			return false;
		}

		if (pdpte->all != 0 && pdpte->large_page.large_page == 0)
		{
			LogError("EPT PDPTE already points to a PD but pml2_pages is empty: flat_pdpt=%llu", flat_pdpt_index);
			return false;
		}

		unsigned __int64 const region_base = flat_pdpt_index * ept_1gb_page_size;
		if (physical_range_overlaps_ram(ept_state, region_base, ept_1gb_page_size))
		{
			LogError("EPT RAM region has no PD during PML2 preparation: flat_pdpt=%llu", flat_pdpt_index);
			return false;
		}

		if (!publish_mmio_2mb_mapping(ept_state, pdpte, flat_pdpt_index))
		{
			LogError("Failed to publish MMIO 2MB mapping during PML2 preparation: flat_pdpt=%llu", flat_pdpt_index);
			return false;
		}

		return true;
	}

	/// <summary>
	/// 按需映射非 RAM 区域（MMIO / 地址空洞）。
	/// 构建期对“完全不含 RAM”的 1GB 区域刻意留空（PDPTE 的 R/W/X 全 0），首次访问时在这里补映射：
	/// 支持 1GB EPT 大页的 CPU 直接填一个 1GB UC 条目，否则从预分配 PD 池取一页填 512 个 2MB UC 条目。
	/// 只允许在 EPT violation 处理路径调用（VMX root + 宿主私有栈）：函数内不写文件日志、不分配内存。
	/// 返回 true 后调用方必须刷新 EPT 缓存（INVEPT）再让来宾重试指令。
	/// </summary>
	bool ensure_mmio_mapping(__ept_state& ept_state, unsigned __int64 physical_address)
	{
		unsigned __int64 const flat_pdpt_index =
			(MASK_EPT_PML4_INDEX(physical_address) << 9) + MASK_EPT_PML3_INDEX(physical_address);
		if (flat_pdpt_index >= ept_state.mapped_pdpte_count)
		{
			++ept_state.mmio_mapping_failure_count; // GPA 超出 EPT 覆盖范围：交给未处理路径统计与限次保护
			return false;
		}

		__ept_pdpte* pdpte = get_pdpte_entry(ept_state, static_cast<unsigned __int32>(flat_pdpt_index));
		if (pdpte == nullptr)
		{
			++ept_state.mmio_mapping_failure_count;
			return false;
		}

		if (pdpte->read != 0 || pdpte->write != 0 || pdpte->execute != 0)
		{
			return false; // 该区域已有映射（RAM 或已补映射的 MMIO）：只让上层刷新缓存重试，不计失败
		}

		unsigned __int64 const region_base = flat_pdpt_index * ept_1gb_page_size;
		if (physical_range_overlaps_ram(ept_state, region_base, ept_1gb_page_size))
		{
			++ept_state.mmio_mapping_failure_count; // 含 RAM 的区域构建期必须已映射：留空说明页表状态异常，不在热路径猜测
			return false;
		}

		if (!ept_state.ept_1gb_pages_supported)
		{
			if (!publish_mmio_2mb_mapping(ept_state, pdpte, flat_pdpt_index))
			{
				++ept_state.mmio_mapping_failure_count;
				return false;
			}
		}
		else
		{
			// 支持 1GB EPT 大页：一个 PDPTE 直接覆盖整段 1GB MMIO，零额外内存。
			__ept_pdpte entry = { 0 };
			entry.large_page.read = 1;
			entry.large_page.write = 1;
			entry.large_page.execute = 1;
			entry.large_page.memory_type = MEMORY_TYPE_UNCACHEABLE; // MMIO 必须 UC，否则整机卡死
			entry.large_page.ignore_pat = 0;
			entry.large_page.large_page = 1; // PS=1：本条目直接映射 1GB 大页
			entry.large_page.execute_for_usermode = 0;
			entry.large_page.page_frame_number = flat_pdpt_index; // 地址字段在 bit 30 起，值即 1GB 区域索引

			_mm_mfence();
			pdpte->all = entry.all;
		}

		++ept_state.mmio_mapped_region_count;
		ept_state.last_mmio_mapped_guest_physical = physical_address;
		return true;
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

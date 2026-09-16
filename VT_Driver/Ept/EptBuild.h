#pragma once
#include "../Driver.h"
#include "../Globals.h"

// EPT 页表构建和管理
namespace ept
{
	/// <summary>
	/// Setup page memory type
	/// 设置页内存类型
	/// </summary>
	/// <param name="entry"> Pointer to pml2 entry </param>
	/// <param name="pfn"> Page frame number </param>
	bool setup_pml2_entry(mtrr_data const& mtrrs, __ept_state& ept_state, __ept_pde& entry, unsigned __int64 pfn);

	/// <summary>
	/// 判断物理范围是否完全属于 RAM，避免把 MMIO 空洞按 WB 映射。
	/// </summary>
	bool is_physical_range_ram(__ept_state const& ept_state, unsigned __int64 physical_address, unsigned __int64 size);

	/// <summary>
	/// 根据 RAM/MMIO 边界和 MTRR 计算 EPT 内存类型。
	/// </summary>
	unsigned __int8 get_ept_memory_type_for_range(mtrr_data const& mtrrs, __ept_state const& ept_state, unsigned __int64 physical_address, unsigned __int64 size);

	/// <summary>
	/// Create ept page table
	/// 创建ept页表
	/// </summary>
	/// <returns> status </returns>
	bool create_ept_page_table(__ept_state& ept_state);

	/// <summary>
	/// 按需映射非 RAM 区域（MMIO / 地址空洞）。
	/// 只允许在 EPT violation 处理路径调用：构建期对“完全不含 RAM”的 1GB 区域刻意留空 PDPTE，
	/// CPU 支持 1GB EPT 大页时补一个 1GB UC 条目，否则从预分配 PD 页池取一页填 512 个 2MB UC 条目。
	/// 该函数运行在 VM-exit（VMX root）里，只做纯内存写入，不做文件日志、不再分配内存。
	/// 成功返回后调用方必须执行 INVEPT，让新映射对硬件页表遍历可见。
	/// </summary>
	/// <returns> true=已完成补映射，需要刷新 EPT 缓存 </returns>
	bool ensure_mmio_mapping(__ept_state& ept_state, unsigned __int64 physical_address);

	/// <summary>
	/// Create ept page table (alternative implementation)
	/// 创建ept页表（替代实现）
	/// </summary>
	/// <returns> status </returns>
	bool create_ept_page_table2(__ept_state& ept_state);

	/// <summary>
	/// Initialize ept structure
	/// 初始化 EPT 结构
	/// </summary>
	/// <returns>status</returns>
	/// <returns></returns>
	bool initialize(__ept_state& ept_state);

	/// <summary>
	/// Release dynamic EPT paging structures allocated beyond the embedded 512GB map.
	/// 释放超过内嵌 512GB 映射之外的动态 EPT 分页结构。
	/// </summary>
	void cleanup_page_table(__ept_state& ept_state);

	/// <summary>
	/// Get pml2 entry
	/// 获取 PML2 条目
	/// </summary>
	/// <param name="physical_address">物理地址</param>
	/// <returns> pointer to pml2 </returns>
	__ept_pde* get_pml2_entry(__ept_state& ept_state, unsigned __int64 physical_address);

	/// <summary>
	/// 解析GPA取得ept的pte
	/// </summary>
	/// <param name="physical_address">物理地址</param>
	/// <returns>指向 PML1 条目的指针</returns>
	__ept_pte* get_pml1_entry(__ept_state& ept_state, unsigned __int64 physical_address);

	/// <summary>
	/// Split pml2 into 512 pml1 entries (From one 2MB page to 512 4KB pages)
	/// 将pml2拆分为512个pml1项 (从一个2MB页面到512个4KB页面)
	/// </summary>
	/// <param name="pre_allocated_buffer"> Pre allocated buffer for split </param>
	/// <param name="physical_address">物理地址</param>
	/// <returns> status </returns>
	/// <param name="physical_address"></param>
	bool split_pml2(mtrr_data const& mtrrs, __ept_state& ept_state, void* pre_allocated_buffer, unsigned __int64 physical_address);

	/// <summary>
	/// 判断页面是否已被拆分（2MB -> 4KB）
	/// </summary>
	bool is_page_splitted(__ept_state& ept_state, unsigned __int64 physical_address);

	// Split 管理函数
	__ept_dynamic_split* ept_get_split_from_pml1(__ept_pte* pml1_entry);
	void ept_mark_runtime_split(void* split_buffer);
	void ept_split_ref_inc(__ept_dynamic_split* split);
	bool ept_split_try_merge_and_release(__ept_state& ept_state, __ept_dynamic_split* split);
	void ept_split_ref_dec_and_maybe_release(__ept_state& ept_state, __ept_dynamic_split* split);
}

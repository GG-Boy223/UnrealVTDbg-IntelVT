#pragma once
#include "../Driver.h"

// EPT MTRR 内存类型管理
namespace ept
{
	/// <summary>
	/// Build mtrr map to track physical memory type
	/// 构建mtrr映射以跟踪物理内存类型(又称缓存类型)
	/// </summary>
	void build_mtrr_map();

	/// <summary>
	/// Get page cache memory type
	/// 获取页缓存内存类型
	/// </summary>
	/// <param name="pfn">页帧号</param>
	/// <param name="is_large_page">是否是大页面</param>
	/// <returns>内存类型</returns>
	unsigned __int8 get_memory_type(unsigned __int64 pfn, bool is_large_page);

	/// <summary>
	/// Check if potential large page doesn't land on two or more different cache memory types
	/// 检查潜在的大页面是否跨越两个或多个不同的缓存内存类型
	/// </summary>
	/// <param name="pfn">页帧号</param>
	/// <returns>是否有效</returns>
	bool is_valid_for_large_page(unsigned __int64 pfn);
}

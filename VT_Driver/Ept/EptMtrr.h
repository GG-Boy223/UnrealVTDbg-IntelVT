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

}

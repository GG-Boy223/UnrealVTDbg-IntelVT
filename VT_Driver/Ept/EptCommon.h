#pragma once
#include "../Driver.h"
#include "../Globals.h"

// EPT 公共常量和全局变量声明
namespace ept
{
	// 常量定义
	constexpr unsigned __int64 kEptPageSize = 0x1000;
	constexpr unsigned __int64 kMaxWatchBytes = static_cast<unsigned __int64>(EPTWATCHLISTSIZE) * kEptPageSize;

	// 辅助函数：验证和检查
	unsigned int normalize_vcpu_count(int requested_count);
	bool is_valid_watch_range(unsigned __int64 virtual_address, unsigned __int64 size);
	bool is_watch_id_valid(int ID);

	// 初始化相关
	void initialize_hooked_page_runtime_fields(__ept_hooked_page_info* hooked_page_info);
}

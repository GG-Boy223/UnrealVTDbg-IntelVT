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
	void initialize_hooked_function_runtime_fields(__ept_hooked_function_info* hooked_function_info);

	// 热路径安全引用
	__ept_hooked_page_info* acquire_page_hot_reference(
		__ept_state& ept_state,
		unsigned __int64 pfn,
		int required_options);
	void release_page_hot_reference(
		__ept_state& ept_state,
		__ept_hooked_page_info* hooked_page_info);

	__ept_hooked_function_info* acquire_function_hot_reference(
		__ept_hooked_page_info* hooked_page_info,
		unsigned __int64 virtual_address);
	void release_function_hot_reference(
		__ept_hooked_function_info* hooked_function_info);

	__ept_hooked_page_info* acquire_any_page_hot_reference(
		__ept_state& ept_state,
		unsigned __int64 pfn);

	__ept_hooked_page_info* find_hooked_page_for_pfn(
		__ept_state& ept_state,
		unsigned __int64 pfn,
		int required_options);
	void insert_hooked_page(
		__ept_state& ept_state,
		__ept_hooked_page_info* hooked_page_info);
	void remove_hooked_page(
		__ept_state& ept_state,
		__ept_hooked_page_info* hooked_page_info);
	void insert_hooked_function(
		__ept_hooked_page_info* hooked_page_info,
		__ept_hooked_function_info* hooked_function_info);
	void remove_hooked_function(
		__ept_hooked_page_info* hooked_page_info,
		__ept_hooked_function_info* hooked_function_info);
	void reclaim_retired_resources(__ept_state& ept_state);
}

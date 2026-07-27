#pragma once // 头文件只被编译器包含一次，防止重复定义错误

#ifndef _HYPERVISOR_GATEWAY_H // 头文件保护宏开始，避免多次包含导致重定义
#define _HYPERVISOR_GATEWAY_H // 定义保护宏

namespace hvgt // Hypervisor Gateway 命名空间：提供对外的“网关”接口
{
	/// <summary>
	/// Turn off virtual machine
	/// </summary>
	bool vmoff(unsigned __int32 processor_count); // 关闭虚拟化：仅当所有目标CPU确认退出VMX时返回true

	/// <summary>
	/// Invalidates mappings in the translation lookaside buffers (TLBs)
	/// and paging-structure caches that were derived from extended page tables (EPT)
	/// </summary>
	/// <param name="invept_all"> If true invalidates all contexts otherway invalidate only single context (currently hv doesn't use more than 1 context)</param>
	void invept(bool invept_all); // 刷新 EPT 缓存：true 刷新全部上下文，false 刷新单一上下文

	/// <summary>
	/// Set/Unset presence of hypervisor
	/// </summary>
	/// <param name="value"> If false, hypervisor is not visible via cpuid interface, If true, it become visible</param>
	void hypervisor_visible(bool value); // 控制 Hypervisor 是否在 CPUID 中可见（隐藏/显示）

	/// <summary>
	/// Unhook all pages and invalidate tlb
	/// </summary>
	/// <returns> status </returns>
	bool ept_unhook(); // 卸载所有 EPT Hook，并刷新相关缓存

	/// <summary>
	/// Unhook single page and invalidate tlb
	/// </summary>
	/// <param name="page_physcial_address"></param>
	/// <returns> status </returns>
	bool ept_unhook(void* function_address); // 卸载指定函数/页的 EPT Hook，并刷新缓存

	/// <summary>
	/// Hook function via ept and invalidates mappings
	/// </summary>
	/// <param name="target_address">Address of function which we want to hook</param>
	/// <param name="hook_function">Address of function which is used to call original function</param>
	/// <param name="origin_function">Address of function which is used to call original function</param>
	/// <returns> status </returns>
	bool hook_function(void* target_address, void* proxy_function, void** origin_function); // 对函数建立 EPT Hook，返回原函数入口

	/// <summary>
	/// Hook function via ept and invalidates mappings
	/// </summary>
	/// <param name="target_address">Address of function which we want to hook</param>
	/// <param name="hook_function">Address of function which is used to call original function</param>
	/// <param name="trampoline_address">Address of codecave which is at least 14 bytes in size and in 2GB range of target function address
	/// Use only if function you want to hook uses some relatives jmps/moves in first 14 bytes</param>
	/// <param name="origin_function">Address of function which is used to call original function</param>
	/// <returns> status </returns>
	bool hook_function(void* target_address, void* proxy_function, void* trampoline_address, void** origin_function); // 带 trampoline 的 Hook（当目标函数前若干字节包含相对跳转时使用）

	/// <summary>
	/// Check if we can communicate with hypervisor
	/// </summary>
	/// <returns> status </returns>
	bool test_vmcall(); // 测试 VMCALL 通道是否可用

	/// <summary>
	/// Send irp with information to allocate memory
	/// </summary>
	/// <returns> status </returns>
	bool send_irp_perform_allocation(); // 通过 IOCTL 触发 Hypervisor 分配内存（预留接口）
}

#endif // !_HYPERVISOR_GATEWAY_H // 头文件保护宏结束

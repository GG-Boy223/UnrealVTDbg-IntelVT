#pragma once

#ifndef _EPT_H
#define _EPT_H

#include "Driver.h"
#include "Globals.h"
#include "invalid_ept.h" // INVEPT 描述符与接口声明

namespace ept
{
	/// <summary>
	/// Build mtrr map to track physical memory type
	/// </summary>
	void build_mtrr_map();

	/// <summary>
	/// Initialize ept structure
	/// </summary>
	/// <returns></returns>
	bool initialize(__ept_state& ept_state);

	void cleanup_page_table(__ept_state& ept_state);

	bool is_physical_range_ram(__ept_state const& ept_state, unsigned __int64 physical_address, unsigned __int64 size);
	unsigned __int8 get_ept_memory_type_for_range(mtrr_data const& mtrrs, __ept_state const& ept_state, unsigned __int64 physical_address, unsigned __int64 size);

	/// <summary>
	/// 交换物理页面并刷新tlb
	/// </summary>
	/// <param name="entry_address"> Pointer to page table entry which we want to change </param>
	/// <param name="entry_value"> Pointer to page table entry which we want use to change </param>
	/// <param name="invalidate"> If true invalidates tlb after changning pte value </param>
	/// <param name="invalidation_type"> Specifiy if we want to invalidate single context or all contexts  </param>
	void swap_pml1_and_invalidate_tlb(__ept_state& ept_state, __ept_pte* entry_address, __ept_pte entry_value, invept_type invalidation_type);

	/// <summary>
	/// Unhook all functions and invalidate tlb
	/// </summary>
	void unhook_all_functions(__ept_state& ept_state);

	//通过vmcall进行hook 将vmcall指令插入到目标代码
	bool vmcall_hook_function(__ept_state& ept_state,
		void* target_function/*被挂钩的函数地址*/,
		void* proxy_function/*新函数地址*/,
		void** origin_function,
		unsigned __int64 target_cr3);

	//int3 hook
	bool cc_hook_function(__ept_state& ept_state, void* target_function/*被挂钩的函数地址*/, void* proxy_function/*新函数地址*/, void** origin_function);

	//#DB hook
	bool int1_hook_function(__ept_state& ept_state, void* target_function/*被挂钩的函数地址*/, void* proxy_function/*新函数地址*/, void** origin_function);

	/// <summary>
	/// Perfrom a hook
	/// </summary>
	/// <param name="target_address" > Address of function which we want to hook </param>
	/// <param name="hook_function"> Address of hooked version of function which we are hooking </param>
	/// <param name="(Optional) trampoline"> Address of codecave which is located in 2gb range of target function (Use only if you need smaller trampoline)</param>
	/// <param name="origin_function"> Address used to call original function </param>
	/// <returns></returns>
	bool hook_function(__ept_state& ept_state, void* target_address, void* proxy_function, void** origin_function);

	/// <summary>
	/// Unhook single function
	/// </summary>
	/// <param name="virtual_address"></param>
	/// <returns></returns>
	bool unhook_function(__ept_state& ept_state, unsigned __int64 virtual_address);

	/// <summary>
	/// Split pml2 into 512 pml1 entries (From one 2MB page to 512 4KB pages)
	/// </summary>
	/// <param name="pre_allocated_buffer"> Pre allocated buffer for split </param>
	/// <param name="physical_address"></param>
	/// <returns> status </returns>
	bool split_pml2(mtrr_data const& mtrrs, __ept_state& ept_state, void* pre_allocated_buffer, unsigned __int64 physical_address);


	bool ept_split_try_merge_and_release(__ept_state& ept_state, __ept_dynamic_split* split);
	void ept_split_ref_dec_and_maybe_release(__ept_state& ept_state, __ept_dynamic_split* split);

	//读取隐形软件断点
	bool get_hide_software_breakpoint(__ept_state& ept_state, PVT_BREAK_POINT vmcallinfo);

	//设置隐形软件断点
	bool set_hide_software_breakpoint(PVT_BREAK_POINT vmcallinfo);

	//写伪造页内存
	bool write_fake_page_memory(__ept_hooked_function_info* hooked_function_info, void* target_address, void* buffer, unsigned __int64 buffer_size);

	//读伪造页内存
	bool read_fake_page_memory(__ept_hooked_function_info* hooked_function_info, void* target_address, void* buffer, unsigned __int64 buffer_size);

	// 按 guest 虚拟地址和 CR3 读取/写入隐藏 INT3 对应页的 fake page 内容。
	bool get_software_breakpoint_buffer(
		__ept_state& ept_state,
		unsigned __int64 target_address,
		void* buffer,
		unsigned __int64 buffer_size,
		unsigned __int64 current_cr3);
	bool set_software_breakpoint_buffer(
		__ept_state& ept_state,
		unsigned __int64 target_address,
		void* buffer,
		unsigned __int64 buffer_size,
		unsigned __int64 current_cr3);

	bool handler_vmcall_rip(__ept_state& ept_state);

	//判断是否是监视的页
	int ept_isWatchPage(unsigned __int64 pfn, int ID);

	void apply_active_watch_permissions_locked(__ept_hooked_page_info* hooked_page_info, int ignoredID = -1);

	//设置ept监视
	bool ept_watch_activate(VT_BREAK_POINT vmcallinfo, unsigned __int64 Type, PVT_BREAK_POINT guest_request, int& errorCode);

	//取消监视断点
	int ept_watch_deactivate(VT_BREAK_POINT vmcallinfo, int ID);

	//处理断点监视事件
	bool ept_handleWatchEvent(__vcpu* vcpu,
		__ept_violation ept_violation,
		__ept_hooked_page_info* hooked_page_info,
		unsigned __int64 guest_physical_adddress,
		int& bpType);
	bool ept_handle_int3_execute(
		__vcpu* vcpu,
		__ept_hooked_page_info* hooked_page_info,
		unsigned __int64 guest_physical_address);
}

#endif // !_EPT_H

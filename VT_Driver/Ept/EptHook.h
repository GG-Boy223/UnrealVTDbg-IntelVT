#pragma once
#include "../Driver.h"
#include "../Globals.h"

EXTERN_C size_t __fastcall LDE(const void* lpData, unsigned int size);

// EPT Hook 功能
namespace ept
{
	// 跳板缓冲区容量：pool_manager 按 INTENTION_EXEC_TRAMPOLINE 预分配的单个缓冲区大小，
	// 申请跳板与写入跳板必须使用同一常量，避免申请大小与写入上限不一致。
	constexpr unsigned __int64 kTrampolineBufferSize = 100;

	// 写回原函数的绝对跳转长度：push imm32; mov dword ptr [rsp+4], imm32; ret。
	constexpr unsigned __int64 kAbsoluteJumpSize = 14;

	// 用目标函数所在的完整 4KB 原页初始化伪页。复制源必须是页首，不能再从
	// target_function 的页内偏移开始复制，否则伪页会整体错位。
	bool initialize_fake_page_from_target(__ept_hooked_page_info* hooked_page_info, const void* target_function);

	/// <summary>
	/// Write an absolute jump, We aren't touching any register except stack so it's the most safest trampoline
	/// Size: 14 bytes
	/// </summary>
	/// <param name="target_buffer"> Pointer to trampoline buffer </param>
	/// <param name="destination_address"> Address of place where we want to jump </param>
	void hook_write_absolute_jump(unsigned __int8* target_buffer, unsigned __int64 destination_address);

	/// <summary>
	/// Write relative jump,
	/// Size: 5 Bytes
	/// </summary>
	/// <param name="target_buffer"> Pointer to trampoline buffer </param>
	/// <param name="destination_address"> Address where we want to jump </param>
	/// <param name="source_address"> Address from which we want to jump </param>
	bool hook_write_relative_jump(unsigned __int8* target_buffer, unsigned __int64 destination_address, unsigned __int64 source_address);

	// 将vmcall指令写入伪造页  通过触发vmcall来实现hook
	bool write_vmcall_instruction_to_memory(__ept_hooked_function_info* hooked_function_info, void* target_function, void* proxy_function, void** origin_function);

	// 将int3指令写入伪造页  通过触发int3中断来实现hook
	bool write_cc_instruction_to_memory(__ept_hooked_function_info* hooked_function_info, void* target_function, void* proxy_function, void** origin_function);

	// 将int1写入伪页  进行hook
	bool write_int1_instruction_to_memory(__ept_hooked_function_info* hooked_function_info, void* target_function, void* proxy_function, void** origin_function);

	// Hook instruction memory
	bool hook_instruction_memory(__ept_state& ept_state, __ept_hooked_function_info* hooked_function_info, void* target_function, void* proxy_function, void** origin_function);

	// 如果是我们vmcall hook的地址则转到代理函数
	bool handler_vmcall_rip(__ept_state& ept_state);

	// 通过vmcall进行hook 将vmcall指令插入到目标代码
	bool vmcall_hook_function(__ept_state& ept_state, void* target_function, void* proxy_function, void** origin_function, unsigned __int64 target_cr3);

	// int3 hook
	//int3 hook
	bool cc_hook_function(__ept_state& ept_state, void* target_function, void* proxy_function, void** origin_function);

	// #DB hook
	//#DB hook
	bool int1_hook_function(__ept_state& ept_state, void* target_function, void* proxy_function, void** origin_function);

	/// <summary>
	/// Perfrom a hook
	/// </summary>
	/// <param name="target_address"> Address of function which we want to hook </param>
	/// <param name="proxy_function"> Address of hooked version of function which we are hooking </param>
	/// <param name="origin_function"> Address used to call original function </param>
	/// <returns></returns>
	bool hook_function(__ept_state& ept_state, void* target_function, void* proxy_function, void** origin_function);

	/// <summary>
	/// Unhook single function
	/// </summary>
	/// <param name="virtual_address"></param>
	/// <returns></returns>
	bool unhook_function(__ept_state& ept_state, unsigned __int64 virtual_address);

	/// <summary>
	/// Unhook all functions and invalidate tlb
	/// </summary>
	void unhook_all_functions(__ept_state& ept_state);

	// 写伪造页内存
	//写伪造页内存
	bool write_fake_page_memory(__ept_hooked_function_info* hooked_function_info, void* target_address, void* buffer, unsigned __int64 buffer_size);

	// 读伪造页内存
	//读伪造页内存
	bool read_fake_page_memory(__ept_hooked_function_info* hooked_function_info, void* target_address, void* buffer, unsigned __int64 buffer_size);
}

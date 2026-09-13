#include "../../EptHook.h"
#include "../../EptBuild.h"
#include "../../EptSync.h"
#include "../../EptCommon.h"
#include "../../../Globals.h"
#include "../../../EPT.h"
#include "../../../vmx.h"
#include "../../../hypervisor_routines.h"
#include "../../../Vmcs/VmcsApi.h"

namespace ept
{
	/// <summary>
	/// Write an absolute jump, We aren't touching any register except stack so it's the most safest trampoline
	/// Size: 14 bytes
	/// </summary>
	/// <param name="target_buffer"> Pointer to trampoline buffer </param>
	/// <param name="destination_address"> Address of place where we want to jump </param>
	void hook_write_absolute_jump(unsigned __int8* target_buffer, unsigned __int64 destination_address)
	{
		// push lower 32 bits of destination address
		target_buffer[0] = 0x68;
		*((unsigned __int32*)&target_buffer[1]) = (unsigned __int32)destination_address;

		// mov dword ptr [rsp + 4]
		target_buffer[5] = 0xc7;
		target_buffer[6] = 0x44;
		target_buffer[7] = 0x24;
		target_buffer[8] = 0x04;

		// higher 32 bits of destination address
		*((unsigned __int32*)&target_buffer[9]) = (unsigned __int32)(destination_address >> 32);

		// ret
		target_buffer[13] = 0xc3;
	}

	/// <summary>
	/// Write relative jump,
	/// Size: 5 Bytes
	/// </summary>
	/// <param name="target_buffer"> Pointer to trampoline buffer </param>
	/// <param name="destination_address"> Address where we want to jump </param>
	/// <param name="source_address"> Address from which we want to jump </param>
	bool hook_write_relative_jump(unsigned __int8* target_buffer, unsigned __int64 destination_address, unsigned __int64 source_address)
	{
		// destination - (source + sizeof instruction)
		if (target_buffer == nullptr)
		{
			return false;
		}

		const unsigned __int64 next_instruction = source_address + 0x5;
		const __int64 jmp_delta = static_cast<__int64>(destination_address) -
			static_cast<__int64>(next_instruction);
		if (jmp_delta < INT_MIN || jmp_delta > INT_MAX)
		{
			return false;
		}

		const __int32 jmp_value = static_cast<__int32>(jmp_delta);

		// relative jmp opcode
		target_buffer[0] = 0xe9;

		// set jmp offset
		*((__int32*)&target_buffer[1]) = jmp_value;
		// *((__int32*)&target_buffer[1]) = jmp_value;
		return true;
	}

	// 旧项目保留的禁用路径：vmcall 后再写入三个 0xCC，并把跳板覆盖长度提高到 6 字节。
	// 为什么注释：当前实现只需要写入 0F 01 C1 三字节 vmcall，跳板也只备份覆盖到完整指令长度；旧 6 字节方案是
	// 针对极短函数头部和 RIP 落到第三字节时的蓝屏暴露策略，属于强诊断策略。
	// 放开的好处：遇到 DbgkExitProcess、PspCallThreadNotifyRoutines 这类短函数头部时，异常更早暴露，便于发现
	// EPT 伪页切换时序问题。
	// 放开的坏处：额外 CC 会改变伪页后续字节，可能干扰原函数前 6 字节内的真实指令备份和异常分发。
	// 项目影响：EPT RIP hook 是核心路径，覆盖长度变化会影响 trampoline、MTF 恢复和 handler_vmcall_rip 的一致性。
	// 结论：继续注释；如要开启，应先针对短函数头部建立专项测试，并确认所有跳板长度按 6 字节处理。
	//bool write_vmcall_instruction_to_memory(__ept_hooked_function_info* hooked_function_info, void* target_function, void* proxy_function, void** origin_function)
	//{
	//	unsigned __int64 hooked_instructions_size = 0;
	//
	//	unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_function);
	//	unsigned __int8* target_buffer = &hooked_function_info->fake_page_contents[page_offset];
	//	target_buffer[0] = 0x0f;
	//	target_buffer[1] = 0x01;
	//	target_buffer[2] = 0xC1;
	//	target_buffer[3] = 0xCC;
	//	target_buffer[4] = 0xCC;
	//	target_buffer[5] = 0xCC;
	//	hooked_function_info->handler_function = proxy_function;
	//
	//	if (origin_function)
	//	{
	//		while (hooked_instructions_size < 6)
	//			hooked_instructions_size += LDE((unsigned __int8*)target_function + hooked_instructions_size, 64);
	//
	//		hooked_function_info->hook_size = hooked_instructions_size;
	//		RtlCopyMemory(hooked_function_info->first_trampoline_address, target_function, hooked_instructions_size);
	//		hook_write_absolute_jump(&hooked_function_info->first_trampoline_address[hooked_instructions_size],
	//			(unsigned __int64)target_function + hooked_instructions_size);
	//		*origin_function = hooked_function_info->first_trampoline_address;
	//	}
	//
	//	return true;
	//}

	//将vmcall指令写入伪造页  通过触发vmcall来实现hook
	bool write_vmcall_instruction_to_memory(__ept_hooked_function_info* hooked_function_info, void* target_function, void* proxy_function, void** origin_function)
	{
		unsigned __int64 hooked_instructions_size = 0;

		// Get offset of hooked function within page
		// 获得线性地址的低12位 页偏移
		unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_function);
		unsigned __int8* target_buffer = &hooked_function_info->fake_page_contents[page_offset];
		target_buffer[0] = 0x0f;
		target_buffer[1] = 0x01;
		target_buffer[2] = 0xC1;  //0F 01 C1 vmcall
		hooked_function_info->handler_function = proxy_function;

		//DbgBreakPoint();
		if (origin_function)
		{
			//计算被修改的指令占用多少字节
			while (hooked_instructions_size < 3)
				hooked_instructions_size += LDE((unsigned __int8*)target_function + hooked_instructions_size, 64);

			hooked_function_info->hook_size = hooked_instructions_size;

			// Copy overwritten instructions to trampoline buffer
			// 将覆盖的指令备份到跳板缓冲区
			RtlCopyMemory(hooked_function_info->first_trampoline_address, target_function, hooked_instructions_size);

			// Add the absolute jump back to the original function.
			// 添加绝对跳转回到原来的函数
			hook_write_absolute_jump(&hooked_function_info->first_trampoline_address[hooked_instructions_size], (unsigned __int64)target_function + hooked_instructions_size);

			// Return to user address of trampoline to call original function
			*origin_function = hooked_function_info->first_trampoline_address;
		}

		return true;
	}

	//将int3指令写入伪造页  通过触发int3中断来实现hook
	bool write_cc_instruction_to_memory(__ept_hooked_function_info* hooked_function_info, void* target_function, void* proxy_function, void** origin_function)
	{
		unsigned __int64 hooked_instructions_size = 1;

		// Get offset of hooked function within page
		// 获得线性地址的低12位 页偏移
		unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_function);
		hooked_function_info->hook_size = hooked_instructions_size;
		unsigned __int8* target_buffer = &hooked_function_info->fake_page_contents[page_offset];
		target_buffer[0] = 0xCC;
		hooked_function_info->handler_function = proxy_function;  //int3的处理函数

		//DbgBreakPoint();
		if (origin_function)
		{
			//计算被修改的指令占用多少字节
			hooked_instructions_size = LDE((unsigned __int8*)target_function, 64);

			// Copy overwritten instructions to trampoline buffer
			// 将覆盖的指令复制到跳板缓冲区
			RtlCopyMemory(hooked_function_info->first_trampoline_address, target_function, hooked_instructions_size);

			// Add the absolute jump back to the original function.
			// 添加绝对跳转回到原来的函数
			hook_write_absolute_jump(&hooked_function_info->first_trampoline_address[hooked_instructions_size], (unsigned __int64)target_function + hooked_instructions_size);

			// Return to user address of trampoline to call original function
			*origin_function = hooked_function_info->first_trampoline_address;
		}

		return true;
	}

	//将int1写入伪页  进行hook
	bool write_int1_instruction_to_memory(__ept_hooked_function_info* hooked_function_info, void* target_function, void* proxy_function, void** origin_function)
	{
		unsigned __int64 hooked_instructions_size = 1;

		// Get offset of hooked function within page
		// 获得线性地址的低12位 页偏移
		unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_function);
		hooked_function_info->hook_size = hooked_instructions_size;
		unsigned __int8* target_buffer = &hooked_function_info->fake_page_contents[page_offset];
		target_buffer[0] = 0xf1;  //int1
		hooked_function_info->handler_function = proxy_function;  //int1的处理函数

		//DbgBreakPoint();
		if (origin_function)
		{
			//计算被修改的指令占用多少字节
			hooked_instructions_size = LDE((unsigned __int8*)target_function, 64);

			// Copy overwritten instructions to trampoline buffer
			// 将覆盖的指令复制到跳板缓冲区
			RtlCopyMemory(hooked_function_info->first_trampoline_address, target_function, hooked_instructions_size);

			// Add the absolute jump back to the original function.
			// 添加绝对跳转回到原来的函数
			hook_write_absolute_jump(&hooked_function_info->first_trampoline_address[hooked_instructions_size], (unsigned __int64)target_function + hooked_instructions_size);

			// Return to user address of trampoline to call original function
			*origin_function = hooked_function_info->first_trampoline_address;
		}

		return true;
	}

	/// <summary>
	///
	/// </summary>
	/// <param name="hooked_page"> Pointer to __ept_hooked_page_info structure which holds info about hooked page </param>
	/// <param name="target_function"> Address of function which we want to hook </param>
	/// <param name="proxy_function"> Address of hooked version of function which we are hooking </param>
	/// <param name="origin_function"> Address used to call original function </param>
	/// <returns></returns>
	bool hook_instruction_memory(__ept_state& ept_state, __ept_hooked_function_info* hooked_function_info, void* target_function, void* proxy_function, void** origin_function)
	{
		unsigned __int64 hooked_instructions_size = 0;

		// Get offset of hooked function within page
		// 获得线性地址的低12位 页偏移
		unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_function);

		// If first 14 bytes of function are on 2 separate pages then return (Hypervisor doesn't support function hooking at page boundaries)
		// 判断页偏移 >4095  即判断是否跨入到下一个4K页面
		if ((page_offset + 14) > PAGE_SIZE - 1)
		{
			LogError("Function at page boundary");
			return false;
		}

		// Get the full size of instructions necessary to copy
		while (hooked_instructions_size < 14)
			hooked_instructions_size += LDE((unsigned __int8*)target_function + hooked_instructions_size, 64);

		// If instructions to hook are on two seperate pages then return (Hypervisor doesn't support function hooking at page boundaries)
		if ((hooked_instructions_size + 14) > PAGE_SIZE - 1)
		{
			LogError("Function at page boundary");
			return false;
		}

		hooked_function_info->hook_size = hooked_instructions_size;

		//
		// Now it's trampoline so we don't have to store origin function
		if (origin_function == nullptr)
		{
			// Write the absolute jump to our shadow page memory to jump to our hooked_page.
			hook_write_absolute_jump(&hooked_function_info->fake_page_contents[page_offset], (unsigned __int64)proxy_function);

			return true;
		}

		// Copy overwritten instructions to trampoline buffer
		RtlCopyMemory(hooked_function_info->first_trampoline_address, target_function, hooked_instructions_size);

		// Add the absolute jump back to the original function.
		hook_write_absolute_jump(&hooked_function_info->first_trampoline_address[hooked_instructions_size], (unsigned __int64)target_function + hooked_instructions_size);

		// Return to user address of trampoline to call original function
		*origin_function = hooked_function_info->first_trampoline_address;

		// Write the absolute jump to our shadow page memory to jump to our hooked_page.
		hook_write_absolute_jump(&hooked_function_info->fake_page_contents[page_offset], (unsigned __int64)proxy_function);

		return true;
	}

	//如果是我们vmcall hook的地址则转到代理函数
	bool handler_vmcall_rip(__ept_state& ept_state)
	{
		unsigned __int64 guest_rip = hv::vmread(GUEST_RIP);
		unsigned __int64 physical_address = hv::get_physical_address(hv::ghv.system_cr3.flags, (PVOID)guest_rip);

		if (physical_address == 0)
		{
			return false;
		}

		__ept_hooked_page_info* hooked_page_info =
			acquire_page_hot_reference(ept_state, GET_PFN(physical_address), EPTO_HOOK_FUNCTION);
		if (hooked_page_info == nullptr)
		{
			return false;
		}

		__ept_hooked_function_info* hooked_function_info =
			acquire_function_hot_reference(hooked_page_info, guest_rip);
		if (hooked_function_info == nullptr)
		{
			release_page_hot_reference(ept_state, hooked_page_info);
			return false;
		}

		hv::vmwrite(GUEST_RIP, hooked_function_info->handler_function);

		release_function_hot_reference(hooked_function_info);
		release_page_hot_reference(ept_state, hooked_page_info);
		return true;
	}
}

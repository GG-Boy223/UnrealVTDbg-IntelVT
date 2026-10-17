#include "../../EptHook.h"
#include "../../EptBuild.h"
#include "../../EptSync.h"
#include "../../EptCommon.h"
#include "../../../Globals.h"
#include "../../../EPT.h"
#include "../../../vmx.h"
#include "../../../hypervisor_routines.h"
#include "../../../Vmcs/VmcsApi.h"
#include "../../../../Common/Ring0/Hvm/DebuggerVisibilityPolicy.h"

namespace ept
{
	namespace
	{
		constexpr size_t kMaxInstructionLength = 15;

		// 计算从 target_function 起、至少覆盖 min_bytes 字节所需覆盖的完整指令长度。
		// 返回 false 的三种情况：LDE 解码失败（返回 0）、单条指令超过 x64 上限 15 字节、
		// 覆盖指令加回跳超过跳板容量或跨出 4KB 页。
		// 这里运行在 VM-exit 上下文：无界解码会让整个物理 CPU 回不到宿主，因此取值必须先校验再使用。
		bool decode_hooked_instructions_size(
			const void* target_function,
			unsigned __int64 page_offset,
			unsigned __int64 min_bytes,
			unsigned __int64& hooked_instructions_size)
		{
			hooked_instructions_size = 0;

			while (hooked_instructions_size < min_bytes)
			{
				const size_t instruction_length = LDE(
					reinterpret_cast<const unsigned __int8*>(target_function) + hooked_instructions_size,
					64);

				// LDE 解码失败返回 0：按 0 递增会让本循环永不退出，物理 CPU 会卡在 VMX root 态。
				if (instruction_length == 0 ||
					instruction_length > kMaxInstructionLength)
				{
					return false;
				}

				hooked_instructions_size += instruction_length;
			}

			// 跳板后面还要写一个 kAbsoluteJumpSize 的绝对跳转，必须在固定池缓冲区内完成。
			if (hooked_instructions_size > kTrampolineBufferSize - kAbsoluteJumpSize)
			{
				return false;
			}

			// 覆盖范围必须完整落在同一个 4KB 页内：伪造页写入与卸载时的恢复都按页内偏移进行。
			if (hooked_instructions_size > kEptPageSize - page_offset)
			{
				return false;
			}

			return true;
		}

		// 只按当前 EPT 页记录查找函数；调用方必须保证当前 CPU 正在处理该 ept_state 对应的 VM-exit。
		bool try_acquire_hooked_function_by_address(
			__ept_state& ept_state,
			unsigned __int64 virtual_address,
			__ept_hooked_function_info** hooked_function_info,
			__ept_hooked_page_info** hooked_page_info)
		{
			*hooked_function_info = nullptr;
			*hooked_page_info = nullptr;

			for (PLIST_ENTRY current_page = ept_state.hooked_page_list.Flink;
				current_page != &ept_state.hooked_page_list;
				current_page = current_page->Flink)
			{
				__ept_hooked_page_info* page =
					CONTAINING_RECORD(current_page, __ept_hooked_page_info, hooked_page_list);

				if (page->Options != EPTO_HOOK_FUNCTION || page->removing != 0)
				{
					continue;
				}

				for (PLIST_ENTRY current_function = page->hooked_functions_list.Flink;
					current_function != &page->hooked_functions_list;
					current_function = current_function->Flink)
				{
					__ept_hooked_function_info* function =
						CONTAINING_RECORD(current_function, __ept_hooked_function_info, hooked_function_list);

					if (function->removing != 0 ||
						reinterpret_cast<unsigned __int64>(function->virtual_address) != virtual_address)
					{
						continue;
					}

					InterlockedIncrement(&page->hot_reference_count);
					InterlockedIncrement(&function->hot_reference_count);
					if (page->removing == 0 && function->removing == 0)
					{
						*hooked_function_info = function;
						*hooked_page_info = page;
						return true;
					}

					InterlockedDecrement(&function->hot_reference_count);
					InterlockedDecrement(&page->hot_reference_count);
					return false;
				}
			}

			return false;
		}

		// VMCALL 指令地址的候选值：
		//   1) GUEST_RIP 本身；
		//   2) GUEST_RIP - VM_EXIT_INSTRUCTION_LENGTH。
		// 不同 Intel 实现/退出类别对 VMCALL 的 RIP 语义可能表现为指令地址或下一条指令地址，
		// 两个候选都按函数入口精确匹配。instruction_length 非法时不做减法，避免 underflow。
		unsigned int build_vmcall_rip_candidates(
			unsigned __int64 guest_rip,
			unsigned __int64 instruction_length,
			unsigned __int64 candidates[2])
		{
			return hvm_debug_visibility::BuildVmcallRipCandidates(
				guest_rip,
				instruction_length,
				candidates);
		}

		// 在 VM-exit 内解析 VMCALL 对应的函数 hook。
		// 关键：必须使用当前 GUEST_CR3 做 GVA→GPA；hv::ghv.system_cr3 是宿主 dwm 地址空间
		// （见 Vmm/VmmHostPageTables.cpp），在 KVA shadow/进程地址空间切换后不能保证映射当前 guest 内核代码页。
		bool resolve_vmcall_hook(
			__ept_state& ept_state,
			unsigned __int64 guest_rip,
			unsigned __int64 instruction_length,
			__ept_hooked_function_info** hooked_function_info,
			__ept_hooked_page_info** hooked_page_info)
		{
			*hooked_function_info = nullptr;
			*hooked_page_info = nullptr;

			unsigned __int64 candidates[2] = {};
			const unsigned int candidate_count =
				build_vmcall_rip_candidates(guest_rip, instruction_length, candidates);

			cr3 guest_cr3 = {};
			guest_cr3.flags = hvm_debug_visibility::SelectVmcallTranslationCr3(
				hv::vmread(GUEST_CR3),
				hv::ghv.system_cr3.flags);
			if (guest_cr3.flags != 0)
			{
				for (unsigned int index = 0; index < candidate_count; ++index)
				{
					const unsigned __int64 physical_address =
						hv::get_physical_address(guest_cr3.flags, reinterpret_cast<PVOID>(candidates[index]));
					if (physical_address == 0)
					{
						continue;
					}

					__ept_hooked_page_info* page = acquire_page_hot_reference(
						ept_state,
						GET_PFN(physical_address),
						EPTO_HOOK_FUNCTION);
					if (page == nullptr)
					{
						continue;
					}

					__ept_hooked_function_info* function =
						acquire_function_hot_reference(page, candidates[index]);
					if (function != nullptr)
					{
						*hooked_function_info = function;
						*hooked_page_info = page;
						return true;
					}

					release_page_hot_reference(ept_state, page);
				}
			}

			// 当前 guest CR3 翻译失败或 PFN 不匹配时，按精确虚拟地址扫描本 CPU 的 hook 记录。
			// 这只在异常路径执行，正常 VMCALL 仍走上面的页帧快路径。
			for (unsigned int index = 0; index < candidate_count; ++index)
			{
				if (try_acquire_hooked_function_by_address(
					ept_state,
					candidates[index],
					hooked_function_info,
					hooked_page_info))
				{
					return true;
				}
			}

			return false;
		}

		// 最后一道收敛路径：VMCALL 已经来自某个函数 hook 伪页，但函数记录查找失败。
		// 通过伪页中的 0F 01 C1 定位页记录，并优先按页内偏移恢复对应 handler；
		// 若记录确实不存在，则把原页恢复为可执行，避免把自定义 VMCALL 误报为 guest #UD 并蓝屏。
		bool recover_unattributed_vmcall(
			__ept_state& ept_state,
			unsigned __int64 guest_rip,
			unsigned __int64 instruction_length)
		{
			unsigned __int64 candidates[2] = {};
			const unsigned int candidate_count =
				build_vmcall_rip_candidates(guest_rip, instruction_length, candidates);

			for (PLIST_ENTRY current_page = ept_state.hooked_page_list.Flink;
				current_page != &ept_state.hooked_page_list;
				current_page = current_page->Flink)
			{
				__ept_hooked_page_info* page =
					CONTAINING_RECORD(current_page, __ept_hooked_page_info, hooked_page_list);
				if (page->Options != EPTO_HOOK_FUNCTION ||
					page->removing != 0 ||
					page->fake_page_contents == nullptr ||
					page->entry_address == nullptr)
				{
					continue;
				}

				for (unsigned int index = 0; index < candidate_count; ++index)
				{
					const unsigned __int64 page_offset = candidates[index] & 0xFFF;
					if (page_offset + 3 > PAGE_SIZE)
					{
						continue;
					}

					unsigned __int8* instruction = &page->fake_page_contents[page_offset];
					if (!hvm_debug_visibility::IsVmcallInstruction(instruction))
					{
						continue;
					}

					if (ept_state.page_to_change == page)
					{
						hv::clear_pending_mtf_metadata(&ept_state);
						hv::set_mtf(false);
					}

					for (PLIST_ENTRY current_function = page->hooked_functions_list.Flink;
						current_function != &page->hooked_functions_list;
						current_function = current_function->Flink)
					{
						__ept_hooked_function_info* function =
							CONTAINING_RECORD(current_function, __ept_hooked_function_info, hooked_function_list);
						if (function->removing != 0 ||
							function->handler_function == nullptr ||
							(reinterpret_cast<unsigned __int64>(function->virtual_address) & 0xFFF) != page_offset)
						{
							continue;
						}

						InterlockedIncrement(&page->hot_reference_count);
						InterlockedIncrement(&function->hot_reference_count);
						if (page->removing == 0 && function->removing == 0)
						{
							hv::vmwrite(GUEST_RIP, function->handler_function);
							release_function_hot_reference(function);
							release_page_hot_reference(ept_state, page);
							return true;
						}

						InterlockedDecrement(&function->hot_reference_count);
						InterlockedDecrement(&page->hot_reference_count);
					}

					__ept_pte restored_entry = page->original_entry;
					restored_entry.read = 1;
					restored_entry.write = 1;
					restored_entry.execute = 1;
					swap_pml1_and_invalidate_tlb(
						ept_state,
						page->entry_address,
						restored_entry,
						invept_single_context);
					return true;
				}
			}

			return false;
		}
	}

	bool initialize_fake_page_from_target(__ept_hooked_page_info* hooked_page_info, const void* target_function)
	{
		const unsigned __int64 target_address =
			reinterpret_cast<unsigned __int64>(target_function);
		const unsigned __int64 target_page_base =
			page_align_down(target_address);
		RtlCopyMemory(
			hooked_page_info->fake_page_contents,
			reinterpret_cast<const void*>(target_page_base),
			kEptPageSize);
		return true;
	}

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
		// Get offset of hooked function within page
		// 获得线性地址的低12位 页偏移
		unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_function);

		// 先完成解码与校验再改写伪造页：校验失败时不会留下半写入的指令。
		unsigned __int64 hooked_instructions_size = 0;
		if (decode_hooked_instructions_size(target_function, page_offset, 3, hooked_instructions_size) == false)
		{
			LogError("解析被覆盖指令长度失败");
			return false;
		}

		unsigned __int8* target_buffer = &hooked_function_info->fake_page_contents[page_offset];
		target_buffer[0] = 0x0f;
		target_buffer[1] = 0x01;
		target_buffer[2] = 0xC1;  //0F 01 C1 vmcall
		hooked_function_info->handler_function = proxy_function;

		//DbgBreakPoint();
		if (origin_function)
		{
			// 覆盖长度已在写入伪造页前完成解码与校验。

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
		// Get offset of hooked function within page
		// 获得线性地址的低12位 页偏移
		unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_function);

		// 先完成解码与校验再改写伪造页：校验失败时不会留下半写入的指令。
		unsigned __int64 hooked_instructions_size = 0;
		if (decode_hooked_instructions_size(target_function, page_offset, 1, hooked_instructions_size) == false)
		{
			LogError("解析被覆盖指令长度失败");
			return false;
		}

		// int3 只覆盖 1 字节，卸载时按 1 字节恢复。
		hooked_function_info->hook_size = 1;
		unsigned __int8* target_buffer = &hooked_function_info->fake_page_contents[page_offset];
		target_buffer[0] = 0xCC;
		hooked_function_info->handler_function = proxy_function;  //int3的处理函数

		//DbgBreakPoint();
		if (origin_function)
		{
			//计算被修改的指令占用多少字节
			// 跳板只需要复制被 0xCC 覆盖的那一条完整指令，长度已在上面解码并校验。

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
		// Get offset of hooked function within page
		// 获得线性地址的低12位 页偏移
		unsigned __int64 page_offset = MASK_EPT_PML1_OFFSET((unsigned __int64)target_function);

		// 先完成解码与校验再改写伪造页：校验失败时不会留下半写入的指令。
		unsigned __int64 hooked_instructions_size = 0;
		if (decode_hooked_instructions_size(target_function, page_offset, 1, hooked_instructions_size) == false)
		{
			LogError("解析被覆盖指令长度失败");
			return false;
		}

		// int1 只覆盖 1 字节，卸载时按 1 字节恢复。
		hooked_function_info->hook_size = 1;
		unsigned __int8* target_buffer = &hooked_function_info->fake_page_contents[page_offset];
		target_buffer[0] = 0xf1;  //int1
		hooked_function_info->handler_function = proxy_function;  //int1的处理函数

		//DbgBreakPoint();
		if (origin_function)
		{
			//计算被修改的指令占用多少字节
			// 跳板只需要复制被 0xf1 覆盖的那一条完整指令，长度已在上面解码并校验。

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

		// 14 字节绝对跳转必须完整落在目标页内，避免 LDE 越过页尾读取。
		if ((page_offset + kAbsoluteJumpSize) > PAGE_SIZE)
		{
			LogError("Function at page boundary");
			return false;
		}

		// Get the full size of instructions necessary to copy
		// 解码并校验覆盖长度：解码失败或越界时直接失败，避免在 VM-exit 上下文里死循环。
		if (decode_hooked_instructions_size(target_function, page_offset, kAbsoluteJumpSize, hooked_instructions_size) == false)
		{
			LogError("解析被覆盖指令长度失败");
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
		const unsigned __int64 guest_rip = hv::vmread(GUEST_RIP);
		const unsigned __int64 instruction_length = hv::vmread(VM_EXIT_INSTRUCTION_LENGTH);
		__ept_hooked_function_info* hooked_function_info = nullptr;
		__ept_hooked_page_info* hooked_page_info = nullptr;

		if (!resolve_vmcall_hook(
			ept_state,
			guest_rip,
			instruction_length,
			&hooked_function_info,
			&hooked_page_info))
		{
			return recover_unattributed_vmcall(ept_state, guest_rip, instruction_length);
		}

		hv::vmwrite(GUEST_RIP, hooked_function_info->handler_function);

		release_function_hot_reference(hooked_function_info);
		release_page_hot_reference(ept_state, hooked_page_info);
		return true;
	}
}

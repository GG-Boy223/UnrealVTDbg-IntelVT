#include "VmExitException.h"

void vmexit_unimplemented(__vcpu* vcpu) // 未实现 VM-exit 的兜底处理，向 guest 注入 #GP 避免静默继续。
{
	sLog("\n"); // 记录未实现路径，便于后续补齐具体 reason。
	hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT, INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
}

void vmexit_nmi_window_handler(__vcpu* vcpu) // 在 guest 解除 NMI blocking 后投递 queued_nmis。
{
	sLog("\n"); // NMI-window 是敏感路径，保留最小日志占位。
	if (vcpu->queued_nmis == 0)
	{
		auto ctrl = hv::read_ctrl_proc_based(); // 没有待投递 NMI 时读取控制字段准备关闭窗口退出。
		ctrl.nmi_window_exiting = 0; // 关闭 NMI-window exiting，避免无意义高频 VM-exit。
		hv::write_ctrl_proc_based(ctrl); // 写回 VM-execution control。
		return; // 队列为空，无需注入事件。
	}

	if (vm_entry_event_pending())
	{
		auto ctrl = hv::read_ctrl_proc_based(); // 已有 pending event 时不能覆盖 VM-entry injection 字段。
		ctrl.nmi_window_exiting = 1; // 保持窗口退出打开，等待下一次可投递时机。
		hv::write_ctrl_proc_based(ctrl); // 写回控制位。
		return; // 本轮不投递 NMI，避免丢失已有事件。
	}

	--vcpu->queued_nmis; // 消耗一个排队 NMI，后续通过 VM-entry 投递给 guest。

	hv::inject_nmi(); // 安排 VM-entry 注入 NMI。

	if (vcpu->queued_nmis == 0)
	{
		auto ctrl = hv::read_ctrl_proc_based(); // 队列清空后关闭 NMI-window exiting。
		ctrl.nmi_window_exiting = 0; // 防止继续因窗口开放而退出。
		hv::write_ctrl_proc_based(ctrl); // 写回控制位。
	}

	if (vcpu->queued_nmis > 0)
	{
		auto ctrl = hv::read_ctrl_proc_based(); // 队列仍有 NMI 时保持窗口退出。
		ctrl.nmi_window_exiting = 1; // 请求下一次可注入窗口。
		hv::write_ctrl_proc_based(ctrl); // 写回控制位。
	}
}

void vmexit_monitor_trap_flag_handler(__vcpu* vcpu) // MTF 单步窗口结束后恢复 EPT hook/watch 临时状态。
{
	hv::set_mtf(false); // 当前 MTF 已触发，先关闭 monitor trap flag 防止重复单步退出。

	const auto hooked_page_info = (vcpu != nullptr &&
		vcpu->ept_state != nullptr &&
		vcpu->ept_state->mtf_pending) ? vcpu->ept_state->page_to_change : nullptr; // 只处理被 mark_pending_mtf_page 标记过的页。
	if (hooked_page_info == nullptr)
	{
		hv::clear_pending_mtf_metadata(vcpu); // 没有有效 pending 页时清理残留元数据，避免后续误恢复。
		return; // 无可恢复页面，直接返回 guest。
	}

	if (hooked_page_info->Options == EPTO_HOOK_FUNCTION)
	{
		hooked_page_info->original_entry.execute = 0; // 函数 EPT hook 恢复为不可执行原页，下一次执行继续触发 EPT violation。
		ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
			hooked_page_info->entry_address,
			hooked_page_info->original_entry,
			invept_type::invept_single_context); // 替换 PML1 entry 并单上下文 INVEPT，确保 CPU 看到新权限。
		hv::clear_pending_mtf_metadata(vcpu); // 函数 hook 的一次性窗口已结束，清理 pending 标记。
	}

	if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
	{
		spinlock::lock(&eptWatchList_lock); // watch 列表是共享状态，MTF 恢复时必须持锁检查 ID 有效性。
		int ID = hooked_page_info->ID; // 取出 watch ID，后续访问 eptWatchList 前做边界/状态检查。
		if ((ID == -1) || (eptWatchList[ID].inuse == 0))
		{
			CleanUp(vcpu, hooked_page_info); // watch 已删除或 ID 无效时清理 hook 临时状态。
			spinlock::unlock(&eptWatchList_lock); // 异常返回前释放锁，避免全局 watch 死锁。
			return; // 不再访问失效 watch 条目。
		}

		if (hooked_page_info->isInt3)
		{
			cr3 guest_cr3; // 构造目标进程 CR3，用于按 guest 虚拟地址读取真实页面内容。
			guest_cr3.flags = eptWatchList[ID].cr3; // 使用 watch 记录中的 CR3，避免读到当前进程地址空间。

			if (!hooked_page_info->fake_page_synced || hooked_page_info->fake_page_needs_full_sync)
			{
				if (PAGE_SIZE != hv::read_guest_virtual_memory(guest_cr3, PAGE_ALIGN(eptWatchList[ID].VirtualAddress), &hooked_page_info->fake_page_contents, PAGE_SIZE))
				{
					hooked_page_info->fake_page_synced = false; // 读取失败时标记 fake page 不可信。
					hooked_page_info->fake_page_needs_full_sync = true; // 下次命中必须重新整页同步。
					CleanUp(vcpu, hooked_page_info); // 清理本次 pending 状态，避免卡住 MTF 生命周期。
					spinlock::unlock(&eptWatchList_lock); // 失败路径释放 watch 锁。
					return; // 无法构造 fake page 时不切换 EPT entry。
				}

				hooked_page_info->fake_page_synced = true; // fake page 内容已经与 guest 原页同步。
				hooked_page_info->fake_page_needs_full_sync = false; // 清除强制全量同步标记。
			}

			for (int i = 0; i < EPTWATCHLISTSIZE; i++)
			{
				if (ept::ept_isWatchPage(GET_PFN(eptWatchList[ID].PhysicalAddress), i) && eptWatchList[i].bpType == 3)
				{
					int offset = eptWatchList[i].VirtualAddress & 0xFFF; // 同页多个 INT3 watch 需要按页内偏移写入断点字节。
					hooked_page_info->fake_page_contents[offset] = 0xCC; // 在 fake page 上放置 INT3，不修改 guest 原始物理页。
				}
			}

			hooked_page_info->original_entry.execute = 0; // 恢复原页不可执行，让下一次执行再次进入 EPT watch 逻辑。
			ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
				hooked_page_info->entry_address,
				hooked_page_info->original_entry,
				invept_type::invept_single_context); // 刷新 EPT TLB，确保 fake/original 页权限即时生效。
		}
		else
		{
			ept::apply_active_watch_permissions_locked(hooked_page_info); // 按当前仍有效的 watch 列表恢复读/写/执行权限。

			if (hooked_page_info->isBp)
			{
				hv::inject_single_step(vcpu); // 命中硬件式 watch 后补一次 #DB，交给调试器处理单步事件。
			}
			_mm_mfence(); // 确保权限写入在 INVEPT 前对当前处理器可见。
			invept_single_context_func(vcpu->ept_state->ept_pointer->all); // 刷新当前 EPTP 上下文，避免旧权限缓存残留。
		}
		CleanUp(vcpu, hooked_page_info); // watch 恢复完成后清理一次性命中状态。
		spinlock::unlock(&eptWatchList_lock); // 释放全局 watch 列表锁。
	}
}

unsigned __int32 handler_breakpoint(__ept_state& ept_state, __vmexit_interrupt_info interrupt_info) // 处理 EPT hook 页上的 #BP，决定交给 guest 还是跳转 hook handler。
{
	sLog("\n"); // 记录 #BP 处理路径。
	unsigned __int64 guest_rip = hv::vmread(GUEST_RIP); // 当前 guest RIP 用于匹配 hook 函数入口。
	unsigned __int64 physical_address = MmGetPhysicalAddress((PVOID)guest_rip).QuadPart; // 转为物理地址以匹配 EPT hook 页 PFN。

	PLIST_ENTRY current = &ept_state.hooked_page_list; // 从 EPT state 的 hook 页链表头开始遍历。
	while (&ept_state.hooked_page_list != current->Flink)
	{
		current = current->Flink; // 移动到下一个 hook 页节点。
		__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list); // 从 LIST_ENTRY 还原 hook 页结构。

		if (hooked_page_info->pfn_of_hooked_page == GET_PFN(physical_address))
		{
			PLIST_ENTRY current_hooked_function = &hooked_page_info->hooked_functions_list; // 只在 PFN 匹配的页内遍历 hook 函数。
			while (&hooked_page_info->hooked_functions_list != current_hooked_function->Flink)
			{
				current_hooked_function = current_hooked_function->Flink; // 移动到下一个函数 hook 记录。
				__ept_hooked_function_info* hooked_function_info = CONTAINING_RECORD(current_hooked_function, __ept_hooked_function_info, hooked_function_list); // 还原函数 hook 信息。

				if (hooked_function_info->virtual_address == (PVOID)guest_rip)
				{
					if (hooked_function_info->breakpoint_address == guest_rip)
					{
						return 1; // 命中真实断点地址，保留给 guest/调试器处理。
					}
					else
					{
						hv::vmwrite(GUEST_RIP, hooked_function_info->handler_function); // 将 guest RIP 改到 hook handler，实现 EPT inline hook。
						return 2; // 返回 2 表示已重定向执行流。
					}
				}
			}
		}
	}
	return 1; // 未匹配 hook 记录时按普通 #BP 处理。
}

unsigned __int32 handler_debug(__ept_state& ept_state, __vmexit_interrupt_info interrupt_info) // 处理 EPT hook 页上的 #DB，逻辑与 #BP 匹配路径一致。
{
	sLog("\n"); // 记录 #DB 处理路径。
	unsigned __int64 guest_rip = hv::vmread(GUEST_RIP); // 读取当前 guest RIP。
	unsigned __int64 physical_address = MmGetPhysicalAddress((PVOID)guest_rip).QuadPart; // 转换为物理地址用于 PFN 匹配。

	PLIST_ENTRY current = &ept_state.hooked_page_list;
	while (&ept_state.hooked_page_list != current->Flink)
	{
		current = current->Flink;
		__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list);

		if (hooked_page_info->pfn_of_hooked_page == GET_PFN(physical_address))
		{
			PLIST_ENTRY current_hooked_function = &hooked_page_info->hooked_functions_list;
			while (&hooked_page_info->hooked_functions_list != current_hooked_function->Flink)
			{
				current_hooked_function = current_hooked_function->Flink;
				__ept_hooked_function_info* hooked_function_info = CONTAINING_RECORD(current_hooked_function, __ept_hooked_function_info, hooked_function_list);

				if (hooked_function_info->virtual_address == (PVOID)guest_rip)
				{
					if (hooked_function_info->breakpoint_address == guest_rip)
					{
						return 1; // 命中断点地址时保留原调试异常。
					}
					else
					{
						hv::vmwrite(GUEST_RIP, hooked_function_info->handler_function); // 非断点地址命中 hook 函数时重定向到 handler。
						return 2; // 已改写执行流。
					}
				}
			}
		}
	}
	return 1; // 未匹配时按普通 #DB 继续处理。
}

void vmexit_init_signal_handler(__vcpu* vcpu) // 处理 INIT signal VM-exit，恢复 guest activity state 后跳过当前状态。
{
	sLog("GUEST_ACTIVITY_STATE: %d\n", hv::vmread(GUEST_ACTIVITY_STATE)); // 记录 INIT 前 activity state。
	sLog("GUEST_RSP: 0x%llX\n", hv::vmread(GUEST_RSP)); // 记录栈指针，便于定位异常 INIT。
	sLog("GUEST_RIP: 0x%llX\n", hv::vmread(GUEST_RIP)); // 记录指令指针，便于复现问题。
	hv::vmwrite<unsigned __int64>(GUEST_ACTIVITY_STATE, vmx_active); // 将 guest activity state 恢复为 active。
	adjust_rip(vcpu); // 当前处理完成后推进 RIP。
}

void vmexit_sipi_handler(__vcpu* vcpu) // 处理 SIPI VM-exit，确保 guest activity state 为 active。
{
	sLog("GUEST_ACTIVITY_STATE: %d\n", hv::vmread(GUEST_ACTIVITY_STATE)); // 记录 SIPI 前 activity state。
	hv::vmwrite<unsigned __int64>(GUEST_ACTIVITY_STATE, vmx_active); // 激活 guest，允许后续 VMRESUME。
}

void vmexit_triple_fault_handler(__vcpu* vcpu) // triple fault 兜底处理，当前实现向 guest 注入 #GP。
{
	sLog("\n"); // 记录 triple fault 路径。
	hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
		INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // 以 #GP 反馈，避免直接让 host 崩溃。
}

void vmexit_failed(__vcpu* vcpu) // VM-entry/VM-exit 失败兜底处理。
{
	sLog("\n"); // 记录失败路径。
	hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
		INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // 向 guest 注入 #GP，保持故障显式可见。
}

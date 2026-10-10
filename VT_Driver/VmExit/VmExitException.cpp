#include "VmExitException.h"

#include "../Ept/EptWatch.h"

namespace
{
	void update_nmi_window_from_queue(__vcpu* vcpu)
	{
		auto ctrl = hv::read_ctrl_proc_based();
		ctrl.nmi_window_exiting = 0;
		hv::write_ctrl_proc_based(ctrl);

		if (vcpu->queued_nmi)
		{
			ctrl.nmi_window_exiting = 1;
			hv::write_ctrl_proc_based(ctrl);
		}
	}
}

void vmexit_unimplemented(__vcpu* vcpu) // 未实现 VM-exit 的兜底处理，向 guest 注入 #GP 避免静默继续。
{
	sLog("\n"); // 记录未实现路径，便于后续补齐具体 reason。
	hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT, INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true); // 注入 #GP(0) 给来宾：按硬件规则告知"非法/不支持的操作"，防止继续执行
}

void vmexit_nmi_window_handler(__vcpu* vcpu) // 在 guest 解除 NMI blocking 后投递 queued_nmi。
{
	sLog("\n"); // NMI-window 是敏感路径，保留最小日志占位。
	if (!vcpu->queued_nmi)
	{
		update_nmi_window_from_queue(vcpu); // NMI 可异步入队，先清窗口再按布尔标志决定是否重开。
		return; // 队列为空，无需注入事件。
	}

	if (vm_entry_event_pending())
	{
		auto ctrl = hv::read_ctrl_proc_based(); // 已有 pending event 时不能覆盖 VM-entry injection 字段。
		ctrl.nmi_window_exiting = 1; // 保持窗口退出打开，等待下一次可投递时机。
		hv::write_ctrl_proc_based(ctrl); // 写回控制位。
		return; // 本轮不投递 NMI，避免丢失已有事件。
	}

	vcpu->queued_nmi = false; // 消费待投递的 NMI，后续通过 VM-entry 投递给 guest。

	hv::inject_nmi(); // 安排 VM-entry 注入 NMI。

	update_nmi_window_from_queue(vcpu); // 处理期间新到的 NMI 会重新打开窗口，避免覆盖 host NMI handler 的写回。
}

//void vmexit_monitor_trap_flag_handler(__vcpu* vcpu) // 处理 MTF（Monitor Trap Flag）VM-exit，用于单步式的 EPT 钩子收尾
//{
//	hv::set_mtf(false);  // 关闭 MTF：本次单步完成后不再继续触发 MTF VM-exit
//
//	// 切记不可滥用 page_to_change：它只会在 EPT 违例时被记录
//	// 当它非空时，说明 MTF 是我们主动打开的，用来处理特定页面
//	// 处理结束后必须清空，否则可能对错误页面做恢复/替换
//	const auto hooked_page_info = vcpu->ept_state->page_to_change; // 取出本次需要处理的 EPT 钩子页面信息
//	if (hooked_page_info) // 仅当存在待处理页面时才进入后续逻辑
//	{
//		if (hooked_page_info->Options == EPTO_HOOK_FUNCTION) // 钩子类型：函数钩子（通常通过执行权限切换实现）
//		{
//			// 将原页设置为不可执行：下一次执行该页会触发 EPT 违例，从而切换到伪页
//			hooked_page_info->original_entry.execute = 0; // 禁止原页执行权限，制造下一次 EPT 违例
//			vcpu->ept_state->page_to_change = nullptr; // 清空 page_to_change，避免重复处理同一页
//			ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state, // 交换 PML1 表项并刷新 TLB，确保权限立即生效
//				hooked_page_info->entry_address, // 要更新的 EPT PML1 项地址
//				hooked_page_info->original_entry, // 更新后的原页表项（已禁用执行）
//				invept_type::invept_single_context); // 仅刷新当前 EPT 上下文，减少全局开销
//		}
//		if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT) // 钩子类型：虚拟断点（读/写/执行监控）
//			spinlock::lock(&eptWatchList_lock); // 加锁保护监视列表，避免并发修改
//			int ID = hooked_page_info->ID; // 取得监视条目 ID，用于索引 watchlist
//			if ((ID == -1) || (!ept_watch_entry_is_effective(eptWatchList[ID]))) // 若 ID 无效或条目未启用
//			{
//				CleanUp(vcpu, hooked_page_info); // 清理钩子状态，避免残留
//				spinlock::unlock(&eptWatchList_lock); // 释放锁
//				return; // 直接返回，停止后续处理
//			}
//			if (hooked_page_info->isInt3) // INT3（0xCC）断点类型
//				cr3 guest_cr3; // 临时保存来宾 CR3（页表基址）
//				guest_cr3.flags = eptWatchList[ID].cr3; // 使用该断点关联的 CR3，保证虚拟地址转换一致
//				if (!hooked_page_info->fake_page_synced || hooked_page_info->fake_page_needs_full_sync)
//				{
//					// 只有首次建立或显式标记 dirty 时才整页同步，避免高频 INT3 MTF 每次读 4KB。
//					if (PAGE_SIZE != hv::read_guest_virtual_memory(guest_cr3, PAGE_ALIGN(eptWatchList[ID].VirtualAddress), &hooked_page_info->fake_page_contents, PAGE_SIZE))
//					{
//						hooked_page_info->fake_page_synced = false;
//						hooked_page_info->fake_page_needs_full_sync = true;
//						CleanUp(vcpu, hooked_page_info); // 清理状态，避免残留
//						spinlock::unlock(&eptWatchList_lock); // 释放锁
//						return; // 终止处理
//					}
//					hooked_page_info->fake_page_synced = true;
//					hooked_page_info->fake_page_needs_full_sync = false;
//				}
//				// 整页同步会覆盖伪页中的 0xCC；平时只补写同页仍 active 的 INT3 字节。
//				for (int i = 0; i < EPTWATCHLISTSIZE; i++) // 遍历所有监视条目
//					if (ept::ept_isWatchPage(GET_PFN(eptWatchList[ID].PhysicalAddress), i) && eptWatchList[i].bpType == 3)
//						int offset = eptWatchList[i].VirtualAddress & 0xFFF; // 计算页内偏移（低 12 位）
//						hooked_page_info->fake_page_contents[offset] = 0xCC; // 写入 INT3 指令字节
//				// 再次将原页设为不可执行，以便下一次执行触发 EPT 违例并切换伪页
//				hooked_page_info->original_entry.execute = 0; // 禁止原页执行权限
//				ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state, // 交换页表项并刷新 TLB
//					hooked_page_info->entry_address, // 目标页表项地址
//					hooked_page_info->original_entry, // 更新后的原页表项
//					invept_type::invept_single_context); // 刷新当前 EPT 上下文
//			else // 非 INT3 的虚拟断点（读/写/执行监控）
//				ept::apply_active_watch_permissions_locked(hooked_page_info);
//				if (hooked_page_info->isBp)
//					hv::inject_single_step(vcpu);
//				// 刷新当前逻辑处理器的 EPT TLB，保证权限变化立即生效
//				_mm_mfence(); // 内存栅栏：保证页表修改先完成
//				invept_single_context_func(vcpu->ept_state->ept_pointer->all); // 执行 INVEPT 刷新单一 EPT 上下文
//			CleanUp(vcpu, hooked_page_info); // 处理结束后清理钩子状态
//			spinlock::unlock(&eptWatchList_lock); // 解锁监视列表
//	}
//}
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

	if (InterlockedCompareExchange(&hooked_page_info->removing, 0, 0) != 0)
	{
		CleanUp(vcpu, hooked_page_info); // 删除事务已接管该页，不能再写 EPT 或重建 watch 权限。
		return;
	}

	if (hooked_page_info->Options == EPTO_HOOK_FUNCTION) // 原理：函数钩子需要在执行/读写时切换页权限；作用：区分钩子类型；结果：进入函数钩子处理
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
		int const ID = hooked_page_info->ID;
		bool const is_int3 = hooked_page_info->isInt3;
		bool const is_bp = hooked_page_info->isBp;
		cr3 guest_cr3 = {};
		unsigned __int64 watch_virtual_address = 0;
		unsigned __int64 watch_physical_address = 0;

		spinlock::lock(&eptWatchList_lock); // 只在读取 watch 快照时持有全局锁。
		if ((ID < 0) ||
			(ID >= EPTWATCHLISTSIZE) ||
			!ept_watch_entry_owns_hardware(eptWatchList[ID]) ||
			GET_PFN(eptWatchList[ID].PhysicalAddress) != hooked_page_info->pfn_of_hooked_page)
		{
			CleanUp(vcpu, hooked_page_info); // watch 已删除或 ID 无效时清理 hook 临时状态。
			spinlock::unlock(&eptWatchList_lock);
			return; // 不再访问失效 watch 条目。
		}

		guest_cr3.flags = eptWatchList[ID].cr3; // 使用 watch 记录中的 CR3，避免读到当前进程地址空间。
		watch_virtual_address = eptWatchList[ID].VirtualAddress;
		watch_physical_address = eptWatchList[ID].PhysicalAddress;
		spinlock::unlock(&eptWatchList_lock);

		if (is_int3)
		{
			if (!hooked_page_info->fake_page_synced || hooked_page_info->fake_page_needs_full_sync)
			{
				if (PAGE_SIZE != hv::read_guest_virtual_memory(
					guest_cr3,
					reinterpret_cast<void*>(ept::page_align_down(watch_virtual_address)),
					&hooked_page_info->fake_page_contents,
					PAGE_SIZE))
				{
					hooked_page_info->fake_page_synced = false; // 读取失败时标记 fake page 不可信。
					hooked_page_info->fake_page_needs_full_sync = true; // 下次命中必须重新整页同步。
					CleanUp(vcpu, hooked_page_info); // 清理本次 pending 状态，避免卡住 MTF 生命周期。
					return; // 无法构造 fake page 时不切换 EPT entry。
				}

				hooked_page_info->fake_page_synced = true; // fake page 内容已经与 guest 原页同步。
				hooked_page_info->fake_page_needs_full_sync = false; // 清除强制全量同步标记。
			}

			spinlock::lock(&eptWatchList_lock);
			if (InterlockedCompareExchange(&hooked_page_info->removing, 0, 0) != 0 ||
				(ID < 0) ||
				(ID >= EPTWATCHLISTSIZE) ||
				!ept_watch_entry_owns_hardware(eptWatchList[ID]) ||
				eptWatchList[ID].bpType != 3 ||
				GET_PFN(eptWatchList[ID].PhysicalAddress) != GET_PFN(watch_physical_address))
			{
				spinlock::unlock(&eptWatchList_lock);
				CleanUp(vcpu, hooked_page_info);
				return;
			}

			bool has_effective_int3 = false; // 本页是否仍有 INT3 watch：决定原页要不要继续拦写。
			const unsigned __int64 watch_pfn = GET_PFN(eptWatchList[ID].PhysicalAddress);
			const unsigned __int64 guest_cr3 = hv::vmread(GUEST_CR3);
			const unsigned __int64 current_pid = ept::current_guest_process_id();
			for (int i = 0; i < EPTWATCHLISTSIZE; i++)
			{
				if (GET_PFN(eptWatchList[i].PhysicalAddress) != watch_pfn || eptWatchList[i].bpType != 3)
				{
					continue;
				}

				if (ept::ept_isWatchPage(watch_pfn, i) &&
					ept::watch_process_matches(eptWatchList[i], current_pid, guest_cr3))
				{
					int offset = eptWatchList[i].VirtualAddress & 0xFFF; // 同页多个 INT3 watch 需要按页内偏移写入断点字节。
					hooked_page_info->fake_page_contents[offset] = 0xCC; // 在 fake page 上放置 INT3，不修改 guest 原始物理页。
				}

				if (ept_watch_entry_is_effective(eptWatchList[i]))
				{
					has_effective_int3 = true;
				}
			}

			UNREFERENCED_PARAMETER(has_effective_int3);
			// 无论窗口期间 INT3 是否仍然存在，都必须按当前有效 watch 组合重算权限。
			// 旧路径在 has_effective_int3 时硬编码 read=1/write=0/execute=0，会覆盖同页
			// READWRITE watch 所需的 read=0，导致精确读监视在 INT3 MTF 收尾后被吞掉。
			ept::apply_active_watch_permissions_locked(hooked_page_info, -1);
			spinlock::unlock(&eptWatchList_lock);

			_mm_mfence();
			if (vcpu->ept_state->ept_pointer != nullptr)
			{
				invept_single_context_func(vcpu->ept_state->ept_pointer->all);
			}
		}
		else
		{
			spinlock::lock(&eptWatchList_lock);
			if (InterlockedCompareExchange(&hooked_page_info->removing, 0, 0) != 0 ||
				(ID < 0) ||
				(ID >= EPTWATCHLISTSIZE) ||
				!ept_watch_entry_owns_hardware(eptWatchList[ID]) ||
				eptWatchList[ID].bpType == 3 ||
				GET_PFN(eptWatchList[ID].PhysicalAddress) != GET_PFN(watch_physical_address))
			{
				spinlock::unlock(&eptWatchList_lock);
				CleanUp(vcpu, hooked_page_info);
				return;
			}

			ept::apply_active_watch_permissions_locked(hooked_page_info); // 按当前仍有效的 watch 列表恢复读/写/执行权限。
			spinlock::unlock(&eptWatchList_lock);

			if (is_bp)
			{
				hv::inject_single_step(vcpu); // 命中硬件式 watch 后补一次 #DB，交给调试器处理单步事件。
			}
			//刷新当前逻辑处理器的eptp
			_mm_mfence(); // 确保权限写入在 INVEPT 前对当前处理器可见。
			invept_single_context_func(vcpu->ept_state->ept_pointer->all); // 刷新当前 EPTP 上下文，避免旧权限缓存残留。
		}
		CleanUp(vcpu, hooked_page_info); // watch 恢复完成后清理一次性命中状态。
	}
}

//cpu重启信号
//CPU 重启信号（INIT）处理
void vmexit_init_signal_handler(__vcpu* vcpu) // 处理 INIT signal VM-exit，恢复 guest activity state 后跳过当前状态。
{
	sLog("GUEST_ACTIVITY_STATE: %d\n", hv::vmread(GUEST_ACTIVITY_STATE)); // 记录 INIT 前 activity state。
	sLog("GUEST_RSP: 0x%llX\n", hv::vmread(GUEST_RSP)); // 记录栈指针，便于定位异常 INIT。
	sLog("GUEST_RIP: 0x%llX\n", hv::vmread(GUEST_RIP)); // 记录指令指针，便于复现问题。
	hv::vmwrite<unsigned __int64>(GUEST_ACTIVITY_STATE, vmx_active); // 将 guest activity state 恢复为 active。
	UNREFERENCED_PARAMETER(vcpu); // INIT是异步事件，没有可推进的指令RIP。
}

void vmexit_sipi_handler(__vcpu* vcpu) // 处理 SIPI VM-exit，确保 guest activity state 为 active。
{
	sLog("GUEST_ACTIVITY_STATE: %d\n", hv::vmread(GUEST_ACTIVITY_STATE)); // 记录 SIPI 前 activity state。
	hv::vmwrite<unsigned __int64>(GUEST_ACTIVITY_STATE, vmx_active); // 激活 guest，允许后续 VMRESUME。
}

//处理 triple fault 的 VM-exit：三重故障表示来宾已无法继续执行，保存快照后进入统一fatal路径。
void vmexit_triple_fault_handler(__vcpu* vcpu)
{
	vmx_fatal_error(
		vmx_fatal_triple_fault,
		vcpu != nullptr ? vcpu->vmexit_info.guest_rip : 0,
		vcpu != nullptr ? vcpu->vmexit_info.qualification : 0);
}

//处理 VM-entry/VM-exit 失败路径：保存失败原因后确定性终止，禁止重新VM-entry。
void vmexit_failed(__vcpu* vcpu)
{
	vmx_fatal_error(
		vmx_fatal_vm_entry_failure,
		vcpu != nullptr ? vcpu->vmexit_info.reason : 0,
		vcpu != nullptr ? vcpu->vmexit_info.qualification : 0);
}

#include "VmExitEpt.h"
#include "../Ept/EptCommon.h"
#include "../Ept/EptBuild.h"
#include "../Cr3/Cr3Tracker.h"

namespace
{
	constexpr unsigned __int64 kMaxUnhandledEptViolationRetries = 64;

	// 孤儿权限自愈：页面不在 hooked_page_list，但 EPT 权限被限制（read/write/execute 任一为 0）
	// 或 PFN 指向伪页，说明是 watch/hook 删除或回滚残留的孤儿权限。恢复为完全可访问的原页，
	// 让 guest 重试成功后继续前进，避免「无法归因 -> 无限重试 -> 核困在 vmx-root -> 0x101」。
	// 覆盖 4KB 已拆分页和 2MB 大页两种粒度；1GB 大页（PDPTE 直接映射）权限在构建期固定，不在此处理。
	bool self_heal_orphaned_ept_page(__ept_state& ept_state, unsigned __int64 physical_address)
	{
		__ept_pte* const entry = ept::get_pml1_entry(ept_state, physical_address);
		if (entry != nullptr)
		{
			const bool restricted = entry->read == 0 || entry->write == 0 || entry->execute == 0;
			const bool pfn_mismatch = entry->page_frame_number != GET_PFN(physical_address);
			if (!restricted && !pfn_mismatch)
			{
				return false; // 页面完全可访问且指向原页，本次 violation 不属于孤儿权限
			}

			entry->read = 1;
			entry->write = 1;
			entry->execute = 1;
			entry->page_frame_number = GET_PFN(physical_address); // 归一回原页，丢弃残留伪页映射
			return true;
		}

		// 未拆分（2MB 大页）：检查并恢复 2MB 大页权限。2MB 大页 PFN 在 watch/hook 中从不被改写，
		// 只可能残留权限限制，因此只需恢复 read/write/execute。
		__ept_pde* const pde = ept::get_pml2_entry(ept_state, physical_address);
		if (pde != nullptr && pde->page_directory_entry.large_page == 1)
		{
			const bool restricted =
				pde->large_page.read == 0 ||
				pde->large_page.write == 0 ||
				pde->large_page.execute == 0;
			if (restricted)
			{
				pde->large_page.read = 1;
				pde->large_page.write = 1;
				pde->large_page.execute = 1;
				return true;
			}
		}

		return false;
	}
}

void vmexit_ept_misconfiguration_handler(__vcpu* vcpu) // 处理 EPT 配置错误(misconfiguration)导致的 VM-exit
{
	sLog("\n"); // 日志占位,用于调试定位
	auto const phys = hv::vmread(GUEST_PHYSICAL_ADDRESS); // 读取导致错误的客户机物理地址(GPA)
	vmx_fatal_error(
		vmx_fatal_ept_misconfiguration,
		phys,
		vcpu != nullptr ? vcpu->vmexit_info.qualification : 0);
}

//处理 EPT Violation 退出:根据读/写/执行违规类型处理 EPT 断点/函数钩子,并同步修改后的 EPT 条目
void vmexit_ept_violation_handler(__vcpu* vcpu) // vcpu=当前虚拟CPU状态
{
	__ept_violation ept_violation; // 原理:EPT 违规信息保存在 VM-exit qualification;作用:解析访问类型/权限;结果:ept_violation 字段可用

	ept_violation.all = vcpu->vmexit_info.qualification; // 原理:将 qualification 原值写入位域结构;作用:解析读/写/执行违规;结果:ept_violation 填充
	unsigned __int64 guest_physical_adddress = hv::vmread(GUEST_PHYSICAL_ADDRESS); // 固定读取GPA

	// 常规命中不扫描退休链；只有本次释放的确是被删除事务摘链的页/函数时，
	// 才在引用归零后进入回收安全点，避免每次访存监视命中都产生 O(n) 开销。

	__ept_hooked_page_info* hooked_page_info = nullptr;
	__ept_hooked_function_info* hooked_function_info = nullptr;

	if (vcpu != nullptr && vcpu->ept_state != nullptr)
	{
		// 热路径先按 GPA 页帧号获取页引用;删除事务置 removing 后无法取得新引用,
		// 因此本函数在整个生命周期内不会访问已被 release_pool 回收的节点。
		hooked_page_info = ept::acquire_page_hot_reference(
			*vcpu->ept_state,
			GET_PFN(guest_physical_adddress),
			0);
	}

	if (hooked_page_info == nullptr)
	{
		goto unhandled;
	}

	if ((ept_violation.read_access || ept_violation.write_access) &&
		(!ept_violation.ept_readable || !ept_violation.ept_writeable))
	{
		if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
		{
			//处理断点监视事件
			// 伪页是快照：任何被拦截的写入都说明原页内容已变，必须在 MTF 收尾时整页重新同步，
			// 否则下一次换入伪页执行的是过期指令流。这里按页判断而不依赖 ept_handleWatchEvent
			// 的 INT3 分支，是为了覆盖「同页还有数据 watch」与「命中缓存 ID」等其它子路径。
			if (ept_violation.write_access && hooked_page_info->changed_entry.page_frame_number != 0)
			{
				hooked_page_info->fake_page_needs_full_sync = true;
			}
			int bpType = 0;
			if (ept::ept_handleWatchEvent(vcpu, ept_violation, hooked_page_info, guest_physical_adddress, bpType))
			{
				if (bpType == 3)
				{
					if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
					{
						hv::set_mtf(false);
						// 页面已进入删除事务（removing != 0）。这里既不能切换表项也不能启用 MTF，
						// 但也**绝不能**直接 handled_return：那样不改变任何状态、来宾会立刻重试同一条
						// 指令并再次走到这里 —— 该逻辑处理器被永久困在 VMX root（IF=0）。
						// 一旦有核卡在 root，删除断点走的 KeGenericCallDpc 全核屏障就永远等不到它，
						// 表现为"删除所有断点后调试器与被调试进程一起卡死"。
						// 改走 unhandled 路径：释放热引用、按页计数、本地 INVEPT 后让来宾重试；
						// 删除事务完成后重试即成功（前向进展），条件持续不消失时也会在 64 次后
						// 收敛为可诊断的 vmx_fatal_error，而不是无解释的自旋。
						goto unhandled_after_release;
					}
					hooked_page_info->isInt3 = true;
					// 一次性放行窗口：重试的访问可能是读也可能是写（隐藏态已把 write 关掉），
					// 所以这里给原页完整权限，MTF 收尾时再恢复隐藏态。
					hooked_page_info->original_entry.read = 1;
					hooked_page_info->original_entry.write = 1;
					hooked_page_info->original_entry.execute = 1;
					hv::set_mtf(true);
					ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
						hooked_page_info->entry_address,
						hooked_page_info->original_entry,
						invept_type::invept_single_context);
				}
				else
				{
					if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
					{
						hv::set_mtf(false);
						// 同 bpType==3：无进展的 handled_return 会造成 root 态永久自旋。
						goto unhandled_after_release;
					}
					_mm_mfence();
					invept_single_context_func(vcpu->ept_state->ept_pointer->all);
				}
				goto handled_return;
			}
		}

		if (hooked_page_info->Options == EPTO_HOOK_FUNCTION)
		{
			if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
			{
				// 页面已进入删除事务（removing != 0）。这里不改变任何状态，
				// 若直接 handled_return，来宾会立刻重试同一条指令并再次走到这里，
				// 该逻辑处理器被永久困在 VMX root（IF=0）。改走 unhandled 路径：
				// 释放热引用、按页计数、本地 INVEPT 后让来宾重试。
				goto unhandled_after_release;
			}
			hooked_page_info->original_entry.execute = 1;
			hv::set_mtf(true);
			ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
				hooked_page_info->entry_address,
				hooked_page_info->original_entry,
				invept_type::invept_single_context);
			goto handled_return;
		}
	}
	else if (ept_violation.execute_access)
	{
		if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
		{
			int bpType = 0;
			const bool exactWatchHandled =
				ept::ept_handleWatchEvent(vcpu, ept_violation, hooked_page_info, guest_physical_adddress, bpType);
			if (exactWatchHandled && bpType != 3)
			{
				// 普通 EXECUTE watch 没有 fake page;放行当前指令并用 MTF 恢复组合权限。
				if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
				{
					// 同 EPTO_HOOK_FUNCTION 分支：无进展的 handled_return 会造成 root 态永久自旋。
					goto unhandled_after_release;
				}
				_mm_mfence();
				invept_single_context_func(vcpu->ept_state->ept_pointer->all);
				goto handled_return;
			}

			// ept_handleWatchEvent 返回 bpType==3 时说明缓存/分组扫描选中了 INT3；
			// 未命中普通精确 watch 且本页有伪页时也必须进入 INT3 执行路由。
			// 二者都统一调用 int3 处理，避免同一页上的执行型 bph 被静默吞掉。
			if ((exactWatchHandled && bpType == 3) ||
				hooked_page_info->changed_entry.page_frame_number != 0)
			{
				if (ept::ept_handle_int3_execute(vcpu, hooked_page_info, guest_physical_adddress))
				{
					goto handled_return;
				}
			}
			goto unhandled_after_release;
		}

		if (hooked_page_info->Options == EPTO_HOOK_FUNCTION)
		{
			// vmcall/int3/int1 伪页执行入口:先按 guest RIP 取函数引用,
			// 命中后再切换 GUEST_RIP;函数正在被 unhook 时返回未命中并走保护路径。
			hooked_function_info = ept::acquire_function_hot_reference(
				hooked_page_info,
				vcpu->vmexit_info.guest_rip);
			if (hooked_function_info != nullptr)
			{
				hv::vmwrite(GUEST_RIP, hooked_function_info->handler_function);
				ept::release_function_hot_reference(hooked_function_info);
				hooked_function_info = nullptr;
				goto handled_return;
			}

			ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
				hooked_page_info->entry_address,
				hooked_page_info->changed_entry,
				invept_type::invept_single_context);
			goto handled_return;
		}
	}

	goto unhandled_after_release;

unhandled_after_release:
	ept::release_page_hot_reference(*vcpu->ept_state, hooked_page_info);
	hooked_page_info = nullptr;

unhandled:
	// 孤儿权限自愈：删除/回滚残留导致页面不在 hooked_page_list、但 EPT 权限仍被限制。
	// 恢复为完全可访问的原页后让 guest 重试成功，避免同一条指令无限 violation -> 0x101。
	if (vcpu != nullptr && vcpu->ept_state != nullptr &&
		self_heal_orphaned_ept_page(*vcpu->ept_state, guest_physical_adddress))
	{
		_mm_mfence();
		invept_single_context_func(vcpu->ept_state->ept_pointer->all);
		return;
	}

	// EPT violation 可能由未跟踪页、并发清理后的权限状态或非预期访问触发。
	// 先刷新 EPT TLB,让硬件按当前页表状态重试,同时保留现场信息供后续诊断。
	if (vcpu != nullptr && vcpu->ept_state != nullptr)
	{
		// 按 GPA 页帧累计（忽略页内偏移与 qualification）。
		// 原实现要求「连续且完全相同」（GPA 与 qualification 都要逐位相等），因此只要来宾在两个
		// GPA 之间交替触发未处理 violation，计数每次都被重置为 1，永远到不了上限。
		// 此时没有任何一条指令会退休，来宾永远碰不到「可投递中断的指令边界」，
		// 该处理器就在 root/guest 之间无限往返 —— 这是 0x101 CLOCK_WATCHDOG_TIMEOUT 的一条通路。
		// 改为按页累计后，交替循环同样会被计入，最终收敛为一个带完整现场的可诊断致命错误，
		// 而不是让处理器无解释地卡死。
		const unsigned __int64 guest_physical_page = guest_physical_adddress & ~0xFFFull;
		if (vcpu->ept_state->last_unhandled_ept_violation_guest_physical == guest_physical_page)
		{
			++vcpu->ept_state->unhandled_ept_violation_count;
		}
		else
		{
			vcpu->ept_state->unhandled_ept_violation_count = 1;
		}

		vcpu->ept_state->last_unhandled_ept_violation_guest_physical = guest_physical_page;
		vcpu->ept_state->last_unhandled_ept_violation_qualification = ept_violation.all;
		vcpu->ept_state->last_unhandled_ept_violation_guest_rip = vcpu->vmexit_info.guest_rip;

		if (vcpu->ept_state->unhandled_ept_violation_count >= kMaxUnhandledEptViolationRetries)
		{
			// 孤儿自愈未命中、且同一页连续重试达到上限：注入 #GP 给 guest，
			// 让 guest 异常处理接管，避免同一条指令无限 violation -> 核困在 vmx-root -> 0x101。
			// 这是 HvVmxCore/Ophion 式「推进 guest」兜底，比 DbgBreakPoint 崩溃更温和。
			hv::inject_interruption(
				EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION,
				0,
				true);
			return;
		}
	}

	_mm_mfence();
	if (vcpu != nullptr && vcpu->ept_state != nullptr && vcpu->ept_state->ept_pointer != nullptr)
	{
		invept_single_context_func(vcpu->ept_state->ept_pointer->all);
	}
	return;

handled_return:
	if (hooked_function_info != nullptr)
	{
		ept::release_function_hot_reference(hooked_function_info);
		hooked_function_info = nullptr;
	}
	ept::release_page_hot_reference(*vcpu->ept_state, hooked_page_info);
	return;
}

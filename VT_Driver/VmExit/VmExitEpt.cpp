#include "VmExitEpt.h"
#include "../Ept/EptCommon.h"
#include "../Ept/EptBuild.h"
#include "../Cr3/Cr3Tracker.h"

namespace
{
	constexpr unsigned __int64 kMaxUnhandledEptViolationRetries = 64;
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
				Cr3TrackerObserve(hv::vmread(GUEST_CR3));
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
	// 非 RAM 区域（MMIO / 地址空洞）在 EPT 构建期刻意留空：首次访问在这里补映射。
	// 补映射成功后刷新 EPT 缓存并直接返回 —— EPT violation 的语义就是“来宾重试该指令”，
	// 不需要（也不能）在这里改写 GUEST_RIP。
	if (vcpu != nullptr && vcpu->ept_state != nullptr && vcpu->ept_state->ept_pointer != nullptr &&
		ept::ensure_mmio_mapping(*vcpu->ept_state, guest_physical_adddress))
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
			vmx_fatal_error(vmx_fatal_unhandled_ept_violation, guest_physical_adddress, ept_violation.all);
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

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
			int bpType = 0;
			if (ept::ept_handleWatchEvent(vcpu, ept_violation, hooked_page_info, guest_physical_adddress, bpType))
			{
				if (bpType == 3)
				{
					hooked_page_info->isInt3 = true;
					hooked_page_info->original_entry.execute = 1;
					if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
					{
						goto handled_return;
					}
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
						goto handled_return;
					}
					_mm_mfence();
					invept_single_context_func(vcpu->ept_state->ept_pointer->all);
				}
				goto handled_return;
			}
		}

		if (hooked_page_info->Options == EPTO_HOOK_FUNCTION)
		{
			hooked_page_info->original_entry.execute = 1;
			if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
			{
				goto handled_return;
			}
			hv::set_mtf(true);
			ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
				hooked_page_info->entry_address,
				hooked_page_info->original_entry,
				invept_type::invept_single_context);
			goto handled_return;
		}
	}
	else if (ept_violation.execute_access &&
		(ept_violation.ept_readable || ept_violation.ept_writeable))
	{
		if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
		{
			// 普通 EXECUTE watch 没有 fake page;放行当前指令并用 MTF 恢复组合权限。
			if (hooked_page_info->changed_entry.page_frame_number == 0)
			{
				int bpType = 0;
				if (ept::ept_handleWatchEvent(vcpu, ept_violation, hooked_page_info, guest_physical_adddress, bpType))
				{
					if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
					{
						goto handled_return;
					}
					_mm_mfence();
					invept_single_context_func(vcpu->ept_state->ept_pointer->all);
					goto handled_return;
				}
				goto unhandled_after_release;
			}

			Cr3TrackerObserve(hv::vmread(GUEST_CR3));
			if (ept::ept_handle_int3_execute(vcpu, hooked_page_info, guest_physical_adddress))
			{
				goto handled_return;
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
		// 只有连续相同 GPA+qualification 才累计,普通瞬时 violation 不会无限增长计数。
		if (vcpu->ept_state->last_unhandled_ept_violation_guest_physical == guest_physical_adddress &&
			vcpu->ept_state->last_unhandled_ept_violation_qualification == ept_violation.all)
		{
			++vcpu->ept_state->unhandled_ept_violation_count;
		}
		else
		{
			vcpu->ept_state->unhandled_ept_violation_count = 1;
		}

		vcpu->ept_state->last_unhandled_ept_violation_guest_physical = guest_physical_adddress;
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

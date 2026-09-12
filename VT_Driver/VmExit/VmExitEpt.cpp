#include "VmExitEpt.h"
#include "../Cr3/Cr3Tracker.h"

namespace
{
	constexpr unsigned __int64 kMaxUnhandledEptViolationRetries = 64;
}

void vmexit_ept_misconfiguration_handler(__vcpu* vcpu) // 处理 EPT 配置错误（misconfiguration）导致的 VM-exit
{
	sLog("\n"); // 日志占位，用于调试定位
	auto const phys = hv::vmread(GUEST_PHYSICAL_ADDRESS); // 读取导致错误的客户机物理地址（GPA）
	vmx_fatal_error(
		vmx_fatal_ept_misconfiguration,
		phys,
		vcpu != nullptr ? vcpu->vmexit_info.qualification : 0);
}

//处理 EPT Violation 退出：根据读/写/执行违规类型处理 EPT 断点/函数钩子，并同步修改后的 EPT 条目
void vmexit_ept_violation_handler(__vcpu* vcpu) // vcpu=当前虚拟CPU状态
{
	__ept_violation ept_violation; // 原理：EPT 违规信息保存在 VM-exit qualification；作用：解析访问类型/权限；结果：ept_violation 字段可用

	ept_violation.all = vcpu->vmexit_info.qualification; // 原理：将 qualification 原值写入位域结构；作用：解析读/写/执行违规；结果：ept_violation 填充
	//unsigned __int64 guest_physical_adddress = hv::vmread(ept_violation.caused_by_translation ? GUEST_PHYSICAL_ADDRESS : GUEST_LINEAR_ADDRESS);// 原理：EPT 违规可能由地址转换触发；作用：读取 GPA 或 GLA；结果：得到触发违规的地址
	unsigned __int64 guest_physical_adddress = hv::vmread(GUEST_PHYSICAL_ADDRESS); // 固定读取GPA

	//触发EPT违例后 枚举hook列表 判断是否是由我们设置的hook页引发的
	PLIST_ENTRY current = &vcpu->ept_state->hooked_page_list; // 原理：维护已钩住页面链表；作用：从链表头开始遍历；结果：current 指向链表
	while (&vcpu->ept_state->hooked_page_list != current->Flink) // 原理：遍历所有 EPT 钩子页面；作用：寻找命中的钩子；结果：逐个检查
	{
		current = current->Flink;
		__ept_hooked_page_info* hooked_page_info = CONTAINING_RECORD(current, __ept_hooked_page_info, hooked_page_list); // 原理：由链表节点反推出钩子结构体指针；作用：得到 hooked_page_info；结果：可访问钩子参数

		//guest里的物理地址不会改变，还是指向原来的GPA
		// 原理：按页帧号匹配被钩住的物理页；作用：判断是否命中当前违规页；结果：命中则进入处理
		if (hooked_page_info->pfn_of_hooked_page == GET_PFN(guest_physical_adddress)) // 原理：EPT 违规可能由地址转换触发；作用：读取 GPA 或 GLA；结果：得到触发违规的地址
		{
			if ((ept_violation.read_access || ept_violation.write_access) && (!ept_violation.ept_readable || !ept_violation.ept_writeable)) // 原理：读/写违规表示权限不足；作用：区分断点/钩子处理路径；结果：选择相应分支
			{
				if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
				{
					//处理断点监视事件的
					int bpType = 0; // 原理：bpType 表示断点类型；作用：区分 INT3 等类型；结果：决定是否设置 MTF
					if (ept::ept_handleWatchEvent(vcpu, ept_violation, hooked_page_info, guest_physical_adddress, bpType)) // 原理：EPT 违规可能由地址转换触发；作用：读取 GPA 或 GLA；结果：得到触发违规的地址
					{
						if (bpType == 3) // 原理：bpType 表示断点类型；作用：区分 INT3 等类型；结果：决定是否设置 MTF
						{
							hooked_page_info->isInt3 = true; // 原理：记录当前为 INT3 断点；作用：后续处理区分断点类型；结果：状态标记更新
							hooked_page_info->original_entry.execute = 1; // 原理：临时允许执行以完成断点处理；作用：放行一次执行；结果：原页可执行
							//vcpu->ept_state->page_to_change = hooked_page_info; // 原理：记录待恢复/切换的页；作用：供 MTF 或后续逻辑使用；结果：状态保存 // old direct pending-MTF assignment kept for audit
							if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
							{
								return;
							}
							hv::set_mtf(true);
							ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
								hooked_page_info->entry_address,
								hooked_page_info->original_entry,
								invept_type::invept_single_context);
						}
						else
						{
							//vcpu->ept_state->page_to_change = hooked_page_info; // 原理：记录待恢复/切换的页；作用：供 MTF 或后续逻辑使用；结果：状态保存 // old direct pending-MTF assignment kept for audit
							if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
							{
								return;
							}
							//刷新当前逻辑处理器的eptp
							_mm_mfence(); // 原理：内存栅栏确保页表修改先于后续访问；作用：避免乱序导致权限不一致；结果：修改有序生效
							invept_single_context_func(vcpu->ept_state->ept_pointer->all); // 原理：刷新当前 EPT 上下文的 TLB 缓存；作用：让新 PML1 条目生效；结果：旧缓存失效
						}
						return;
					}
				}

				if (hooked_page_info->Options == EPTO_HOOK_FUNCTION) // 原理：函数钩子需要在执行/读写时切换页权限；作用：区分钩子类型；结果：进入函数钩子处理
				{
					hooked_page_info->original_entry.execute = 1;
					//vcpu->ept_state->page_to_change = hooked_page_info; // 原理：记录待恢复/切换的页；作用：供 MTF 或后续逻辑使用；结果：状态保存 // old direct pending-MTF assignment kept for audit
					if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
					{
						return;
					}
					hv::set_mtf(true);
					ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state,
						hooked_page_info->entry_address,
						hooked_page_info->original_entry,
						invept_type::invept_single_context);
					return;
				}
			}
			else if (ept_violation.execute_access && (ept_violation.ept_readable || ept_violation.ept_writeable)) // 原理：执行违规表示尝试执行不可执行页；作用：处理执行型钩子/断点；结果：切换到可执行条目
			{
				if (hooked_page_info->Options == EPTO_VIRTUAL_BREAKPOINT)
				{
					// 普通 EXECUTE watch 没有 fake page；放行当前指令并用 MTF 恢复组合权限。
					if (hooked_page_info->changed_entry.page_frame_number == 0)
					{
						int bpType = 0;
						if (ept::ept_handleWatchEvent(
							vcpu,
							ept_violation,
							hooked_page_info,
							guest_physical_adddress,
							bpType))
						{
							if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
							{
								return;
							}
							_mm_mfence();
							invept_single_context_func(vcpu->ept_state->ept_pointer->all);
							return;
						}
						break;
					}

					Cr3TrackerObserve(hv::vmread(GUEST_CR3));
					if (ept::ept_handle_int3_execute(
						vcpu,
						hooked_page_info,
						guest_physical_adddress))
					{
						return;
					}
					break;
				}

				if (hooked_page_info->Options == EPTO_HOOK_FUNCTION) // 原理：函数钩子需要在执行/读写时切换页权限；作用：区分钩子类型；结果：进入函数钩子处理
				{
					//交换物理页面并刷新tlb
					ept::swap_pml1_and_invalidate_tlb(*vcpu->ept_state, // 原理：交换 EPT PML1 条目并失效 TLB；作用：让权限变更立即生效；结果：访问规则更新
						hooked_page_info->entry_address,
						hooked_page_info->changed_entry,
						invept_type::invept_single_context);
					return;
				}
			}

			break;
		}
	}

	// EPT violation 可能由未跟踪页、并发清理后的权限状态或非预期访问触发。
	// 无条件 BugCheck 会把一次可恢复的权限状态变化变成整机故障;先刷新 EPT TLB,
	// 让硬件按当前页表状态重试,同时保留现场信息供后续诊断。
	if (vcpu != nullptr && vcpu->ept_state != nullptr)
	{
		// Only count consecutive violations for the same GPA+qualification. This
		// bounds a stuck EPT state without treating ordinary transient violations
		// accumulated over hours as a fatal problem.
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
		vcpu->ept_state->last_unhandled_ept_violation_guest_rip =
			vcpu->vmexit_info.guest_rip;

		if (vcpu->ept_state->unhandled_ept_violation_count >= kMaxUnhandledEptViolationRetries)
		{
			vmx_fatal_error(
				vmx_fatal_unhandled_ept_violation,
				guest_physical_adddress,
				ept_violation.all);
		}
	}

	_mm_mfence();
	if (vcpu != nullptr &&
		vcpu->ept_state != nullptr &&
		vcpu->ept_state->ept_pointer != nullptr)
	{
		invept_single_context_func(vcpu->ept_state->ept_pointer->all);
	}
}

#include "Driver.h"
#include "poolmanager.h"
#include "Globals.h"
#include "vmcall_handler.h"
#include "interrupt.h"
#include "vmcall_reason.h" // 引入 VMCALL 命令编号：告诉 Hypervisor 执行哪一种具体操作
#include "invalid_ept.h" // INVEPT 描述符与接口声明
#include "msr.h"
#include "mtrr.h" // MTRR 相关结构
#include "EPT.h"
#include "AsmCallset.h"
#include "hypervisor_routines.h"
#include "vmexit_handler.h"
#include "Vmcs/VmcsApi.h"

namespace
{
	bool IsVmcallContextValid(__vcpu* vcpu)
	{
		return vcpu != nullptr && vcpu->vmexit_info.guest_registers != nullptr;
	}

	bool IsCurrentEptStateValid(__vcpu* vcpu, bool require_ept_pointer)
	{
		if (vcpu == nullptr || vcpu->ept_state == nullptr)
		{
			return false;
		}

		return !require_ept_pointer || vcpu->ept_state->ept_pointer != nullptr;
	}

	bool AreAllEptStatesValid(bool require_ept_pointer)
	{
		if (g_vmm_context.vcpu == nullptr || g_vmm_context.processor_count == 0)
		{
			return false;
		}

		for (unsigned __int32 i = 0; i < g_vmm_context.processor_count; ++i)
		{
			if (g_vmm_context.vcpu[i].ept_state == nullptr)
			{
				return false;
			}

			if (require_ept_pointer && g_vmm_context.vcpu[i].ept_state->ept_pointer == nullptr)
			{
				return false;
			}
		}

		return true;
	}

	bool IsSupportedVmcallReason(unsigned __int64 vmcall_reason)
	{
		return vmcall_reason <= static_cast<unsigned __int64>(VMCALL_CLEAR_BREAKPOINT_STATE);
	}

	bool IsProjectVmcall(guest_context const* guest_registers)
	{
		return guest_registers != nullptr &&
			(guest_registers->rax == VMCALL_IDENTIFIER ||
				guest_registers->eax == VMCALL_IDENTIFIER2);
	}

	void InjectUndefinedOpcode()
	{
		hv::inject_interruption(EXCEPTION_VECTOR_UNDEFINED_OPCODE, INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false);
	}

	void CompleteVmcall(__vcpu* vcpu, bool status)
	{
		adjust_rip(vcpu);
		vcpu->vmexit_info.guest_registers->rax = status;
	}

	bool AreVmcallPreconditionsValid(__vcpu* vcpu,
		unsigned __int64 vmcall_reason,
		unsigned __int64 vmcall_parameter1,
		unsigned __int64 vmcall_parameter2)
	{
		switch (vmcall_reason) //什么原因调用vmcall
		{
		case VMCALL_EPT_RIP_HOOK:
			return IsCurrentEptStateValid(vcpu, true) &&
				vmcall_parameter1 != 0 &&
				vmcall_parameter2 != 0;

		case VMCALL_EPT_UNHOOK_FUNCTION: //关闭ept hook
			return IsCurrentEptStateValid(vcpu, true) &&
				(vmcall_parameter1 != 0 || vmcall_parameter2 != 0);

		case VMCALL_INVEPT_CONTEXT: //刷新ept缓存
			return vmcall_parameter1 != 0 || IsCurrentEptStateValid(vcpu, true);

		case VMCALL_READ_SOFTWARE_BREAKPOINT:
			return IsCurrentEptStateValid(vcpu, true) && vmcall_parameter1 != 0;

		case VMCALL_HIDE_SOFTWARE_BREAKPOINT:
		case VMCALL_WATCH_WRITES:
		case VMCALL_WATCH_READS:
		case VMCALL_WATCH_EXECUTES:
		case VMCALL_WATCH_DELETE:
			return AreAllEptStatesValid(true) && vmcall_parameter1 != 0;

		case VMCALL_GET_BREAKPOINT:
		case VMCALL_INIT_OFFSET:
			return vmcall_parameter1 != 0;

		case VMCALL_CLEAR_BREAKPOINT_STATE:
			return true;

		default:
			return true;
		}
	}
}

void restore_segment_registers()
{
	__writemsr(IA32_FS_BASE, hv::vmread(GUEST_FS_BASE));
	__writemsr(IA32_GS_BASE, hv::vmread(GUEST_GS_BASE));
	//恢复gdtr和idtr寄存器的值
	__reload_gdtr(hv::vmread(GUEST_GDTR_BASE), hv::vmread(GUEST_GDTR_LIMIT)); // 恢复 GDTR 基址与界限，保证分段描述符表正确
	__reload_idtr(hv::vmread(GUEST_IDTR_BASE), hv::vmread(GUEST_IDTR_LIMIT)); // 恢复 IDTR 基址与界限，保证中断描述符表正确
}

void call_vmxoff(__vcpu* vcpu)
{
	// 1. 推进 RIP 并立即写回 VMCS
	//uint64_t new_rip = vcpu->vmexit_info.guest_rip + vcpu->vmexit_info.instruction_length;
	//hv::vmwrite(GUEST_RIP, new_rip);

	//// 2. 保存状态供卸载时使用
	//vcpu->vmx_off_state.guest_rip = new_rip;
	//vcpu->vmx_off_state.guest_rsp = vcpu->vmexit_info.guest_registers->rsp;
	//vcpu->vmx_off_state.vmx_off_executed = true;


	//vcpu->vmx_off_state.guest_rip = vcpu->vmexit_info.guest_rip + vcpu->vmexit_info.instruction_length;
	//vcpu->vmx_off_state.guest_rsp = vcpu->vmexit_info.guest_registers->rsp;
	vcpu->vmx_off_state.vmx_off_executed = true;
}

void vmexit_vmcall_handler(__vcpu* vcpu)
{
	bool status = true;
	unsigned __int64 vmcall_reason = 0;
	unsigned __int64 vmcall_parameter1 = 0;
	unsigned __int64 vmcall_parameter2 = 0;
	unsigned __int64 vmcall_parameter3 = 0;
	unsigned __int64 vmcall_parameter4 = 0;
	unsigned __int64 vmcall_parameter5 = 0;
	unsigned __int64 vmcall_parameter6 = 0;
	unsigned __int64 vmcall_parameter7 = 0;
	unsigned __int64 vmcall_parameter8 = 0;
	unsigned __int64 vmcall_parameter9 = 0;

	if (vcpu == nullptr)
	{
		return;
	}

	if (!IsVmcallContextValid(vcpu))
	{
		InjectUndefinedOpcode();
		return;
	}

	//
	// Check if this vmcall belongs to us
	// 检查此vmcall是否属于我们调用的
	//
	if (!IsProjectVmcall(vcpu->vmexit_info.guest_registers))
	{
		if (IsCurrentEptStateValid(vcpu, false) && ept::handler_vmcall_rip(*vcpu->ept_state))
		{
			return;  //不去更改rax寄存器
		}

		InjectUndefinedOpcode();
		return;
	}

	//只允许ring0程序调用
	//if (hv::get_guest_cpl() != 0)
	//{
	//	//向guest机注入#GP异常
	//	hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT, INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, 1);
	//	return;
	//}



	if (vcpu->vmexit_info.guest_registers->eax == VMCALL_IDENTIFIER2)
	{
		vmcall_reason = vcpu->vmexit_info.guest_registers->ecx;
		vmcall_parameter1 = vcpu->vmexit_info.guest_registers->edx;
	}
	else
	{
		vmcall_reason = vcpu->vmexit_info.guest_registers->rcx;
		vmcall_parameter1 = vcpu->vmexit_info.guest_registers->rdx;
		vmcall_parameter2 = vcpu->vmexit_info.guest_registers->r8;
		vmcall_parameter3 = vcpu->vmexit_info.guest_registers->r9;

		//
		// These only if __vmcall_ex was called
		//
		vmcall_parameter4 = vcpu->vmexit_info.guest_registers->r10;
		vmcall_parameter5 = vcpu->vmexit_info.guest_registers->r11;
		vmcall_parameter6 = vcpu->vmexit_info.guest_registers->r12;
		vmcall_parameter7 = vcpu->vmexit_info.guest_registers->r13;
		vmcall_parameter8 = vcpu->vmexit_info.guest_registers->r14;
		vmcall_parameter9 = vcpu->vmexit_info.guest_registers->r15;
	}

	if (!IsSupportedVmcallReason(vmcall_reason) ||
		!AreVmcallPreconditionsValid(vcpu, vmcall_reason, vmcall_parameter1, vmcall_parameter2))
	{
		CompleteVmcall(vcpu, false);
		return;
	}

	switch (vmcall_reason)  //什么原因调用vmcall
	{
	case VMCALL_TEST:
	{
		break;
	}
	case VMCALL_VMXOFF:
	{
		call_vmxoff(vcpu);  //退出vmx模式
		break;
	}
	case VMCALL_EPT_CC_HOOK:
	{
		// 旧项目保留的禁用路径：进入 EPT hook 前按 vmcall_parameter4 临时切换 CR3，调用 cc_hook_function 后再恢复。
		// 为什么注释：当前重构只放开 VMCALL_EPT_RIP_HOOK 这一条完整维护的路径，CC/INT1/通用 hook 分支没有接入
		// 现有 pending MTF 元数据、watchid 生命周期和参数前置校验。
		// 放开的好处：能恢复旧版通过 0xCC 或 INT1 类型 EPT hook 的能力。
		// 放开的坏处：旧代码直接切换地址空间，失败时可能遗留 target_cr3/MTF 状态；同时这些分支返回结构与当前 IOCTL/Hook
		// 层使用的 BREAKPOINT_RECORD 不一致。
		// 项目影响：会影响 VM-exit EPT violation、MTF 恢复和调试断点链路，属于高回归风险内核路径。
		// 结论：继续注释；要恢复只能按 VMCALL_EPT_RIP_HOOK 的参数校验和状态清理方式重做。
		//unsigned __int64 old_cr3 = hv::swap_context(vmcall_parameter4);

		//status = ept::cc_hook_function(*vcpu->ept_state, (void*)vmcall_parameter1, (void*)vmcall_parameter2, (void**)vmcall_parameter3);

		//hv::restore_context(old_cr3);

		status = false;
		break;
	}
	case VMCALL_EPT_INT1_HOOK:
	{
		//unsigned __int64 old_cr3 = hv::swap_context(vmcall_parameter4);

		//status = ept::int1_hook_function(*vcpu->ept_state, (void*)vmcall_parameter1, (void*)vmcall_parameter2, (void**)vmcall_parameter3);

		//hv::restore_context(old_cr3);

		status = false;
		break;
	}
	case VMCALL_EPT_RIP_HOOK:
	{
		spinlock::lock(&eptWatchTransaction_lock);
		status = ept::vmcall_hook_function(*vcpu->ept_state, 
			(void*)vmcall_parameter1, 
			(void*)vmcall_parameter2, 
			(void**)vmcall_parameter3,
			vmcall_parameter4);
		spinlock::unlock(&eptWatchTransaction_lock);

		break;
	}
	case VMCALL_EPT_HOOK_FUNCTION:  //执行ept hook
	{
		// 旧通用 hook 路径尚未按 RIP_HOOK 的校验/回滚语义重做。
		// 这里不能先真正挂钩再把 status 改成 false，否则会留下半安装状态。
		status = false;
		break;
	}
	case VMCALL_EPT_UNHOOK_FUNCTION:  //关闭ept hook
	{
		spinlock::lock(&eptWatchTransaction_lock);
		// 旧项目保留的禁用路径：unhook 前后通过 vmcall_parameter3 临时切换 CR3。
		// 为什么注释：当前 unhook 只依赖物理页和当前 EPT 状态，不需要切换来宾地址空间；切换 CR3 反而会让错误恢复更复杂。
		// 放开的好处：旧版按来宾 CR3 解释虚拟地址的 unhook 模式可复用。
		// 放开的坏处：当前分支已经接受物理页参数，恢复旧 CR3 切换没有收益，并可能污染后续 VM-exit 的目标 CR3。
		// 项目影响：全量 unhook 时还需要清理 pending MTF 元数据，旧路径没有覆盖这类重构后状态。
		// 结论：继续注释。
		//unsigned __int64 old_cr3 = hv::swap_context(vmcall_parameter3);

		// If set unhook all pages
		if (vmcall_parameter1 != 0)
		{
			hv::clear_pending_mtf_metadata(vcpu);
			ept::unhook_all_functions(*vcpu->ept_state);
		}

		else
		{
			// Page physciall address
			status = ept::unhook_function(*vcpu->ept_state, vmcall_parameter2);
		}
		spinlock::unlock(&eptWatchTransaction_lock);

		//hv::restore_context(old_cr3);

		break;
	}
	case VMCALL_INVEPT_CONTEXT:  //刷新ept缓存
	{
		// If set invept all contexts
		if (vmcall_parameter1 != 0)
		{
			_mm_mfence();
			invept_all_contexts_func();
		}
		else
		{
			_mm_mfence();
			invept_single_context_func((*vcpu->ept_state).ept_pointer->all);
		}

		break;
	}
	case VMCALL_DUMP_POOL_MANAGER:
	{
		pool_manager::dump_pools_info();
		break;
	}
	case VMCALL_DUMP_VMCS_STATE:
	{
		hv::dump_vmcs();
		break;
	}
	case VMCALL_HIDE_HV_PRESENCE:
	{
		g_vmm_context.hv_presence = false;
		break;
	}
	case VMCALL_UNHIDE_HV_PRESENCE:
	{
		g_vmm_context.hv_presence = true; // 标记VMM已存在，便于其他逻辑快速判断
		break;
	}
	case VMCALL_HIDE_SOFTWARE_BREAKPOINT:
	{
		status = ept::set_hide_software_breakpoint((PVT_BREAK_POINT)vmcall_parameter1);
		break;
	}
	case VMCALL_READ_SOFTWARE_BREAKPOINT:
	{
		status = ept::get_hide_software_breakpoint(*vcpu->ept_state, (PVT_BREAK_POINT)vmcall_parameter1);
		break;
	}
	case VMCALL_READ_EPT_FAKE_PAGE_MEMORY:
	{
		// 旧项目保留的禁用路径：临时切换 CR3 后直接读取 EPT 伪造页内存。
		// 为什么注释：当前读取软件断点隐藏内容走 get_hide_software_breakpoint(PVT_BREAK_POINT)，结构体里包含 CR3、
		// 虚拟地址、大小和输出缓冲，边界比裸参数更明确。
		// 放开的好处：可以按旧调用约定读取任意 fake page 片段，便于底层调试。
		// 放开的坏处：缺少结构化大小校验和调用者身份约束，容易造成跨地址空间读错或读越界。
		// 项目影响：属于内核读来宾内存路径，打开会扩大攻击面并增加系统崩溃风险。
		// 结论：继续注释，必要时新建受控 IOCTL/结构体接口。
		//unsigned __int64 old_cr3 = hv::swap_context(vmcall_parameter4);

		//status = ept::get_hide_software_breakpoint(*vcpu->ept_state, (void*)vmcall_parameter1, (void*)vmcall_parameter2, vmcall_parameter3);

		//hv::restore_context(old_cr3);

		status = false;
		break;
	}
	case VMCALL_WATCH_WRITES:
	{
		status = SetBreakpoint((PVT_BREAK_POINT)vmcall_parameter1, EPTW_WRITE);
		break;
	}
	case VMCALL_WATCH_READS:
	{
		status = SetBreakpoint((PVT_BREAK_POINT)vmcall_parameter1, EPTW_READWRITE);
		break;
	}
	case VMCALL_WATCH_EXECUTES:
	{
		status = SetBreakpoint((PVT_BREAK_POINT)vmcall_parameter1, EPTW_EXECUTE);
		break;
	}
	case VMCALL_WATCH_DELETE:
	{
		status = RemoveBreakpoint((PVT_BREAK_POINT)vmcall_parameter1);
		break;
	}
	case VMCALL_GET_BREAKPOINT:
	{
		status = hv::get_breakpoint_detected(vcpu, (PBREAKPOINT_DETECTED)vmcall_parameter1);
		break;
	}
	case VMCALL_INIT_OFFSET:
	{
		//status = InitOffset((PWINDOWS_STRUCT)vmcall_parameter1);
		//adjust_rip(vcpu);
		//break;

        // vmcall_parameter1 = rdx，指向来宾里的 WINDOWS_STRUCT
	    // vmcall_parameter2 = r8，本次承载 DbgkSysWin10 转发下来的 verifyKey
		status = InitOffset((PWINDOWS_STRUCT)vmcall_parameter1, vmcall_parameter2);
		//sLog("[+] 是否 VMCALL_INIT_OFFSET: %d\n", status); //
		break;
	}
	case VMCALL_CLEAR_BREAKPOINT_STATE:
	{
		status = hv::clear_breakpoint_state(vcpu, (void*)vmcall_parameter1);
		break;
	}
	default:
	{
		status = false;
		break;
	}
	}

	CompleteVmcall(vcpu, status);
}

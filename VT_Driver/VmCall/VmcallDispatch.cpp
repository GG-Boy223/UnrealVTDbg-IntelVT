#include "Driver.h"
#include "poolmanager.h"
#include "Globals.h"
#include "vmcall_reason.h"
#include "invalid_ept.h"
#include "EPT.h"
#include "hypervisor_routines.h"
#include "vmexit_handler.h"
#include "VmcallDispatch.h"
#include "../Cr3/Cr3Tracker.h"
#include "../Ept/EptCommon.h"

namespace
{
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

	bool ArePreconditionsValid(__vcpu* vcpu, VmcallDispatch::Request const& request)
	{
		switch (request.reason)
		{
		case VMCALL_EPT_RIP_HOOK:
			return IsCurrentEptStateValid(vcpu, true) &&
				request.parameter1 != 0 &&
				request.parameter2 != 0;

		case VMCALL_EPT_UNHOOK_FUNCTION:
			return IsCurrentEptStateValid(vcpu, true) &&
				(request.parameter1 != 0 || request.parameter2 != 0);

		case VMCALL_INVEPT_CONTEXT:
			return request.parameter1 != 0 || IsCurrentEptStateValid(vcpu, true);

		case VMCALL_READ_SOFTWARE_BREAKPOINT:
			return IsCurrentEptStateValid(vcpu, true) && request.parameter1 != 0;

		case VMCALL_HIDE_SOFTWARE_BREAKPOINT:
		case VMCALL_WATCH_WRITES:
		case VMCALL_WATCH_READS:
		case VMCALL_WATCH_EXECUTES:
		case VMCALL_WATCH_DELETE:
			return AreAllEptStatesValid(true) && request.parameter1 != 0;

		case VMCALL_GET_BREAKPOINT:
		case VMCALL_PEEK_BREAKPOINT:
		case VMCALL_INIT_OFFSET:
			return request.parameter1 != 0;

		default:
			return true;
		}
	}

	void RequestVmxoff(__vcpu* vcpu)
	{
		vcpu->vmx_off_state.vmx_off_completed = false;
		vcpu->vmx_off_state.vmx_off_executed = true;
	}

	bool ExecuteEptRipHook(__vcpu* vcpu, VmcallDispatch::Request const& request)
	{
		bool const status = ept::vmcall_hook_function(*vcpu->ept_state,
			(void*)request.parameter1,
			(void*)request.parameter2,
			(void**)request.parameter3,
			request.parameter4);
		return status;
	}

	bool ExecuteEptUnhook(__vcpu* vcpu, VmcallDispatch::Request const& request)
	{
		bool status = true;

		if (request.parameter1 != 0)
		{
			hv::clear_pending_mtf_metadata(vcpu);
			ept::unhook_all_functions(*vcpu->ept_state);
		}
		else
		{
			status = ept::unhook_function(*vcpu->ept_state, request.parameter2);
		}

		return status;
	}

	bool ExecuteEptFunctionHook(__vcpu* vcpu, VmcallDispatch::Request const& request)
	{
		bool status = false;
		switch (request.reason)
		{
		case VMCALL_EPT_CC_HOOK:
			status = ept::cc_hook_function(
				*vcpu->ept_state,
				(void*)request.parameter1,
				(void*)request.parameter2,
				(void**)request.parameter3);
			break;
		case VMCALL_EPT_INT1_HOOK:
			status = ept::int1_hook_function(
				*vcpu->ept_state,
				(void*)request.parameter1,
				(void*)request.parameter2,
				(void**)request.parameter3);
			break;
		case VMCALL_EPT_HOOK_FUNCTION:
			status = ept::hook_function(
				*vcpu->ept_state,
				(void*)request.parameter1,
				(void*)request.parameter2,
				(void**)request.parameter3);
			break;
		default:
			break;
		}
		return status;
	}

	bool ExecuteReadEptFakePageMemory(__vcpu* vcpu, VmcallDispatch::Request const& request)
	{
		if (vcpu == nullptr ||
			vcpu->ept_state == nullptr ||
			request.parameter1 == 0 ||
			request.parameter2 == 0 ||
			request.parameter3 == 0 ||
			request.parameter3 > ept::kEptPageSize)
		{
			return false;
		}

		const unsigned __int64 physical_address =
			hv::get_physical_address(request.parameter4, (PVOID)request.parameter1);
		if (physical_address == 0)
		{
			return false;
		}

		const unsigned __int64 page_offset = request.parameter1 & 0xFFF;
		const bool range_valid =
			page_offset < ept::kEptPageSize &&
			request.parameter3 <= ept::kEptPageSize - page_offset;
		if (!range_valid)
		{
			return false;
		}

		unsigned char local_buffer[ept::kEptPageSize];

		cr3 guest_cr3;
		guest_cr3.flags = request.parameter4;

		// HvVmxCore 式：只读当前核自己的 EPT 状态，靠 guest 侧逐核广播覆盖所有核。
		__ept_hooked_page_info* hooked_page_info =
			ept::acquire_any_page_hot_reference(
				*vcpu->ept_state,
				GET_PFN(physical_address));
		if (hooked_page_info == nullptr)
		{
			return false;
		}

		RtlCopyMemory(
			local_buffer,
			&hooked_page_info->fake_page_contents[page_offset],
			static_cast<SIZE_T>(request.parameter3));
		ept::release_page_hot_reference(*vcpu->ept_state, hooked_page_info);

		return request.parameter3 == hv::write_guest_virtual_memory(
			guest_cr3,
			(PVOID)request.parameter2,
			local_buffer,
			static_cast<SIZE_T>(request.parameter3));
	}

	bool ExecuteSoftwareBreakpointBuffer(__vcpu* vcpu, VmcallDispatch::Request const& request)
	{
		if (vcpu == nullptr ||
			vcpu->ept_state == nullptr ||
			request.parameter1 == 0 ||
			request.parameter2 == 0 ||
			request.parameter3 == 0 ||
			request.parameter3 > ept::kEptPageSize ||
			request.parameter4 == 0)
		{
			return false;
		}

		// HvVmxCore 式：只改当前核自己的 fake page 内容，靠 guest 侧逐核广播覆盖所有核。
		if (request.reason == VMCALL_SET_SOFTWARE_BREAKPOINT_BUFFER)
		{
			return ept::set_software_breakpoint_buffer(
				*vcpu->ept_state,
				request.parameter1,
				(PVOID)request.parameter2,
				request.parameter3,
				request.parameter4);
		}
		else if (request.reason == VMCALL_GET_SOFTWARE_BREAKPOINT_BUFFER)
		{
			return ept::get_software_breakpoint_buffer(
				*vcpu->ept_state,
				request.parameter1,
				(PVOID)request.parameter2,
				request.parameter3,
				request.parameter4);
		}
		return false;
	}

	// 读取隐形软件断点：get_hide_software_breakpoint 通过页面热引用固定记录生命周期。
	bool ExecuteReadSoftwareBreakpoint(__vcpu* vcpu, VmcallDispatch::Request const& request)
	{
		const bool status = ept::get_hide_software_breakpoint(
			*vcpu->ept_state,
			(PVT_BREAK_POINT)request.parameter1);
		return status;
	}

	bool ExecuteInvept(__vcpu* vcpu, VmcallDispatch::Request const& request)
	{
		_mm_mfence();
		if (request.parameter1 != 0)
		{
			invept_all_contexts_func();
		}
		else
		{
			invept_single_context_func(vcpu->ept_state->ept_pointer->all);
		}

		return true;
	}
}

namespace VmcallDispatch
{
	bool Execute(__vcpu* vcpu, Request const& request)
	{
		if (vcpu == nullptr ||
			request.reason > static_cast<unsigned __int64>(VMCALL_REASON_MAX) ||
			!ArePreconditionsValid(vcpu, request))
		{
			return false;
		}

		switch (request.reason)
		{
		case VMCALL_TEST:
			return true;

		case VMCALL_VMXOFF:
			RequestVmxoff(vcpu);
			return true;

		case VMCALL_EPT_CC_HOOK:
		case VMCALL_EPT_INT1_HOOK:
		case VMCALL_EPT_HOOK_FUNCTION:
			return ExecuteEptFunctionHook(vcpu, request);

		case VMCALL_READ_EPT_FAKE_PAGE_MEMORY:
			return ExecuteReadEptFakePageMemory(vcpu, request);

		case VMCALL_SET_SOFTWARE_BREAKPOINT_BUFFER:
		case VMCALL_GET_SOFTWARE_BREAKPOINT_BUFFER:
			return ExecuteSoftwareBreakpointBuffer(vcpu, request);

		case VMCALL_EPT_RIP_HOOK:
			return ExecuteEptRipHook(vcpu, request);

		case VMCALL_EPT_UNHOOK_FUNCTION:
			return ExecuteEptUnhook(vcpu, request);

		case VMCALL_INVEPT_CONTEXT:
			return ExecuteInvept(vcpu, request);

		case VMCALL_DUMP_POOL_MANAGER:
			pool_manager::dump_pools_info();
			return true;

		case VMCALL_DUMP_VMCS_STATE:
			hv::dump_vmcs();
			return true;

		case VMCALL_HIDE_HV_PRESENCE:
			g_vmm_context.hv_presence = false;
			return true;

		case VMCALL_UNHIDE_HV_PRESENCE:
			g_vmm_context.hv_presence = true;
			return true;

		case VMCALL_HIDE_SOFTWARE_BREAKPOINT:
			return ept::set_hide_software_breakpoint(vcpu, (PVT_BREAK_POINT)request.parameter1);

		case VMCALL_READ_SOFTWARE_BREAKPOINT:
			return ExecuteReadSoftwareBreakpoint(vcpu, request);

		case VMCALL_WATCH_WRITES:
			return SetBreakpoint(vcpu, (PVT_BREAK_POINT)request.parameter1, EPTW_WRITE);

		case VMCALL_WATCH_READS:
			return SetBreakpoint(vcpu, (PVT_BREAK_POINT)request.parameter1, EPTW_READWRITE);

		case VMCALL_WATCH_EXECUTES:
			return SetBreakpoint(vcpu, (PVT_BREAK_POINT)request.parameter1, EPTW_EXECUTE);

		case VMCALL_WATCH_DELETE:
			return RemoveBreakpoint(vcpu, (PVT_BREAK_POINT)request.parameter1);

		case VMCALL_GET_BREAKPOINT:
			return hv::get_breakpoint_detected(
				vcpu,
				(PBREAKPOINT_DETECTED)request.parameter1);

		case VMCALL_PEEK_BREAKPOINT:
			return hv::peek_breakpoint_detected(
				vcpu,
				(PBREAKPOINT_DETECTED)request.parameter1);

		case VMCALL_INIT_OFFSET:
			return InitOffset((PWINDOWS_STRUCT)request.parameter1, request.parameter2);

		case VMCALL_CLEAR_BREAKPOINT_STATE:
			return hv::clear_breakpoint_state(vcpu, (void*)request.parameter1);

		case VMCALL_SYNC_CR3_EXIT_POLICY:
			return Cr3TrackerSynchronizeExitPolicy(vcpu);

		default:
			return false;
		}
	}
}

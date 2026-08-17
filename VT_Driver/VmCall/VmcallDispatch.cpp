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
		spinlock::lock(&eptWatchTransaction_lock);
		bool const status = ept::vmcall_hook_function(*vcpu->ept_state,
			(void*)request.parameter1,
			(void*)request.parameter2,
			(void**)request.parameter3,
			request.parameter4);
		spinlock::unlock(&eptWatchTransaction_lock);
		return status;
	}

	bool ExecuteEptUnhook(__vcpu* vcpu, VmcallDispatch::Request const& request)
	{
		bool status = true;

		spinlock::lock(&eptWatchTransaction_lock);
		if (request.parameter1 != 0)
		{
			hv::clear_pending_mtf_metadata(vcpu);
			ept::unhook_all_functions(*vcpu->ept_state);
		}
		else
		{
			status = ept::unhook_function(*vcpu->ept_state, request.parameter2);
		}
		spinlock::unlock(&eptWatchTransaction_lock);

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
		case VMCALL_READ_EPT_FAKE_PAGE_MEMORY:
			return false;

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
			return ept::set_hide_software_breakpoint((PVT_BREAK_POINT)request.parameter1);

		case VMCALL_READ_SOFTWARE_BREAKPOINT:
			return ept::get_hide_software_breakpoint(
				*vcpu->ept_state,
				(PVT_BREAK_POINT)request.parameter1);

		case VMCALL_WATCH_WRITES:
			return SetBreakpoint((PVT_BREAK_POINT)request.parameter1, EPTW_WRITE);

		case VMCALL_WATCH_READS:
			return SetBreakpoint((PVT_BREAK_POINT)request.parameter1, EPTW_READWRITE);

		case VMCALL_WATCH_EXECUTES:
			return SetBreakpoint((PVT_BREAK_POINT)request.parameter1, EPTW_EXECUTE);

		case VMCALL_WATCH_DELETE:
			return RemoveBreakpoint((PVT_BREAK_POINT)request.parameter1);

		case VMCALL_GET_BREAKPOINT:
			return hv::get_breakpoint_detected(
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

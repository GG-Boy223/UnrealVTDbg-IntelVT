#include "../Driver.h"
#include "AsmCallset.h"
#include "vmcall_reason.h"
#include "../ntapi.h"
#include "../Log/log.h"
#include "../ntos/inc/ntlpcapi.h"
#include "../ntos/inc/ntdbg.h"
#include "../List/MyList.h"
#include "../Globals.h"
#include "HypervisorGatewayInternal.h"
#include "../../Common/Ring0/Hvm/VmcallDispatchPolicy.h"

namespace hvgt
{
	void broadcast_vmoff(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		UNREFERENCED_PARAMETER(Dpc);
		UNREFERENCED_PARAMETER(DeferredContext);

		SafeVmCall(VMCALL_VMXOFF, 0, 0, 0);
		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_invept_all_contexts(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		UNREFERENCED_PARAMETER(Dpc);
		UNREFERENCED_PARAMETER(DeferredContext);

		SafeVmCall(VMCALL_INVEPT_CONTEXT, true, 0, 0);
		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_invept_single_context(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		UNREFERENCED_PARAMETER(Dpc);
		UNREFERENCED_PARAMETER(DeferredContext);

		SafeVmCall(VMCALL_INVEPT_CONTEXT, false, 0, 0);
		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_sync_cr3_exit_policy(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto statuses = reinterpret_cast<volatile SHORT*>(DeferredContext);
		if (statuses != nullptr && SafeVmCall(VMCALL_SYNC_CR3_EXIT_POLICY, 0, 0, 0))
		{
			InterlockedIncrement16(statuses);
		}
		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_read_ept_fake_page_memory(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<SoftwareBreakpointBufferArgs*>(DeferredContext);

		const BOOLEAN transaction_owner = KeSignalCallDpcSynchronize(SystemArgument2);
		if (transaction_owner && args != nullptr)
		{
			const bool transaction_succeeded = SafeVmCallEx(VMCALL_READ_EPT_FAKE_PAGE_MEMORY,
				(unsigned __int64)args->target_address,
				(unsigned __int64)args->buffer,
				args->buffer_size,
				args->current_cr3,
				0,
				0,
				0,
				0,
				0);
			InterlockedExchange(&args->transaction_status, transaction_succeeded ? 1 : -1);
		}

		KeSignalCallDpcSynchronize(SystemArgument2);
		if (args != nullptr &&
			InterlockedCompareExchange(&args->transaction_status, 0, 0) == 1)
		{
			InterlockedIncrement16(&args->statuses);
		}

		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_get_software_breakpoint_buffer(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<SoftwareBreakpointBufferArgs*>(DeferredContext);

		const BOOLEAN transaction_owner = KeSignalCallDpcSynchronize(SystemArgument2);
		if (transaction_owner && args != nullptr)
		{
			const bool transaction_succeeded = SafeVmCallEx(VMCALL_GET_SOFTWARE_BREAKPOINT_BUFFER,
				(unsigned __int64)args->target_address,
				(unsigned __int64)args->buffer,
				args->buffer_size,
				args->current_cr3,
				0,
				0,
				0,
				0,
				0);
			InterlockedExchange(&args->transaction_status, transaction_succeeded ? 1 : -1);
		}

		KeSignalCallDpcSynchronize(SystemArgument2);
		if (args != nullptr &&
			InterlockedCompareExchange(&args->transaction_status, 0, 0) == 1)
		{
			InterlockedIncrement16(&args->statuses);
		}

		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_set_software_breakpoint_buffer(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<SoftwareBreakpointBufferArgs*>(DeferredContext);

		const BOOLEAN transaction_owner = KeSignalCallDpcSynchronize(SystemArgument2);
		if (transaction_owner && args != nullptr)
		{
			const bool transaction_succeeded = SafeVmCallEx(VMCALL_SET_SOFTWARE_BREAKPOINT_BUFFER,
				(unsigned __int64)args->target_address,
				(unsigned __int64)args->buffer,
				args->buffer_size,
				args->current_cr3,
				0,
				0,
				0,
				0,
				0);
			InterlockedExchange(&args->transaction_status, transaction_succeeded ? 1 : -1);
		}

		KeSignalCallDpcSynchronize(SystemArgument2);
		// 跨核 INVEPT 走 DPC 广播：每核发一次 single-context INVEPT（刷本核唯一 EPTP）。
		if (args != nullptr &&
			InterlockedCompareExchange(&args->transaction_status, 0, 0) == 1 &&
			SafeVmCall(VMCALL_INVEPT_CONTEXT, false, 0, 0) &&
			SafeVmCall(VMCALL_SYNC_CR3_EXIT_POLICY, 0, 0, 0))
		{
			InterlockedIncrement16(&args->statuses);
		}

		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_hook_function(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<HookFunctionArgs*>(DeferredContext);

		// HyperDbg/原版式：每核直发 hook，改自己的 EPT，成功计数。失败不回滚，由上层按
		// statuses == processor_count 判断整体结果。
		if (args != nullptr &&
			SafeVmCallEx(VMCALL_EPT_RIP_HOOK,
				(unsigned __int64)args->target_address,
				(unsigned __int64)args->proxy_function,
				(unsigned __int64)args->origin_function,
				args->current_cr3,
				0,
				0,
				0,
				0,
				0))
		{
			InterlockedIncrement16(&args->statuses);
		}

		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_unhook_function(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<UnHookFunctionArgs*>(DeferredContext);

		if (args != nullptr &&
			SafeVmCall(VMCALL_EPT_UNHOOK_FUNCTION,
				args->unhook_all_functions,
				(unsigned __int64)args->function_to_unhook,
				args->current_cr3))
		{
			InterlockedIncrement16(&args->statuses);
		}

		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_vmcall(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<VmcallBroadcastArgs*>(DeferredContext);
		unsigned __int64 command = 0;

		if (args == nullptr || !TryReadVmcallCommand(args->vmcallinfo, &command))
		{
			SignalDpcDone(SystemArgument1, SystemArgument2);
			return;
		}

		const bool serialized_ept_transaction =
			hv::vmcall_dispatch_policy::IsEptMutation(command);

		if (!serialized_ept_transaction)
		{
			if (SafeVmCall(command,
				(unsigned __int64)args->vmcallinfo,
				args->parameter2,
				args->parameter3))
			{
				InterlockedIncrement16(&args->statuses);
			}

			SignalDpcDone(SystemArgument1, SystemArgument2);
			return;
		}

		const BOOLEAN transaction_owner = KeSignalCallDpcSynchronize(SystemArgument2);
		if (transaction_owner)
		{
			const bool transaction_succeeded = SafeVmCall(command,
				(unsigned __int64)args->vmcallinfo,
				args->parameter2,
				args->parameter3);
			InterlockedExchange(&args->transaction_status, transaction_succeeded ? 1 : -1);
		}

		KeSignalCallDpcSynchronize(SystemArgument2);
		// 跨核 INVEPT 走 DPC 广播：每核发一次 single-context INVEPT（刷本核唯一 EPTP）。
		if (InterlockedCompareExchange(&args->transaction_status, 0, 0) == 1 &&
			SafeVmCall(VMCALL_INVEPT_CONTEXT, false, 0, 0) &&
			SafeVmCall(VMCALL_SYNC_CR3_EXIT_POLICY, 0, 0, 0))
		{
			InterlockedIncrement16(&args->statuses);
		}

		SignalDpcDone(SystemArgument1, SystemArgument2);
	}

	void broadcast_test_vmcall(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto statuses = reinterpret_cast<volatile SHORT*>(DeferredContext);

		if (statuses != nullptr && SafeVmCall(VMCALL_TEST, 0, 0, 0))
		{
			InterlockedIncrement16(statuses);
		}

		SignalDpcDone(SystemArgument1, SystemArgument2);
	}
}

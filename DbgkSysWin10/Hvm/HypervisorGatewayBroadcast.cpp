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

namespace hvgt
{
	void SignalDpcDone(PVOID SystemArgument1, PVOID SystemArgument2)
	{
		KeSignalCallDpcSynchronize(SystemArgument2);
		KeSignalCallDpcDone(SystemArgument1);
	}

	bool SafeVmCall(unsigned __int64 command, unsigned __int64 parameter1, unsigned __int64 parameter2, unsigned __int64 parameter3)
	{
		bool status = false;

		__try
		{
			status = __vm_call(command, parameter1, parameter2, parameter3);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			status = false;
		}

		//ExReleaseFastMutex(&LongFlagsMutex);
		return status;
	}

	bool SafeVmCallEx(unsigned __int64 command,
		unsigned __int64 parameter1,
		unsigned __int64 parameter2,
		unsigned __int64 parameter3,
		unsigned __int64 parameter4,
		unsigned __int64 parameter5,
		unsigned __int64 parameter6,
		unsigned __int64 parameter7,
		unsigned __int64 parameter8,
		unsigned __int64 parameter9)
	{
		bool status = false;

		__try
		{
			status = __vm_call_ex(command,
				parameter1,
				parameter2,
				parameter3,
				parameter4,
				parameter5,
				parameter6,
				parameter7,
				parameter8,
				parameter9);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			status = false;
		}

		//ExReleaseFastMutex(&LongFlagsMutex);
		return status;
	}

	bool TryReadVmcallCommand(PVOID vmcallinfo, unsigned __int64* command)
	{
		if (vmcallinfo == nullptr || command == nullptr)
		{
			//outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
			return false;
		}

		__try
		{
			*command = ((PVMCALLINFO)vmcallinfo)->command;
			// *command = ((PVMCALLINFO)vmcallinfo)->command;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			*command = 0;
			// *command = 0;
			return false;
		}
	}

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

	void broadcast_read_ept_fake_page_memory(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<HookFunctionArgs*>(DeferredContext);

		if (args != nullptr &&
			SafeVmCallEx(VMCALL_READ_EPT_FAKE_PAGE_MEMORY,
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

	void broadcast_get_hide_software_breakpoint(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<HookFunctionArgs*>(DeferredContext);

		if (args != nullptr &&
			SafeVmCallEx(VMCALL_READ_SOFTWARE_BREAKPOINT,
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

	void broadcast_set_hide_software_breakpoint(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<HookFunctionArgs*>(DeferredContext);

		if (args != nullptr &&
			SafeVmCallEx(VMCALL_HIDE_SOFTWARE_BREAKPOINT,
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

	void broadcast_hook_function(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<HookFunctionArgs*>(DeferredContext);

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

		if (args != nullptr &&
			TryReadVmcallCommand(args->vmcallinfo, &command) &&
			SafeVmCall(command,
				(unsigned __int64)args->vmcallinfo,
				args->parameter2,
				args->parameter3))
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

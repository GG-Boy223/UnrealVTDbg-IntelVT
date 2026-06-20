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
	void vmoff()
	{
		KeGenericCallDpc(broadcast_vmoff, NULL);
	}

	void invept(bool invept_all)
	{
		if (invept_all == true)
		{
			KeGenericCallDpc(broadcast_invept_all_contexts, NULL);
		}
		else
		{
			KeGenericCallDpc(broadcast_invept_single_context, NULL);
		}
	}

	void hypervisor_visible(bool value)
	{
		if (value == true)
		{
			__vm_call(VMCALL_UNHIDE_HV_PRESENCE, 0, 0, 0);
		}
		else
		{
			__vm_call(VMCALL_HIDE_HV_PRESENCE, 0, 0, 0);
		}
	}

	bool ept_unhook()
	{
		UnHookFunctionArgs args{ true, nullptr, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_unhook_function, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	bool ept_unhook(void* function_address)
	{
		UnHookFunctionArgs args{ false, function_address, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_unhook_function, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	bool read_ept_fake_page_memory(void* target_address, void* buffer, unsigned __int64 buffer_size)
	{
		HookFunctionArgs args{ target_address, buffer, (void**)buffer_size, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_read_ept_fake_page_memory, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	bool get_hide_software_breakpoint(void* target_address, void* buffer, unsigned __int64 buffer_size)
	{
		HookFunctionArgs args{ target_address, buffer, (void**)buffer_size, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_get_hide_software_breakpoint, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	bool set_hide_software_breakpoint(MDL_MAP* map_table, void* buffer, unsigned __int64 buffer_size)
	{
		HookFunctionArgs args{ map_table, buffer, (void**)buffer_size, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_set_hide_software_breakpoint, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	bool hook_function(void* target_address, void* proxy_function, void** origin_function)
	{
		HookFunctionArgs args{ target_address, proxy_function, origin_function, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_hook_function, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	bool vmcall(PVOID vmcallinfo, unsigned __int64 parameter2, unsigned __int64 parameter3)
	{
		VmcallBroadcastArgs args = { 0 };
		args.vmcallinfo = vmcallinfo;
		args.parameter2 = parameter2;
		args.parameter3 = parameter3;
		KeGenericCallDpc(broadcast_vmcall, &args);
		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	bool test_vmcall()
	{
		volatile SHORT statuses{};
		KeGenericCallDpc(broadcast_test_vmcall, (PVOID)&statuses);

		return static_cast<ULONG>(statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	bool send_irp_perform_allocation()
	{
		return false;
	}
}

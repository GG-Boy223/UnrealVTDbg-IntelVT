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
	/// <summary>
	/// Turn off virtual machine
	/// </summary>
	void vmoff()
	{
		KeGenericCallDpc(broadcast_vmoff, NULL);
	}

	/// and paging-structure caches that were derived from extended page tables (EPT)
	/// </summary>
	/// <param name="invept_all"> If true invalidates all contexts otherway invalidate only single context (currently hv doesn't use more than 1 context)</param>
	/// <summary>
	/// Invalidates mappings in the translation lookaside buffers (TLBs)
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

	/// <summary>
	/// Set/Unset presence of hypervisor
	/// </summary>
	/// <param name="value"> If false, hypervisor is not visible via cpuid interface, If true, it become visible</param>
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

	/// Unhook all functions and invalidate tlb
	/// 卸载全部hook 并刷新tlb
	/// </summary>
	/// <returns> status </returns>
	/// <summary>
	bool ept_unhook()
	{
		UnHookFunctionArgs args{ true, nullptr, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_unhook_function, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	/// 卸载指定函数 并刷新tlb
	/// </summary>
	/// <param name="function_address"></param>
	/// <returns> status </returns>
	/// <summary>
	/// Unhook single function and invalidate tlb
	bool ept_unhook(void* function_address)
	{
		UnHookFunctionArgs args{ false, function_address, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_unhook_function, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	//读ept伪页内存
	bool read_ept_fake_page_memory(void* target_address, void* buffer, unsigned __int64 buffer_size)
	{
		HookFunctionArgs args{ target_address, buffer, (void**)buffer_size, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_read_ept_fake_page_memory, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	//读取隐形软件断点
	bool get_hide_software_breakpoint(void* target_address, void* buffer, unsigned __int64 buffer_size)
	{
		HookFunctionArgs args{ target_address, buffer, (void**)buffer_size, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_get_hide_software_breakpoint, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	//设置隐形软件断点
	bool set_hide_software_breakpoint(MDL_MAP* map_table, void* buffer, unsigned __int64 buffer_size)
	{
		HookFunctionArgs args{ map_table, buffer, (void**)buffer_size, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_set_hide_software_breakpoint, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	/// <param name="target_address">Address of function which we want to hook</param>
	/// <param name="hook_function">Address of function which is used to call original function</param>
	/// <param name="origin_function">Address of function which is used to call original function</param>
	/// <returns> status </returns>
	/// <summary>
	/// Hook function via ept and invalidates mappings
	/// </summary>
	bool hook_function(void* target_address, void* proxy_function, void** origin_function)
	{
		HookFunctionArgs args{ target_address, proxy_function, origin_function, __readcr3(), 0 };
		KeGenericCallDpc(broadcast_hook_function, &args);

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	// 默认参数保持兼容，这样旧调用点不需要改。
	bool vmcall(PVOID vmcallinfo, unsigned __int64 parameter2, unsigned __int64 parameter3)
	{
		VmcallBroadcastArgs args = { 0 };
		args.vmcallinfo = vmcallinfo;
		args.parameter2 = parameter2;
		args.parameter3 = parameter3;
		KeGenericCallDpc(broadcast_vmcall, &args);
		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	/// <summary>
	/// Check if we can communicate with hypervisor
	/// </summary>
	/// <returns> status </returns>
	bool test_vmcall()
	{
		volatile SHORT statuses{};
		KeGenericCallDpc(broadcast_test_vmcall, (PVOID)&statuses);

		return static_cast<ULONG>(statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	}

	bool send_irp_perform_allocation()
	{
		//outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
		return false;
	}
}

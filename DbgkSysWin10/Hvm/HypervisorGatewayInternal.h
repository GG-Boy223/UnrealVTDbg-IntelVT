#pragma once

#include "../Driver.h"
#include "../Globals.h"
#include "hypervisor_gateway.h"

namespace hvgt
{
	struct HookFunctionArgs
	{
		void* target_address;
		void* proxy_function;
		void** origin_function;
		unsigned __int64 current_cr3;
		volatile SHORT statuses;
		volatile LONG transaction_status;
		volatile LONG* committed_processors;
		ULONG committed_processor_count;
		volatile SHORT rollback_statuses;
	};

	// 专用于普通 vmcall 广播的参数结构。
	// 这一步把"额外参数"从调用方封装到 DPC 广播里，避免与 HookFunctionArgs 字段复用导致语义混乱。
	struct VmcallBroadcastArgs
	{
		PVOID vmcallinfo;
		unsigned __int64 parameter2;
		unsigned __int64 parameter3;
		volatile SHORT statuses;
		volatile LONG transaction_status;
	};

	struct UnHookFunctionArgs
	{
		bool unhook_all_functions;
		void* function_to_unhook;
		unsigned __int64 current_cr3;
		volatile SHORT statuses;
	};

	void SignalDpcDone(PVOID SystemArgument1, PVOID SystemArgument2);
	bool SafeVmCall(unsigned __int64 command, unsigned __int64 parameter1, unsigned __int64 parameter2, unsigned __int64 parameter3);
	bool SafeVmCallEx(unsigned __int64 command,
		unsigned __int64 parameter1,
		unsigned __int64 parameter2,
		unsigned __int64 parameter3,
		unsigned __int64 parameter4,
		unsigned __int64 parameter5,
		unsigned __int64 parameter6,
		unsigned __int64 parameter7,
		unsigned __int64 parameter8,
		unsigned __int64 parameter9);
	bool TryReadVmcallCommand(PVOID vmcallinfo, unsigned __int64* command);

	void broadcast_vmoff(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_invept_all_contexts(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_invept_single_context(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_sync_cr3_exit_policy(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_read_ept_fake_page_memory(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_get_hide_software_breakpoint(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_set_hide_software_breakpoint(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_hook_function(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_rollback_hook_function(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_unhook_function(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_vmcall(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
	void broadcast_test_vmcall(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
}
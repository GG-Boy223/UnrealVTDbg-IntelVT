#include "Driver.h" // 引入驱动核心头文件：提供内核类型、宏、调试输出等基础定义，保证本文件可调用内核 API
#include "hypervisor_gateway.h" // 引入网关层头文件：确保这里实现的函数与对外声明完全一致
#include "AsmCallset.h" // 引入汇编封装：提供 __vm_call / __vm_call_ex 等 VMX 指令的 C++ 包装
#include "vmcall_reason.h" // 引入 VMCALL 命令编号：告诉 Hypervisor 执行哪一种具体操作
#include "ntapi.h" // 引入内核 API 声明：包括 KeGenericCallDpc 等跨 CPU 广播机制
#include "poolmanager.h" // Globals.h 中的 VMM 上下文包含 pool_manager 类型
#include "Globals.h" // 访问每核 VCPU 状态，避免对未启动 CPU 发送 VMXOFF VMCALL

namespace hvgt // Hypervisor Gateway：普通内核线程调用 Hypervisor 的“桥梁层”
{

	// 这里的参数结构体用于在“所有 CPU 上执行 Hook”时传参。
	// 初学者理解：Hook 不是只在当前 CPU 生效，而是每个 CPU 都有自己的缓存和执行路径，
	// 所以必须让所有 CPU 都执行一次 Hook，保证一致性。
	struct HookFunctionArgs
	{
		void* target_address; // 目标函数地址：要被 Hook 的函数入口（来宾内核中的地址）
		void* proxy_function; // 代理函数地址：Hook 后真正执行的新函数入口
		void** origin_function; // 输出参数：返回“原函数入口”，即 trampoline，方便需要时调用原逻辑
		unsigned __int64 current_cr3; // 当前 CR3：表示当前地址空间的页表基址，Hypervisor 用它解析虚拟地址
		volatile SHORT statuses; // 成功计数：每个 CPU 成功后原子 +1，用于判断是否全部 CPU 完成
	};

	// 用于在“所有 CPU 上卸载 Hook”时传参。
	// 初学者理解：卸载 Hook 也必须在每个 CPU 上执行，否则某些 CPU 仍可能执行旧映射。
	struct UnHookFunctionArgs
	{
		bool unhook_all_functions; // true=卸载全部 Hook；false=只卸载一个指定函数的 Hook
		void* function_to_unhook; // 指定卸载的函数地址（当 unhook_all_functions 为 false 才使用）
		unsigned __int64 current_cr3; // 当前 CR3：保证 Hypervisor 在正确的地址空间操作
		volatile SHORT statuses; // 成功计数：每个 CPU 成功后原子 +1
	};

	// DPC 回调：在每个 CPU 上刷新“所有 EPT 上下文”。
	// 初学者理解：EPT 是地址映射缓存，改了 EPT 表必须刷新，否则 CPU 继续用旧缓存。
	void broadcast_invept_all_contexts(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		UNREFERENCED_PARAMETER(Dpc); // 不使用 DPC 参数
		UNREFERENCED_PARAMETER(DeferredContext); // 不使用上下文参数

		__vm_call(VMCALL_INVEPT_CONTEXT, true, 0, 0); // VMCALL：true 表示刷新“所有 EPT 上下文”
		KeSignalCallDpcSynchronize(SystemArgument2); // 同步通知
		KeSignalCallDpcDone(SystemArgument1); // 完成通知
	}

	// DPC 回调：在每个 CPU 上刷新“单一 EPT 上下文”。
	// 初学者理解：如果系统只用一个 EPTP，就只刷新当前上下文，代价更低。
	void broadcast_invept_single_context(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		UNREFERENCED_PARAMETER(Dpc); // 不使用 DPC 参数
		UNREFERENCED_PARAMETER(DeferredContext); // 不使用上下文参数

		__vm_call(VMCALL_INVEPT_CONTEXT, false, 0, 0); // VMCALL：false 表示只刷新当前 EPT 上下文
		KeSignalCallDpcSynchronize(SystemArgument2); // 同步通知
		KeSignalCallDpcDone(SystemArgument1); // 完成通知
	}

	// DPC 回调：读取隐藏软件断点。
	// 初学者理解：隐藏断点需要一致状态，所以让每个 CPU 都执行同样读取操作。
	void broadcast_get_hide_software_breakpoint(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<HookFunctionArgs*>(DeferredContext); // 把上下文指针转成参数结构体

		// __vm_call_ex 可以传更多参数（如目标地址、缓冲区地址、CR3），方便 Hypervisor 处理
		if (__vm_call_ex(VMCALL_READ_SOFTWARE_BREAKPOINT, (unsigned __int64)args->target_address,
			(unsigned __int64)args->proxy_function, (unsigned __int64)args->origin_function, args->current_cr3, 0, 0, 0, 0, 0))
		{
			InterlockedIncrement16(&args->statuses); // 成功则计数 +1（原子操作，线程安全）
		}

		KeSignalCallDpcSynchronize(SystemArgument2); // 同步通知
		KeSignalCallDpcDone(SystemArgument1); // 完成通知
	}

	// DPC 回调：设置隐藏软件断点。
	// 初学者理解：断点必须在所有 CPU 生效，所以广播执行设置。
	void broadcast_set_hide_software_breakpoint(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<HookFunctionArgs*>(DeferredContext); // 获取参数结构体

		if (__vm_call_ex(VMCALL_HIDE_SOFTWARE_BREAKPOINT, (unsigned __int64)args->target_address,
			(unsigned __int64)args->proxy_function, (unsigned __int64)args->origin_function, args->current_cr3, 0, 0, 0, 0, 0))
		{
			InterlockedIncrement16(&args->statuses); // 成功则计数 +1
		}

		KeSignalCallDpcSynchronize(SystemArgument2); // 同步通知
		KeSignalCallDpcDone(SystemArgument1); // 完成通知
	}

	// DPC 回调：执行 EPT Hook（CC Hook）。
	// 初学者理解：Hook 后 EPT 映射被修改，必须在所有 CPU 上设置一致映射。
	void broadcast_hook_function(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<HookFunctionArgs*>(DeferredContext); // 获取参数结构体

		if (__vm_call_ex(VMCALL_EPT_CC_HOOK, (unsigned __int64)args->target_address,
			(unsigned __int64)args->proxy_function, (unsigned __int64)args->origin_function, args->current_cr3, 0, 0, 0, 0, 0))
		{
			InterlockedIncrement16(&args->statuses); // 成功则计数 +1
		}

		KeSignalCallDpcSynchronize(SystemArgument2); // 同步通知
		KeSignalCallDpcDone(SystemArgument1); // 完成通知
	}

	// DPC 回调：卸载 EPT Hook。
	// 初学者理解：所有 CPU 都需要撤销 Hook，否则会有 CPU 继续执行旧映射。
	void broadcast_unhook_function(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto args = reinterpret_cast<UnHookFunctionArgs*>(DeferredContext); // 获取参数结构体

		if (__vm_call(VMCALL_EPT_UNHOOK_FUNCTION, args->unhook_all_functions,
			(unsigned __int64)args->function_to_unhook, args->current_cr3))
		{
			InterlockedIncrement16(&args->statuses); // 成功则计数 +1
		}

		KeSignalCallDpcSynchronize(SystemArgument2); // 同步通知
		KeSignalCallDpcDone(SystemArgument1); // 完成通知
	}

	// DPC 回调：测试 VMCALL 通信。
	// 初学者理解：让每个 CPU 调一次 VMCALL_TEST，如果都成功说明 Hypervisor 通道正常。
	void broadcast_test_vmcall(KDPC*, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
	{
		const auto statuses = reinterpret_cast<volatile SHORT*>(DeferredContext); // 取得成功计数指针

		if (__vm_call(VMCALL_TEST, 0, 0, 0))
		{
			InterlockedIncrement16(statuses); // 成功则计数 +1
		}

		KeSignalCallDpcSynchronize(SystemArgument2); // 同步通知
		KeSignalCallDpcDone(SystemArgument1); // 完成通知
	}

	// 刷新 EPT 映射缓存。
	// 初学者理解：修改 EPT 后必须刷新，否则 CPU 继续使用旧映射。
	void invept(bool invept_all)
	{
		if (invept_all == true) KeGenericCallDpc(broadcast_invept_all_contexts, NULL); // 刷新所有 EPT 上下文
		else KeGenericCallDpc(broadcast_invept_single_context, NULL); // 刷新单一 EPT 上下文
	}

	// 控制 Hypervisor 在 CPUID 中是否可见。
	// 初学者理解：一些软件通过 CPUID 判断是否存在 Hypervisor，这里可隐藏或显示。
	void hypervisor_visible(bool value)
	{
		if (value == true) // 根据 value 决定置位还是清位
			__vm_call(VMCALL_UNHIDE_HV_PRESENCE, 0, 0, 0); // 让 Hypervisor 可见
		else
			__vm_call(VMCALL_HIDE_HV_PRESENCE, 0, 0, 0); // 让 Hypervisor 隐藏
	}

	// 卸载全部 Hook。
	// 初学者理解：所有 CPU 都必须撤销 Hook，才能保证执行一致性。
	bool ept_unhook()
	{
		UnHookFunctionArgs args{ true, nullptr, __readcr3(), 0 }; // 构造参数：卸载全部 Hook
		KeGenericCallDpc(broadcast_unhook_function, &args); // 广播执行卸载

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS); // 检查是否所有 CPU 成功
	}

	// 卸载指定函数的 Hook。
	bool ept_unhook(void* function_address)
	{
		UnHookFunctionArgs args{ false, function_address, __readcr3(), 0 }; // 构造参数：卸载指定函数
		KeGenericCallDpc(broadcast_unhook_function, &args); // 广播执行卸载

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS); // 所有 CPU 成功才返回 true
	}

	// 读取隐藏软件断点。
	bool get_hide_software_breakpoint(void* target_address, void* buffer, unsigned __int64 buffer_size)
	{
		HookFunctionArgs args{ target_address, buffer, (void**)buffer_size, __readcr3(), 0 }; // 构造读取参数
		KeGenericCallDpc(broadcast_get_hide_software_breakpoint, &args); // 广播读取

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS); // 所有 CPU 成功才返回 true
	}

	// 执行 EPT Hook。
	bool hook_function(void* target_address, void* proxy_function, void** origin_function)
	{
		HookFunctionArgs args{ target_address, proxy_function, origin_function, __readcr3(), 0 }; // 构造 Hook 参数
		KeGenericCallDpc(broadcast_hook_function, &args); // 广播 Hook

		return static_cast<ULONG>(args.statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS); // 所有 CPU 成功才返回 true
	}

	// 测试 VMCALL 通信是否正常。
	bool test_vmcall()
	{
		volatile SHORT statuses{}; // 成功计数初始化为 0
		KeGenericCallDpc(broadcast_test_vmcall, (PVOID)&statuses); // 广播测试 VMCALL

		return static_cast<ULONG>(statuses) == KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS); // 所有 CPU 成功才返回 true
	}

	// 通过 IOCTL 请求 Hypervisor 分配内存（当前未启用）。
	bool send_irp_perform_allocation()
	{
		return false;
	}
}

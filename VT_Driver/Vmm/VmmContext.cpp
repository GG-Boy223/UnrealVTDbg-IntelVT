#include "VmmInternal.h"
#include "../VmExit/VmExitTelemetry.h"

namespace
{
	bool are_all_vcpus_quiescent(unsigned int* first_active_processor)
	{
		if (first_active_processor != nullptr)
		{
			*first_active_processor = static_cast<unsigned int>(-1);
		}

		if (g_vmm_context.vcpu == nullptr)
		{
			return true;
		}

		for (unsigned int i = 0; i < g_vmm_context.processor_count; ++i)
		{
			auto const& vcpu = g_vmm_context.vcpu[i];
			if (!hv::vmm_release_policy::IsVcpuQuiescent(
				vcpu.vcpu_status.vmx_on,
				vcpu.vcpu_status.vmm_launched,
				vcpu.vmx_off_state.vmx_off_executed))
			{
				if (first_active_processor != nullptr)
				{
					*first_active_processor = i;
				}
				return false;
			}
		}

		return true;
	}

	void verify_vmm_context_can_be_released()
	{
		unsigned int first_active_processor = static_cast<unsigned int>(-1);
		const bool all_vcpus_quiescent = are_all_vcpus_quiescent(&first_active_processor);
		const auto lifecycle_state = g_vmm_context.lifecycle_state;
		if (!hv::vmm_release_policy::CanRelease(lifecycle_state, all_vcpus_quiescent))
		{
			vmx_fatal_error(
				vmx_fatal_release_active_context,
				static_cast<unsigned __int64>(lifecycle_state),
				first_active_processor);
		}
	}

	void log_vmx_runtime_snapshots() // 在安全清理上下文输出VMX运行期快照，避免VM-exit路径内做文件IO。
	{
		if (g_vmm_context.vcpu == nullptr || g_vmm_context.processor_count == 0)
		{
			return;
		}

		for (unsigned int i = 0; i < g_vmm_context.processor_count; ++i)
		{
			auto const& resume_snapshot = g_vmm_context.vcpu[i].vmresume_failure_snapshot;
			if (resume_snapshot.count != 0)
			{
				outFile("[警告][VMRESUME失败快照] CPU[%u] Count=%llu RFLAGS=0x%llX VMfailValid=%llu VMfailInvalid=%llu VMREAD失败掩码=0x%llX VM_INSTRUCTION_ERROR=%llu EXIT_REASON=0x%llX EXIT_QUALIFICATION=0x%llX GUEST_RIP=0x%llX GUEST_RSP=0x%llX GUEST_CR3=0x%llX",
					i,
					resume_snapshot.count,
					resume_snapshot.rflags,
					resume_snapshot.vmfail_valid,
					resume_snapshot.vmfail_invalid,
					resume_snapshot.vmread_failure_mask,
					resume_snapshot.vm_instruction_error,
					resume_snapshot.vm_exit_reason,
					resume_snapshot.exit_qualification,
					resume_snapshot.guest_rip,
					resume_snapshot.guest_rsp,
					resume_snapshot.guest_cr3);
			}

			auto const& host_snapshot = g_vmm_context.vcpu[i].host_exception_snapshot;
			if (host_snapshot.count != 0)
			{
				outFile("[警告][Host异常快照] CPU[%u] Count=%llu Vector=%llu Error=0x%llX RIP=0x%llX RSP=0x%llX",
					i,
					host_snapshot.count,
					host_snapshot.vector,
					host_snapshot.error,
					host_snapshot.rip,
					host_snapshot.rsp);
			}
		}
	}
}

void free_vmm_context() // 释放VMM全局资源与每核VCPU相关内存，避免驱动卸载后内存泄漏
{
	verify_vmm_context_can_be_released();
	// 旧逻辑：free_vmm_context直接释放VCPU，VMRESUME失败/Host异常快照会随内存释放丢失。
	log_vmx_runtime_snapshots(); // 先输出运行期诊断快照；该日志不改变进入VT条件，也不在VM-exit路径执行。
	VmExitTelemetryDump();
	hv::cleanup_host_page_tables(); // 清零 Host 页表状态；此时VMX已回滚/关闭，不影响VT功能链路。

	//if (g_vmm_context != nullptr)
	{
		// POOL MANAGER
		if (g_vmm_context.pool_manager != nullptr)
		{
			pool_manager::uninitialize(); // 先反初始化内存池管理器，防止其内部状态悬挂
			free_pool(g_vmm_context.pool_manager); // 释放内存池结构，回收占用的内核内存
			g_vmm_context.pool_manager = nullptr;
		}

		// VCPU TABLE
		if (g_vmm_context.vcpu != nullptr)
		{
			for (unsigned int i = 0; i < g_vmm_context.processor_count; i++)
			{
				// VCPU
				//if (g_vmm_context.vcpu[i] != nullptr)
				{
					// VCPU VMM STACK
					//if (g_vmm_context.vcpu_table[i]->vmm_stack != nullptr)
					//{
					//	free_pool(g_vmm_context.vcpu_table[i]->vmm_stack);
					//}

					// IO BITMAP A
					if (g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_a != nullptr)
					{
						free_vmm_page_aligned(g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_a); // 释放IO位图A，避免每核位图泄漏
						g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_a = nullptr;
					}

					// IO BITMAP B
					if (g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_b != nullptr)
					{
						free_vmm_page_aligned(g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_b); // 释放IO位图B，避免每核位图泄漏
						g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_b = nullptr;
					}

					// EPT_STATE
					if (g_vmm_context.vcpu[i].ept_state != nullptr)
					{
						ept::cleanup_page_table(*g_vmm_context.vcpu[i].ept_state); // 先统一释放EPT动态页和RAM范围缓存，避免部分初始化失败时遗留。
						// EPT POINTER
						if (g_vmm_context.vcpu[i].ept_state->ept_pointer != nullptr)
						{
							free_vmm_page_aligned(g_vmm_context.vcpu[i].ept_state->ept_pointer); // 释放EPTP结构，回收EPT控制块
							g_vmm_context.vcpu[i].ept_state->ept_pointer = nullptr;
						}
						// EPT PAGE TABLE
						if (g_vmm_context.vcpu[i].ept_state->ept_page_table != nullptr)
						{
							// ept::cleanup_page_table(*g_vmm_context.vcpu[i].ept_state); // 旧逻辑：只在ept_page_table非空时清理，部分失败路径会漏掉RAM范围缓存。
							free_vmm_page_aligned(g_vmm_context.vcpu[i].ept_state->ept_page_table); // 释放EPT页表，避免大量物理页泄漏
							g_vmm_context.vcpu[i].ept_state->ept_page_table = nullptr;
						}

						free_pool(g_vmm_context.vcpu[i].ept_state); // 释放EPT状态结构本体，清理每核EPT元数据
						g_vmm_context.vcpu[i].ept_state = nullptr;
					}

					//free_pool(g_vmm_context.vcpu_table[i]);
				}
			}
			free_vmm_page_aligned(g_vmm_context.vcpu); // 释放VCPU数组，清理每核上下文容器
			//g_vmm_context.vcpu = nullptr;
			//g_vmm_context.processor_count = 0;
		}

		//free_pool(g_vmm_context);
	}

	g_vmm_context.vcpu = nullptr;
	g_vmm_context.processor_count = 0;
	g_vmm_context.hv_presence = false;
	g_vmm_context.lifecycle_state = hv::vmm_release_policy::lifecycle_state::empty;
}

// 扫描每个逻辑处理器上的 CPUID.80000008H，取全平台最大的物理地址上界。
// EPT 覆盖范围按“平台最大值”而不是“当前核报的值”计算：来宾（宿主操作系统）的 MMIO/BAR
// 是按全平台解码能力分配的，某个核若按自己较小的 MAXPHYADDR 建表，访问高地址 BAR 时会命中未映射条目。
bool query_platform_physical_address_limit()
{
	unsigned __int64 platform_max_physical_end = 0;
	unsigned __int32 widest_physical_bits = 0;

	for (unsigned int iter = 0; iter < g_vmm_context.processor_count; ++iter)
	{
		GROUP_AFFINITY orig_affinity = {};
		KeSetSystemGroupAffinityThread(&g_vmm_context.vcpu[iter].processor_affinity, &orig_affinity);

		int cpuid_regs[4] = {};
		__cpuid(cpuid_regs, 0x80000008);
		unsigned __int32 const max_phys_addr_bits = static_cast<unsigned __int32>(cpuid_regs[0]) & 0xFFu;
		g_vmm_context.vcpu[iter].max_physical_address_bits = max_phys_addr_bits;

		KeRevertToUserGroupAffinityThread(&orig_affinity);

		if (max_phys_addr_bits != 0 && max_phys_addr_bits < 64)
		{
			unsigned __int64 const max_phys_end = 1ull << max_phys_addr_bits;
			if (max_phys_end > platform_max_physical_end)
			{
				platform_max_physical_end = max_phys_end;
				widest_physical_bits = max_phys_addr_bits;
			}
		}
	}

	if (platform_max_physical_end == 0)
	{
		LogError("CPUID.80000008H MAXPHYADDR is unavailable on every logical processor");
		outFile("[失败][EPT覆盖范围] 所有逻辑处理器都未报告有效的MAXPHYADDR，停止VT加载");
		return false;
	}

	g_vmm_context.platform_max_physical_end = platform_max_physical_end;
	outFile("[信息][EPT覆盖范围] 全平台MAXPHYADDR最大值=%u位 物理上界=0x%llX（已扫描%u个逻辑处理器）",
			widest_physical_bits,
			g_vmm_context.platform_max_physical_end,
			g_vmm_context.processor_count);
	return true;
}

//分配g_vmm_context上下文
bool allocate_vmm_context() // 初始化全局VMM资源，包含每核VCPU与EPT基础结构，为启动VMM做准备
{
	g_vmm_context.lifecycle_state = hv::vmm_release_policy::lifecycle_state::allocated;
	__cpuid_info cpuid_reg = { 0 }; // CPUID临时缓冲，用于读取CPU能力信息
	outFile("========== VMM全局上下文分配开始 ==========");

	USHORT active_group_count = KeQueryActiveGroupCount();
	if (active_group_count != 1)
	{
		outDebug("当前 VMM 布局不支持多个处理器组。\n");
		return false;
	}

	//
	// Allocate virtual cpu context for every logical core
	// 为每个逻辑处理器分配虚拟 CPU 上下文
	g_vmm_context.processor_count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS); // 统计全部处理器组的逻辑处理器数量


	outFile("[信息][VMM上下文] 活跃逻辑处理器数量=%u 活跃处理器组数量=%u",
		g_vmm_context.processor_count,
		KeQueryActiveGroupCount());
	if (g_vmm_context.processor_count == 0 || g_vmm_context.processor_count > 64)
	{
		outFile("[失败][VMM上下文] 系统报告的逻辑处理器数量为0，停止VT加载");
		return false;
	}

	auto const arr_size = sizeof(__vcpu) * g_vmm_context.processor_count; // 计算VCPU数组大小，避免越界
	outFile("[进行中][VMM上下文] 分配VCPU数组：Bytes=%llu", static_cast<unsigned __int64>(arr_size));
	g_vmm_context.vcpu = allocate_page_aligned_pool<__vcpu*>(arr_size); // VCPU数组内含alignas(PAGE_SIZE)硬件结构,必须使用连续物理页分配,不能使用普通池。
	if (g_vmm_context.vcpu == nullptr)
	{
		outFile("[失败][VMM上下文] VCPU数组分配失败");
		return false; // 返回失败，防止使用空指针
	}
	RtlSecureZeroMemory(g_vmm_context.vcpu, arr_size); // 清零VCPU数组，避免随机值影响VMCS配置
	outFile("[成功][VMM上下文] VCPU数组分配完成：VA=0x%p", g_vmm_context.vcpu);

	//
	// Build mtrr map for physcial memory caching informations
	// 构建 mtrr 映射来存储物理内存缓存信息
	//
	outFile("[进行中][VMM上下文] 开始构建MTRR内存类型映射");
	//
	// Build mtrr map for physcial memory caching informations
	// 构建 mtrr 映射来存储物理内存缓存信息
	ept::build_mtrr_map(); // 读取MTRR并构建缓存类型映射，确保EPT内存类型与CPU一致
	outFile("[成功][VMM上下文] MTRR内存类型映射构建完成");

	//提前在guest里分配了内存
	outFile("[进行中][VMM上下文] 开始初始化预分配内存池");
	//提前在guest里分配了内存
	if (pool_manager::initialize() == false) // 预分配内存池，减少VMX路径中的动态分配
	{
		//free_vmm_context();
		outFile("[失败][VMM上下文] 预分配内存池初始化失败");
		return false; // 直接返回失败，避免不完整初始化
	}
	outFile("[成功][VMM上下文] 预分配内存池初始化完成");

	// virtualize every cpu
	for (unsigned int iter = 0; iter < g_vmm_context.processor_count; iter++)
	{
		outFile("---------- VCPU[%u] 上下文初始化开始 ----------", iter);
		if (init_vcpu(&g_vmm_context.vcpu[iter]) == false) // 初始化每个VCPU的位图与EPT状态，保证每核可进入VMX
		{
			//free_vmm_context();
			outFile("[失败][VMM上下文] VCPU[%u] 初始化失败，停止VT加载", iter);
			return false;
		}
		outFile("[成功][VMM上下文] VCPU[%u] 初始化完成", iter);

		PROCESSOR_NUMBER processor_number = {};
		NTSTATUS processor_status = KeGetProcessorNumberFromIndex(iter, &processor_number);
		if (!NT_SUCCESS(processor_status))
		{
			LogError("KeGetProcessorNumberFromIndex failed for index %u: 0x%X", iter, processor_status);
			outFile("[失败][VMM上下文] 获取CPU[%u]处理器组拓扑失败：Status=0x%X", iter, processor_status);
			return false;
		}

		g_vmm_context.vcpu[iter].logical_processor_index = iter;
		g_vmm_context.vcpu[iter].processor_number = processor_number;
		RtlSecureZeroMemory(&g_vmm_context.vcpu[iter].processor_affinity, sizeof(GROUP_AFFINITY));
		g_vmm_context.vcpu[iter].processor_affinity.Group = processor_number.Group;
		g_vmm_context.vcpu[iter].processor_affinity.Mask = (static_cast<KAFFINITY>(1) << processor_number.Number);
		outFile("[信息][VMM上下文] CPU[%u]拓扑：Group=%u Number=%u AffinityMask=0x%llX",
			iter,
			processor_number.Group,
			processor_number.Number,
			static_cast<unsigned __int64>(g_vmm_context.vcpu[iter].processor_affinity.Mask));
	}

	g_vmm_context.hv_presence = true; // 标记VMM已存在，便于其他逻辑快速判断
	outFile("[成功][VMM上下文] VMM存在标记已设置");

	__cpuid((int*)&cpuid_reg.eax, 0); // 读取CPUID最高基础叶，便于后续能力判断
	g_vmm_context.highest_basic_leaf = cpuid_reg.eax; // 缓存最高叶，避免反复执行CPUID
	outFile("[信息][VMM上下文] CPUID最高基础叶：0x%X", g_vmm_context.highest_basic_leaf);

	// 必须先扫描全平台 MAXPHYADDR，再构建 EPT：ept::initialize 用 platform_max_physical_end 决定覆盖范围，
	// 否则高位 MMIO（>RAM 上界）不在 EPT 内，访问即 EPT violation。
	if (!query_platform_physical_address_limit())
	{
		outFile("[失败][VMM上下文] 全平台物理地址上界扫描失败，停止VT加载");
		return false;
	}

	// 时序要求：所有 host 模式分配（EPT 页表）必须在复制 Host PML4 内核半区之前完成，
	// 否则这些分配新占用的内核 PML4 槽不会出现在 Host CR3 里，VMX root 访问即缺页。
	// EPT 必须在各自逻辑处理器上构建（IA32_VMX_EPT_VPID_CAP 的 1GB 大页、MAXPHYADDR、MTRR 都是每核状态）。
	for (unsigned int ept_iter = 0; ept_iter < g_vmm_context.processor_count; ept_iter++)
	{
		GROUP_AFFINITY ept_orig_affinity = {};
		KeSetSystemGroupAffinityThread(&g_vmm_context.vcpu[ept_iter].processor_affinity, &ept_orig_affinity);
		bool const ept_ok = init_vcpu_ept(&g_vmm_context.vcpu[ept_iter]);
		KeRevertToUserGroupAffinityThread(&ept_orig_affinity);
		if (!ept_ok)
		{
			outFile("[失败][VMM上下文] CPU[%u] EPT页表构建失败，停止VT加载", ept_iter);
			return false;
		}
	}

	//创建host页表,将所有物理内存映射到我们的地址空间
	outFile("[进行中][VMM上下文] 开始创建Host页表");
	if (!create_host_page_tables()) // Host CR3 是所有 VM-exit 的根模式执行环境，失败后不能继续 VMLAUNCH。
	{
		outFile("[失败][VMM上下文] Host页表创建失败，停止VT加载，避免进入不可用的VM-Exit宿主环境");
		return false;
	}
	outFile("[成功][VMM上下文] Host页表创建完成");

	outFile("========== VMM全局上下文分配完成 ==========");
	return true;
}

#include "VmmInternal.h"

void free_vmm_context() // 释放VMM全局资源与每核VCPU相关内存，避免驱动卸载后内存泄漏
{
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
						free_pool(g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_a); // 释放IO位图A，避免每核位图泄漏
						g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_a = nullptr;
					}

					// IO BITMAP B
					if (g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_b != nullptr)
					{
						free_pool(g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_b); // 释放IO位图B，避免每核位图泄漏
						g_vmm_context.vcpu[i].vcpu_bitmaps.io_bitmap_b = nullptr;
					}

					// EPT_STATE
					if (g_vmm_context.vcpu[i].ept_state != nullptr)
					{
						// EPT POINTER
						if (g_vmm_context.vcpu[i].ept_state->ept_pointer != nullptr)
						{
							free_pool(g_vmm_context.vcpu[i].ept_state->ept_pointer); // 释放EPTP结构，回收EPT控制块
							g_vmm_context.vcpu[i].ept_state->ept_pointer = nullptr;
						}
						// EPT PAGE TABLE
						if (g_vmm_context.vcpu[i].ept_state->ept_page_table != nullptr)
						{
							ept::cleanup_page_table(*g_vmm_context.vcpu[i].ept_state); // 释放动态扩展的 PDPT/PDE 页
							free_pool(g_vmm_context.vcpu[i].ept_state->ept_page_table); // 释放EPT页表，避免大量物理页泄漏
							g_vmm_context.vcpu[i].ept_state->ept_page_table = nullptr;
						}

						free_pool(g_vmm_context.vcpu[i].ept_state); // 释放EPT状态结构本体，清理每核EPT元数据
						g_vmm_context.vcpu[i].ept_state = nullptr;
					}

					//free_pool(g_vmm_context.vcpu_table[i]);
				}
			}
			free_pool(g_vmm_context.vcpu); // 释放VCPU数组，清理每核上下文容器
			//g_vmm_context.vcpu = nullptr;
			//g_vmm_context.processor_count = 0;
		}

		//free_pool(g_vmm_context);
	}

	g_vmm_context.vcpu = nullptr;
	g_vmm_context.processor_count = 0;
	g_vmm_context.hv_presence = false;
}

//分配g_vmm_context上下文
bool allocate_vmm_context() // 初始化全局VMM资源，包含每核VCPU与EPT基础结构，为启动VMM做准备
{
	__cpuid_info cpuid_reg = { 0 }; // CPUID临时缓冲，用于读取CPU能力信息
	outFile("========== VMM全局上下文分配开始 ==========");

	//
	// Allocate virtual cpu context for every logical core
	// 为每个逻辑处理器分配虚拟 CPU 上下文
	g_vmm_context.processor_count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS); // 统计全部处理器组的逻辑处理器数量
	outFile("[信息][VMM上下文] 活跃逻辑处理器数量=%u 活跃处理器组数量=%u",
		g_vmm_context.processor_count,
		KeQueryActiveGroupCount());
	if (g_vmm_context.processor_count == 0)
	{
		outDebug("不支持当前逻辑处理器数量。\n");
		outFile("[失败][VMM上下文] 系统报告的逻辑处理器数量为0，停止VT加载");
		return false;
	}

	auto const arr_size = sizeof(__vcpu) * g_vmm_context.processor_count; // 计算VCPU数组大小，避免越界
	outFile("[进行中][VMM上下文] 分配VCPU数组：Bytes=%llu", static_cast<unsigned __int64>(arr_size));
	g_vmm_context.vcpu = allocate_pool<__vcpu*>(arr_size); // 为每个逻辑核分配VCPU结构数组，作为每核状态容器
	if (g_vmm_context.vcpu == nullptr)
	{
		outDebug("vcpu_table could not be allocated分配失败"); // 分配失败，无法继续初始化后续VMX结构
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
	ept::build_mtrr_map(); // 读取MTRR并构建缓存类型映射，确保EPT内存类型与CPU一致
	outFile("[成功][VMM上下文] MTRR内存类型映射构建完成");

	//提前在guest里分配了内存
	outFile("[进行中][VMM上下文] 开始初始化预分配内存池");
	if (pool_manager::initialize() == false) // 预分配内存池，减少VMX路径中的动态分配
	{
		outDebug("预分配内存失败!\n"); // 预分配失败会影响后续VMX/EPT结构
		//free_vmm_context();
		outFile("[失败][VMM上下文] 预分配内存池初始化失败");
		return false; // 直接返回失败，避免不完整初始化
	}
	outFile("[成功][VMM上下文] 预分配内存池初始化完成");

	for (unsigned int iter = 0; iter < g_vmm_context.processor_count; iter++)
	{
		outFile("---------- VCPU[%u] 上下文初始化开始 ----------", iter);
		if (init_vcpu(&g_vmm_context.vcpu[iter]) == false) // 初始化每个VCPU的位图与EPT状态，保证每核可进入VMX
		{
			outDebug("init_vcpu失败!\n"); // 其中任一失败即整体失败，避免部分核进入VMX
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

	//创建host页表
	//将所有物理内存映射到我们的地址空间
	outFile("[进行中][VMM上下文] 开始创建Host页表");
	create_host_page_tables();// 构建宿主页表与物理内存映射，便于VMX根模式访问和地址转换
	outFile("[完成][VMM上下文] Host页表创建流程结束");

	//if (!create_host_page_tables())
	//{
	//	outDebug("create_host_page_tables 失败!\n");
	//	//free_vmm_context();
	//	return false;
	//}

	outFile("========== VMM全局上下文分配完成 ==========");
	return true;
}

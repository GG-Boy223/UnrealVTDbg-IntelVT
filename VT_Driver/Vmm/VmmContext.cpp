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

	//
	// Allocate virtual cpu context for every logical core
	// 为每个逻辑处理器分配虚拟 CPU 上下文
	USHORT active_group_count = KeQueryActiveGroupCount();
	if (active_group_count != 1)
	{
		outDebug("当前 VMM 布局不支持多个处理器组。\n");
		return false;
	}

   //g_vmm_context.processor_count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
	g_vmm_context.processor_count = KeQueryActiveProcessorCount(NULL); // 统计系统逻辑处理器数量，确保每核都有VCPU
	if (g_vmm_context.processor_count == 0 || g_vmm_context.processor_count > 64)
	{
		outDebug("不支持当前逻辑处理器数量。\n");
		return false;
	}

	auto const arr_size = sizeof(__vcpu) * g_vmm_context.processor_count; // 计算VCPU数组大小，避免越界
	g_vmm_context.vcpu = allocate_pool<__vcpu*>(arr_size); // 为每个逻辑核分配VCPU结构数组，作为每核状态容器
	if (g_vmm_context.vcpu == nullptr)
	{
		outDebug("vcpu_table could not be allocated分配失败"); // 分配失败，无法继续初始化后续VMX结构
		return false; // 返回失败，防止使用空指针
	}
	RtlSecureZeroMemory(g_vmm_context.vcpu, arr_size); // 清零VCPU数组，避免随机值影响VMCS配置

	//
	// Build mtrr map for physcial memory caching informations
	// 构建 mtrr 映射来存储物理内存缓存信息
	//
	ept::build_mtrr_map(); // 读取MTRR并构建缓存类型映射，确保EPT内存类型与CPU一致

	//提前在guest里分配了内存
	if (pool_manager::initialize() == false) // 预分配内存池，减少VMX路径中的动态分配
	{
		outDebug("预分配内存失败!\n"); // 预分配失败会影响后续VMX/EPT结构
		//free_vmm_context();
		return false; // 直接返回失败，避免不完整初始化
	}

	for (unsigned int iter = 0; iter < g_vmm_context.processor_count; iter++)
	{
		if (init_vcpu(&g_vmm_context.vcpu[iter]) == false) // 初始化每个VCPU的位图与EPT状态，保证每核可进入VMX
		{
			outDebug("init_vcpu失败!\n"); // 其中任一失败即整体失败，避免部分核进入VMX
			//free_vmm_context();
			return false;
		}
	}

	g_vmm_context.hv_presence = true; // 标记VMM已存在，便于其他逻辑快速判断

	__cpuid((int*)&cpuid_reg.eax, 0); // 读取CPUID最高基础叶，便于后续能力判断
	g_vmm_context.highest_basic_leaf = cpuid_reg.eax; // 缓存最高叶，避免反复执行CPUID

	//创建host页表
	//将所有物理内存映射到我们的地址空间
	create_host_page_tables();// 构建宿主页表与物理内存映射，便于VMX根模式访问和地址转换

	//if (!create_host_page_tables())
	//{
	//	outDebug("create_host_page_tables 失败!\n");
	//	//free_vmm_context();
	//	return false;
	//}

	return true;
}

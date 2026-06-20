#include "VmmInternal.h"

//初始化vmm 并运行
bool vmm_init() // 启动全局VMM，并在每个逻辑处理器上进入VMX，完成全核接管
{

	//分配vmm上下文
	if (allocate_vmm_context() == false) // 先分配并初始化全局/每核所需资源，避免半初始化状态
	{
		outDebug("分配vmm上下文失败.\n"); // 资源不足或初始化失败
		return false; // 无法继续进入VMX流程
	}

	//我们需要在低于 DISPATCH_LEVEL 的 IRQL 下运行，以便 KeSetSystemAffinityThreadEx 立即生效
	NT_ASSERT(KeGetCurrentIrql() <= APC_LEVEL);	// 绑定线程亲和力要求低IRQL，否则无法切换到指定CPU

	// virtualize every cpu
	for (unsigned int iter = 0; iter < g_vmm_context.processor_count; iter++)
	{
		// restrict execution to the specified cpu
		auto const orig_affinity = KeSetSystemAffinityThreadEx(1ull << iter); // 将当前线程绑到目标CPU，确保初始化针对该核

		if (!initalize_vcpu(iter)) // 在该CPU上完成VMXON/VMCS初始化与VMM启动
		{
			KeRevertToUserAffinityThreadEx(orig_affinity); // 失败时先恢复线程亲和力，避免影响系统调度
			outDebug("initalize_vcpu失败.\n"); // 输出具体失败位置
			if (iter > 0)
			{
				hvgt::vmoff(iter);
			}
			free_vmm_context();
			return false; // 任一核失败即整体失败，避免部分核进入VMX
		}

		KeRevertToUserAffinityThreadEx(orig_affinity); // 恢复原始线程亲和力
	}
	return true; // 所有CPU成功进入VMX，VMM启动完成
}

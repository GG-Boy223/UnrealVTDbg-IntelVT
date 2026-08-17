#include "VmmInternal.h"

//初始化vmm 并运行
bool vmm_init() // 启动全局VMM，并在每个逻辑处理器上进入VMX，完成全核接管
{

	outFile("========== VMM启动入口：开始分配全局上下文和每CPU资源 ==========");
	//分配vmm上下文
	if (allocate_vmm_context() == false) // 先分配并初始化全局/每核所需资源，避免半初始化状态
	{
		outFile("[失败][VMM] 全局上下文/每CPU资源分配失败");
		return false; // 无法继续进入VMX流程
	}
	outFile("[成功][VMM] 上下文分配完成，逻辑处理器数量=%u", g_vmm_context.processor_count);

	//我们需要在低于 DISPATCH_LEVEL 的 IRQL 下运行，以便组亲和力切换立即生效
	NT_ASSERT(KeGetCurrentIrql() <= APC_LEVEL);	// 绑定线程亲和力要求低IRQL，否则无法切换到指定CPU
	outFile("[信息][VMM] 当前IRQL=%u", KeGetCurrentIrql());

	// virtualize every cpu
	for (unsigned int iter = 0; iter < g_vmm_context.processor_count; iter++)
	{
		// restrict execution to the specified cpu
		GROUP_AFFINITY orig_affinity = {};
		outFile("---------- CPU[%u] 开始进入VT：Group=%u Number=%u AffinityMask=0x%llX ----------",
			iter,
			g_vmm_context.vcpu[iter].processor_affinity.Group,
			g_vmm_context.vcpu[iter].processor_number.Number,
			static_cast<unsigned __int64>(g_vmm_context.vcpu[iter].processor_affinity.Mask));
		KeSetSystemGroupAffinityThread(&g_vmm_context.vcpu[iter].processor_affinity, &orig_affinity); // 将当前线程绑到目标CPU，确保初始化针对该核

		outFile("[进行中][CPU%u] 切换亲和性完成，开始执行VMX初始化链路", iter);
		if (!initalize_vcpu(iter)) // 在该CPU上完成VMXON/VMCS初始化与VMM启动
		{
			KeRevertToUserGroupAffinityThread(&orig_affinity); // 失败时先恢复线程亲和力，避免影响系统调度
			outFile("[失败][CPU%u] VMX初始化链路失败，开始回滚已进入VT的CPU", iter);
			g_vmm_context.lifecycle_state = hv::vmm_release_policy::lifecycle_state::stopping;
			if (iter > 0)
			{
				if (!hvgt::vmoff(iter))
				{
					vmx_fatal_error(vmx_fatal_vmxoff_broadcast, iter, g_vmm_context.processor_count);
				}
			}
			g_vmm_context.lifecycle_state = hv::vmm_release_policy::lifecycle_state::stopped;
			free_vmm_context();
			outFile("[完成][CPU%u] 回滚和资源释放结束", iter);
			return false; // 任一核失败即整体失败，避免部分核进入VMX
		}

		KeRevertToUserGroupAffinityThread(&orig_affinity); // 恢复原始线程亲和力
		outFile("[成功][CPU%u] VMX初始化链路完成", iter);
	}
	outFile("========== 所有逻辑处理器VT初始化完成 ==========");
	if (!Cr3TrackerStartWorker())
	{
		outFile("[失败][VMM] CR3候选处理线程创建失败，开始关闭全部VCPU");
		g_vmm_context.lifecycle_state = hv::vmm_release_policy::lifecycle_state::stopping;
		if (!hvgt::vmoff(g_vmm_context.processor_count))
		{
			vmx_fatal_error(
				vmx_fatal_vmxoff_broadcast,
				g_vmm_context.processor_count,
				0);
		}
		g_vmm_context.lifecycle_state = hv::vmm_release_policy::lifecycle_state::stopped;
		free_vmm_context();
		return false;
	}
	outFile("[成功][VMM] CR3候选处理线程已启动");
	g_vmm_context.lifecycle_state = hv::vmm_release_policy::lifecycle_state::running;
	return true; // 所有CPU成功进入VMX，VMM启动完成
}

#include "Driver.h"        // 驱动基础头文件，提供内核/类型/宏定义
#include "Globals.h"       // 提供 __vcpu 类型定义
#include "invalid_ept.h"   // INVEPT 描述符与接口声明
#include "AsmCallset.h"    // 汇编封装的 INVEPT 指令调用

// 刷新当前逻辑处理器上的所有 EPT 上下文
void invept_all_contexts_func() // 供上层统一刷新 EPT 缓存的入口，确保所有 EPT 映射变化生效
{
    __invept_descriptor descriptor = { 0 }; // 描述符置零，符合 INVEPT(all-context) 的格式要求
    __invept(invept_all_context, &descriptor); // 执行 INVEPT(all-context)，清空所有 EPT 派生缓存
}

// 刷新当前逻辑处理器上指定 EPTP 的 EPT 上下文
void invept_single_context_func(unsigned __int64 ept_pointer) // 仅刷新某个 EPTP，避免全局失效导致的性能开销
{
    __invept_descriptor descriptor = { 0 }; // 初始化描述符，避免残留位导致 #GP 异常
    descriptor.ept_pointer = ept_pointer; // 指定要失效的 EPTP（来自当前 VCPU 的 EPT 状态）
    descriptor.reserved = 0; // 保留位必须为 0，满足指令约束
    __invept(invept_single_context, &descriptor); // 仅无效化该 EPTP 上下文，加速刷新且减小扰动
}

void invept_current_context() // 刷新当前逻辑处理器唯一的 EPT 上下文（本核 EPTP）
{
	// vmx-root 态的 FSBASE 被设置为当前 vcpu 指针（见 idt.cpp 的用法）。
	__vcpu* const vcpu = reinterpret_cast<__vcpu*>(_readfsbase_u64());
	if (vcpu == nullptr ||
		vcpu->ept_state == nullptr ||
		vcpu->ept_state->ept_pointer == nullptr)
	{
		return; // 防御：EPT 尚未就绪时不刷新
	}

	invept_single_context_func(vcpu->ept_state->ept_pointer->all);
}

#include "Driver.h"        // 驱动基础头文件，提供内核/类型/宏定义
#include "invalid_ept.h"   // INVEPT 描述符与接口声明
#include "AsmCallset.h"    // 汇编封装的 INVEPT 指令调用

// 刷新全部 EPT 上下文（所有逻辑处理器）
void invept_all_contexts_func() // 供上层统一刷新 EPT 缓存的入口，确保所有 EPT 映射变化生效
{
    __invept_descriptor descriptor = { 0 }; // 描述符置零，符合 INVEPT(all-context) 的格式要求
    __invept(invept_all_context, &descriptor); // 执行 INVEPT(all-context)，清空所有 EPT 派生缓存
}

// 刷新指定 EPTP 的上下文（单一上下文无效化）
void invept_single_context_func(unsigned __int64 ept_pointer) // 仅刷新某个 EPTP，避免全局失效导致的性能开销
{
    __invept_descriptor descriptor = { 0 }; // 初始化描述符，避免残留位导致 #GP 异常
    descriptor.ept_pointer = ept_pointer; // 指定要失效的 EPTP（来自当前 VCPU 的 EPT 状态）
    descriptor.reserved = 0; // 保留位必须为 0，满足指令约束
    __invept(invept_single_context, &descriptor); // 仅无效化该 EPTP 上下文，加速刷新且减小扰动
}
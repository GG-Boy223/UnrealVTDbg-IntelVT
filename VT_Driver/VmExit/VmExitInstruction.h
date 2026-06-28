#pragma once
#include "VmExitCommon.h"

// CPUID 和时间戳指令
void vmexit_cpuid_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_rdtsc_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_rdtscp_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_xsetbv_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用

// 其他指令处理
void vmexit_invd_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_invlpg_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_vm_instruction(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_getsec_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_vmx_on_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_vmx_preemption_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用

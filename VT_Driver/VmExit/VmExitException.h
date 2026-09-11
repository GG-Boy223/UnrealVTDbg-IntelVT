#pragma once
#include "VmExitCommon.h"

// 异常和NMI相关处理
void vmexit_exception_or_nmi_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_nmi_window_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_monitor_trap_flag_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_init_signal_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_sipi_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_triple_fault_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_unimplemented(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_failed(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
bool vm_entry_event_pending();
bool reinject_idt_vectoring_event_if_needed(__vcpu* vcpu);

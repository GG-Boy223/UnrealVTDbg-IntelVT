#pragma once
#include "VmExitCommon.h"

// 异常和NMI相关处理
void vmexit_exception_or_nmi_handler(__vcpu* vcpu);
void vmexit_nmi_window_handler(__vcpu* vcpu);
void vmexit_monitor_trap_flag_handler(__vcpu* vcpu);
void vmexit_init_signal_handler(__vcpu* vcpu);
void vmexit_sipi_handler(__vcpu* vcpu);
void vmexit_triple_fault_handler(__vcpu* vcpu);
void vmexit_unimplemented(__vcpu* vcpu);
void vmexit_failed(__vcpu* vcpu);
bool vm_entry_event_pending();
bool reinject_idt_vectoring_event_if_needed(__vcpu* vcpu);

// 断点/调试处理
unsigned __int32 handler_breakpoint(__ept_state& ept_state, __vmexit_interrupt_info interrupt_info);
unsigned __int32 handler_debug(__ept_state& ept_state, __vmexit_interrupt_info interrupt_info);

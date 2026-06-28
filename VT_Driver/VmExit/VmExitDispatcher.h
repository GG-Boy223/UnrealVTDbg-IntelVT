#pragma once
#include "VmExitCommon.h"
#include "VmExitException.h"
#include "VmExitControl.h"
#include "VmExitEpt.h"
#include "VmExitMsr.h"
#include "VmExitIo.h"
#include "VmExitInstruction.h"

// 主分发器和入口函数
EXTERN_C bool vmexit_handler(guest_context* guest_registers, PFXSAVE64 fxsave);
void dispatch_vm_exit(__vcpu* vcpu); // 根据退出原因分发处理逻辑

// 仅在 Dispatcher 模块中定义的函数前置声明
void vmexit_nmi_window_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_vmcall_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_unimplemented(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_monitor_trap_flag_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_mov_dr_handler(__vcpu* vcpu);

#pragma once
#include "../Driver.h"
#include "../Globals.h"  // 包含 __vcpu 定义

// MSR 读写相关的 VM-Exit 处理函数
void vmexit_msr_read_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_msr_write_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用

#pragma once
#include "../Driver.h"
#include "../Globals.h"  // 包含 __vcpu 定义

// MSR 读写相关的 VM-Exit 处理函数
void vmexit_msr_read_handler(__vcpu* vcpu);
void vmexit_msr_write_handler(__vcpu* vcpu);

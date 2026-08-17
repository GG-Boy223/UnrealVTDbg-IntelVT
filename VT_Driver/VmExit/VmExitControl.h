#pragma once
#include "VmExitCommon.h"
#include "../Cr3/Cr3Tracker.h"

// 控制寄存器访问处理
void vmexit_cr_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_mov_dr_handler(__vcpu* vcpu);

// CR处理辅助函数
bool mov_to_cr3_handler(__vcpu* vcpu, unsigned __int64 guest_cr3);
bool mov_to_cr0_handler(__vcpu* vcpu, unsigned __int64 guest_cr0);
bool mov_to_cr4_handler(unsigned __int64 guest_cr4);
void emulate_lmsw(__vcpu* vcpu, uint16_t const value);

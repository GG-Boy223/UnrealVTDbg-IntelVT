#pragma once
#include "../Driver.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../vmexit_handler.h"
#include "../hypervisor_routines.h"
#include "../Vmcs/VmcsApi.h"
#include "../interrupt.h"
#include "../AsmCallset.h"
#include "../crx.h"
#include "../drx.h"
#include "../cpuid.h"
#include "../msr.h"
#include "../invalid_vpid.h"
#include "../invalid_pcid.h"
#include "../segment.h"

// 通用辅助函数
// 恢复来宾状态
void RestoreGuest(); // 将来宾寄存器状态恢复到继续执行
void set_hide_vm_exit_overhead(__vcpu* vcpu, bool value);
void hide_vm_exit_overhead(__vcpu* vcpu);
void skip_instruction(); // 直接跳过当前指令
// 调整 RIP 指向下一条指令
void adjust_rip(__vcpu* vcpu); // 更新来宾 RIP，跳过已执行指令
void CleanUp(__vcpu* vcpu, __ept_hooked_page_info* hooked_page_info);

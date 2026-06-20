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
void RestoreGuest();
void set_hide_vm_exit_overhead(__vcpu* vcpu, bool value);
void hide_vm_exit_overhead(__vcpu* vcpu);
void skip_instruction();
void adjust_rip(__vcpu* vcpu);
void CleanUp(__vcpu* vcpu, __ept_hooked_page_info* hooked_page_info);

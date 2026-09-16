#pragma once

#include "../Driver.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../cpuid.h"
#include "../ntapi.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../AllocateMem.h"
#include "../msr.h"
#include "../Vmcs/VmcsApi.h"
#include "../crx.h"
#include "../hypervisor_routines.h"
#include "../hypervisor_gateway.h"
#include "../vmm.h"
#include "../VmExit/VmxFatal.h"
#include "../Cr3/Cr3Tracker.h"

bool allocate_vmm_context();
//创建host页表
bool create_host_page_tables();
//分配vcpu结构内存
bool init_vcpu(__vcpu* vcpu);
//在目标逻辑处理器上构建该核的EPT页表
bool init_vcpu_ept(__vcpu* vcpu);
//扫描每个逻辑处理器的CPUID.80000008H，取全平台最大物理地址上界
bool query_platform_physical_address_limit();
void adjust_control_registers();
bool init_logical_processor2(unsigned int iter);
bool initalize_vcpu(unsigned int iter);

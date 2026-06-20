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

bool allocate_vmm_context();
bool create_host_page_tables();
bool init_vcpu(__vcpu* vcpu);
void adjust_control_registers();
void init_logical_processor2(unsigned int iter);
bool initalize_vcpu(unsigned int iter);

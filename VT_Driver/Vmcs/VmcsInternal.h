#pragma once

#include "../Driver.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../segment.h"
#include "../exception.h"
#include "../msr.h"
#include "../AsmCallset.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "VmcsApi.h"
#include "../hypervisor_routines.h"

void set_entry_control(__vmx_entry_control& entry_control);
void set_exit_control(__vmx_exit_control& exit_control);
void set_primary_controls(__vmx_primary_processor_based_control& primary_controls);
void set_secondary_controls(__vmx_secondary_processor_based_control& secondary_controls);
void set_exception_bitmap(__exception_bitmap& exception_bitmap);
void set_pinbased_control_msr(__vmx_pinbased_control_msr& pinbased_controls);
unsigned __int64 ajdust_controls(unsigned __int64 ctl, unsigned long msr);
uint64_t segment_base(__pseudo_descriptor64 const& gdtr, unsigned __int16 arg_selector);
vmx_segment_access_rights segment_access(unsigned __int8* gdt_base, unsigned __int16 selector);
void fill_vmcs_host_fields(__vcpu* vcpu);
void fill_vmcs_guest_fields(__vcpu* vcpu, void* guest_rsp);

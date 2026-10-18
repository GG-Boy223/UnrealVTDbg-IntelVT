#pragma once

enum vmx_fatal_reason : unsigned __int64
{
    vmx_fatal_vmresume_restore = 1,
    vmx_fatal_vmxoff_instruction = 2,
    vmx_fatal_vmxoff_broadcast = 3,
    vmx_fatal_vm_entry_failure = 4,
    vmx_fatal_triple_fault = 5,
    vmx_fatal_missing_vcpu = 6,
    vmx_fatal_unhandled_ept_violation = 7,
    vmx_fatal_ept_misconfiguration = 8,
    vmx_fatal_release_active_context = 9,
    vmx_fatal_vm_entry_event_queue_overflow = 10,
    vmx_fatal_unimplemented_vmexit_storm = 11,
    vmx_fatal_unhandled_host_exception = 12,
};

extern "C" __declspec(noreturn)
void vmx_fatal_error(
    unsigned __int64 reason,
    unsigned __int64 detail1,
    unsigned __int64 detail2);

extern "C"
unsigned __int64 handle_vmresume_failure(unsigned __int64 rflags);

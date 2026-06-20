#include "HypervisorInternal.h"

namespace hv
{
    volatile long vmcs_dump_lock = 0;

    void dump_vmcs()
    {
        spinlock::lock(&vmcs_dump_lock);

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "-----------------------------------VMCS CORE %u DUMP-----------------------------------\r\n", KeGetCurrentProcessorIndex());

        LogDump("GUEST_CR0: 0x%llX", vmread(GUEST_CR0));
        LogDump("GUEST_CR3: 0x%llX", vmread(GUEST_CR3));
        LogDump("GUEST_CR4: 0x%llX", vmread(GUEST_CR4));
        LogDump("GUEST_ES_BASE: 0x%llX", vmread(GUEST_ES_BASE));
        LogDump("GUEST_CS_BASE: 0x%llX", vmread(GUEST_CS_BASE));
        LogDump("GUEST_SS_BASE: 0x%llX", vmread(GUEST_SS_BASE));
        LogDump("GUEST_DS_BASE: 0x%llX", vmread(GUEST_DS_BASE));
        LogDump("GUEST_FS_BASE: 0x%llX", vmread(GUEST_FS_BASE));
        LogDump("GUEST_GS_BASE: 0x%llX", vmread(GUEST_GS_BASE));
        LogDump("GUEST_LDTR_BASE: 0x%llX", vmread(GUEST_LDTR_BASE));
        LogDump("GUEST_TR_BASE: 0x%llX", vmread(GUEST_TR_BASE));
        LogDump("GUEST_GDTR_BASE: 0x%llX", vmread(GUEST_GDTR_BASE));
        LogDump("GUEST_IDTR_BASE: 0x%llX", vmread(GUEST_IDTR_BASE));
        LogDump("GUEST_DR7: 0x%llX", vmread(GUEST_DR7));
        LogDump("GUEST_RSP: 0x%llX", vmread(GUEST_RSP));
        LogDump("GUEST_RIP: 0x%llX", vmread(GUEST_RIP));
        LogDump("GUEST_RFLAGS: 0x%llX", vmread(GUEST_RFLAGS));
        LogDump("GUEST_SYSENTER_ESP: 0x%llX", vmread(GUEST_SYSENTER_ESP));
        LogDump("GUEST_SYSENTER_EIP: 0x%llX", vmread(GUEST_SYSENTER_EIP));
        LogDump("GUEST_S_CET: 0x%llX", vmread(GUEST_S_CET));
        LogDump("GUEST_SSP: 0x%llX", vmread(GUEST_SSP));
        LogDump("GUEST_INTERRUPT_SSP_TABLE_ADDR: 0x%llX", vmread(GUEST_INTERRUPT_SSP_TABLE_ADDR));

        LogDump("GUEST_VMCS_LINK_POINTER: 0x%llX", vmread(VMCS_LINK_POINTER));
        LogDump("GUEST_DEBUG_CONTROL: 0x%llX", vmread(GUEST_DEBUG_CONTROL));
        LogDump("GUEST_PAT: 0x%llX", vmread(GUEST_PAT));
        LogDump("GUEST_EFER: 0x%llX", vmread(GUEST_EFER));
        LogDump("GUEST_PERF_GLOBAL_CONTROL: 0x%llX", vmread(GUEST_PERF_GLOBAL_CONTROL));
        LogDump("GUEST_PDPTE0: 0x%llX", vmread(GUEST_PDPTE0));
        LogDump("GUEST_PDPTE1: 0x%llX", vmread(GUEST_PDPTE1));
        LogDump("GUEST_PDPTE2: 0x%llX", vmread(GUEST_PDPTE2));
        LogDump("GUEST_PDPTE3: 0x%llX", vmread(GUEST_PDPTE3));
        LogDump("GUEST_BNDCFGS: 0x%llX", vmread(GUEST_BNDCFGS));
        LogDump("GUEST_RTIT_CTL: 0x%llX", vmread(GUEST_RTIT_CTL));
        LogDump("GUEST_PKRS: 0x%llX", vmread(GUEST_PKRS));

        LogDump("GUEST_ES_LIMIT: 0x%llX", vmread(GUEST_ES_LIMIT));
        LogDump("GUEST_CS_LIMIT: 0x%llX", vmread(GUEST_CS_LIMIT));
        LogDump("GUEST_SS_LIMIT: 0x%llX", vmread(GUEST_SS_LIMIT));
        LogDump("GUEST_DS_LIMIT: 0x%llX", vmread(GUEST_DS_LIMIT));
        LogDump("GUEST_FS_LIMIT: 0x%llX", vmread(GUEST_FS_LIMIT));
        LogDump("GUEST_GS_LIMIT: 0x%llX", vmread(GUEST_GS_LIMIT));
        LogDump("GUEST_LDTR_LIMIT: 0x%llX", vmread(GUEST_LDTR_LIMIT));
        LogDump("GUEST_TR_LIMIT: 0x%llX", vmread(GUEST_TR_LIMIT));
        LogDump("GUEST_GDTR_LIMIT: 0x%llX", vmread(GUEST_GDTR_LIMIT));
        LogDump("GUEST_IDTR_LIMIT: 0x%llX", vmread(GUEST_IDTR_LIMIT));
        LogDump("GUEST_ES_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_ES_ACCESS_RIGHTS));
        LogDump("GUEST_CS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_CS_ACCESS_RIGHTS));
        LogDump("GUEST_SS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_SS_ACCESS_RIGHTS));
        LogDump("GUEST_DS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_DS_ACCESS_RIGHTS));
        LogDump("GUEST_FS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_FS_ACCESS_RIGHTS));
        LogDump("GUEST_GS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_GS_ACCESS_RIGHTS));
        LogDump("GUEST_LDTR_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_LDTR_ACCESS_RIGHTS));
        LogDump("GUEST_TR_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_TR_ACCESS_RIGHTS));
        LogDump("GUEST_INTERRUPTIBILITY_STATE: 0x%llX", vmread(GUEST_INTERRUPTIBILITY_STATE));
        LogDump("GUEST_ACTIVITY_STATE: 0x%llX", vmread(GUEST_ACTIVITY_STATE));
        LogDump("GUEST_SMBASE: 0x%llX", vmread(GUEST_SM_BASE));
        LogDump("GUEST_SYSENTER_CS: 0x%llX", vmread(GUEST_SYSENTER_CS));
        LogDump("GUEST_VMX_PREEMPTION_TIMER_VALUE: 0x%llX", vmread(GUEST_VMX_PREEMPTION_TIMER_VALUE));

        LogDump("GUEST_ES_SELECTOR: 0x%llX", vmread(GUEST_ES_SELECTOR));
        LogDump("GUEST_CS_SELECTOR: 0x%llX", vmread(GUEST_CS_SELECTOR));
        LogDump("GUEST_SS_SELECTOR: 0x%llX", vmread(GUEST_SS_SELECTOR));
        LogDump("GUEST_DS_SELECTOR: 0x%llX", vmread(GUEST_DS_SELECTOR));
        LogDump("GUEST_FS_SELECTOR: 0x%llX", vmread(GUEST_FS_SELECTOR));
        LogDump("GUEST_GS_SELECTOR: 0x%llX", vmread(GUEST_GS_SELECTOR));
        LogDump("GUEST_LDTR_SELECTOR: 0x%llX", vmread(GUEST_LDTR_SELECTOR));
        LogDump("GUEST_TR_SELECTOR: 0x%llX", vmread(GUEST_TR_SELECTOR));
        LogDump("GUEST_GUEST_INTERRUPT_STATUS: 0x%llX", vmread(GUEST_INTERRUPT_STATUS));
        LogDump("GUEST_PML_INDEX: 0x%llX", vmread(GUEST_PML_INDEX));

        LogDump("HOST_CR0: 0x%llX", vmread(HOST_CR0));
        LogDump("HOST_CR3: 0x%llX", vmread(HOST_CR3));
        LogDump("HOST_CR4: 0x%llX", vmread(HOST_CR4));
        LogDump("HOST_FS_BASE: 0x%llX", vmread(HOST_FS_BASE));
        LogDump("HOST_GS_BASE: 0x%llX", vmread(HOST_GS_BASE));
        LogDump("HOST_TR_BASE: 0x%llX", vmread(HOST_TR_BASE));
        LogDump("HOST_GDTR_BASE: 0x%llX", vmread(HOST_GDTR_BASE));
        LogDump("HOST_IDTR_BASE: 0x%llX", vmread(HOST_IDTR_BASE));
        LogDump("HOST_SYSENTER_ESP: 0x%llX", vmread(HOST_IA32_SYSENTER_ESP));
        LogDump("HOST_SYSENTER_EIP: 0x%llX", vmread(HOST_IA32_SYSENTER_EIP));
        LogDump("HOST_RSP: 0x%llX", vmread(HOST_RSP));
        LogDump("HOST_RIP: 0x%llX", vmread(HOST_RIP));
        LogDump("HOST_S_CET: 0x%llX", vmread(Host_IA32_S_CET));
        LogDump("HOST_SSP: 0x%llX", vmread(HOST_SSP));
        LogDump("HOST_INTERRUPT_SSP_TABLE_ADDR: 0x%llX", vmread(Host_IA32_INTERRUPT_SSP_TABLE_ADDR));

        LogDump("HOST_PAT: 0x%llX", vmread(HOST_PAT));
        LogDump("HOST_EFER: 0x%llX", vmread(HOST_EFER));
        LogDump("HOST_PERF_GLOBAL_CTRL: 0x%llX", vmread(HOST_PERF_GLOBAL_CTRL));
        LogDump("HOST_PKRS: 0x%llX", vmread(HOST_PKRS));

        LogDump("HOST_SYSENTER_CS: 0x%llX", vmread(HOST_IA32_SYSENTER_CS));

        LogDump("HOST_ES_SELECTOR: 0x%llX", vmread(HOST_ES_SELECTOR));
        LogDump("HOST_CS_SELECTOR: 0x%llX", vmread(HOST_CS_SELECTOR));
        LogDump("HOST_SS_SELECTOR: 0x%llX", vmread(HOST_SS_SELECTOR));
        LogDump("HOST_DS_SELECTOR: 0x%llX", vmread(HOST_DS_SELECTOR));
        LogDump("HOST_FS_SELECTOR: 0x%llX", vmread(HOST_FS_SELECTOR));
        LogDump("HOST_GS_SELECTOR: 0x%llX", vmread(HOST_GS_SELECTOR));
        LogDump("HOST_TR_SELECTOR: 0x%llX", vmread(HOST_TR_SELECTOR));

        LogDump("CONTROL_CR0_GUEST_HOST_MASK: 0x%llX", vmread(CR0_GUEST_HOST_MASK));
        LogDump("CONTROL_CR4_GUEST_HOST_MASK: 0x%llX", vmread(CR4_GUEST_HOST_MASK));
        LogDump("CONTROL_CR0_READ_SHADOW: 0x%llX", vmread(CR0_READ_SHADOW));
        LogDump("CONTROL_CR4_READ_SHADOW: 0x%llX", vmread(CR4_READ_SHADOW));
        LogDump("CONTROL_CR3_TARGET_VALUE_0: 0x%llX", vmread(CR3_TARGET_VALUE0));
        LogDump("CONTROL_CR3_TARGET_VALUE_1: 0x%llX", vmread(CR3_TARGET_VALUE1));
        LogDump("CONTROL_CR3_TARGET_VALUE_2: 0x%llX", vmread(CR3_TARGET_VALUE2));
        LogDump("CONTROL_CR3_TARGET_VALUE_3: 0x%llX", vmread(CR3_TARGET_VALUE3));

        LogDump("CONTROL_BITMAP_IO_A_ADDRESS: 0x%llX", vmread(IO_BITMAP_A_ADDRESS));
        LogDump("CONTROL_BITMAP_IO_B_ADDRESS: 0x%llX", vmread(IO_BITMAP_B_ADDRESS));
        LogDump("CONTROL_MSR_BITMAPS_ADDRESS: 0x%llX", vmread(MSR_BITMAP_ADDRESS));
        LogDump("CONTROL_VMEXIT_MSR_STORE_ADDRESS: 0x%llX", vmread(VM_EXIT_MSR_STORE_ADDR));
        LogDump("CONTROL_VMEXIT_MSR_LOAD_ADDRESS: 0x%llX", vmread(VM_EXIT_MSR_LOAD_ADDR));
        LogDump("CONTROL_VMENTER_MSR_LOAD_ADDRESS: 0x%llX", vmread(VM_ENTRY_MSR_LOAD_ADDR));
        LogDump("CONTROL_VMCS_EXECUTIVE_POINTER: 0x%llX", vmread(VMCS_EXECUTIVE_POINTER));
        LogDump("CONTROL_PML_ADDRESS: 0x%llX", vmread(PML_ADDRESS));
        LogDump("CONTROL_TSC_OFFSET: 0x%llX", vmread(TSC_OFFSET));
        LogDump("CONTROL_VIRTUAL_APIC_ADDRESS: 0x%llX", vmread(VIRTUAL_APIC_PAGE_ADDR));
        LogDump("CONTROL_APIC_ACCESS_ADDRESS: 0x%llX", vmread(APIC_ACCESS_ADDRESS));
        LogDump("CONTROL_POSTED_INTERRUPT_DESCRIPTOR_ADDRESS: 0x%llX", vmread(POSTED_INTERRUPT_DESCRIPTOR_ADDRESS));
        LogDump("CONTROL_VM_FUNCTION_CONTROLS: 0x%llX", vmread(VM_FUNCTION_CONTROLS));
        LogDump("CONTROL_EPT_POINTER: 0x%llX", vmread(EPT_POINTER));
        LogDump("CONTROL_EOI_EXIT_BITMAP_0: 0x%llX", vmread(EOI_EXIT_BITMAP_0));
        LogDump("CONTROL_EOI_EXIT_BITMAP_1: 0x%llX", vmread(EOI_EXIT_BITMAP_1));
        LogDump("CONTROL_EOI_EXIT_BITMAP_2: 0x%llX", vmread(EOI_EXIT_BITMAP_2));
        LogDump("CONTROL_EOI_EXIT_BITMAP_3: 0x%llX", vmread(EOI_EXIT_BITMAP_3));
        LogDump("CONTROL_EPTP_LIST_ADDRESS: 0x%llX", vmread(EPTP_LIST_ADDRESS));
        LogDump("CONTROL_VMREAD_BITMAP_ADDRESS: 0x%llX", vmread(VMREAD_BITMAP_ADDRESS));
        LogDump("CONTROL_VMWRITE_BITMAP_ADDRESS: 0x%llX", vmread(VMWRITE_BITMAP_ADDRESS));
        LogDump("CONTROL_VIRTUALIZATION_EXCEPTION_INFORMATION_ADDRESS: 0x%llX", vmread(VIRTUALIZATION_EXCEPTION_INFORMATION_ADDRESS));
        LogDump("CONTROL_XSS_EXITING_BITMAP: 0x%llX", vmread(XSS_EXITING_BITMAP));
        LogDump("CONTROL_ENCLS_EXITING_BITMAP: 0x%llX", vmread(ENCLS_EXITING_BITMAP));
        LogDump("CONTROL_SUB_PAGE_PERMISSION_TABLE_POINTER: 0x%llX", vmread(SUB_PAGE_PERMISSION_TABLE_POINTER));
        LogDump("CONTROL_TSC_MULTIPLIER: 0x%llX", vmread(TSC_MULTIPLIER));
        LogDump("CONTROL_ENCLV_EXITING_BITMAP: 0x%llX", vmread(ENCLV_EXITING_BITMAP));

        LogDump("CONTROL_PIN_BASED_VM_EXECUTION_CONTROLS: 0x%llX", vmread(PIN_BASED_VM_EXEC_CONTROL));
        LogDump("CONTROL_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS: 0x%llX", vmread(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL));
        LogDump("CONTROL_EXCEPTION_BITMAP: 0x%llX", vmread(EXCEPTION_BITMAP));
        LogDump("CONTROL_PAGE_FAULT_ERROR_CODE_MASK: 0x%llX", vmread(PAGE_FAULT_ERROR_CODE_MASK));
        LogDump("CONTROL_PAGE_FAULT_ERROR_CODE_MATCH: 0x%llX", vmread(PAGE_FAULT_ERROR_CODE_MATCH));
        LogDump("CONTROL_CR3_TARGET_COUNT: 0x%llX", vmread(CR3_TARGET_COUNT));
        LogDump("CONTROL_VM_EXIT_CONTROLS: 0x%llX", vmread(VM_EXIT_CONTROLS));
        LogDump("CONTROL_VM_EXIT_MSR_STORE_COUNT: 0x%llX", vmread(VM_EXIT_MSR_STORE_COUNT));
        LogDump("CONTROL_VM_EXIT_MSR_LOAD_COUNT: 0x%llX", vmread(VM_EXIT_MSR_LOAD_COUNT));
        LogDump("CONTROL_VM_ENTRY_CONTROLS: 0x%llX", vmread(VM_ENTRY_CONTROLS));
        LogDump("CONTROL_VM_ENTRY_MSR_LOAD_COUNT: 0x%llX", vmread(VM_ENTRY_MSR_LOAD_COUNT));
        LogDump("CONTROL_VM_ENTRY_INTERRUPTION_INFORMATION_FIELD: 0x%llX", vmread(VM_ENTRY_INTERRUPTION_INFO_FIELD));
        LogDump("CONTROL_VM_ENTRY_EXCEPTION_ERROR_CODE: 0x%llX", vmread(VM_ENTRY_EXCEPTION_ERROR_CODE));
        LogDump("CONTROL_VM_ENTRY_INSTRUCTION_LENGTH: 0x%llX", vmread(VM_ENTRY_INSTRUCTION_LENGTH));
        LogDump("CONTROL_TPR_THRESHOLD: 0x%llX", vmread(TPR_THRESHOLD));
        LogDump("CONTROL_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS: 0x%llX", vmread(SECONDARY_PROCESSOR_BASED_VM_EXEC_CONTROL));
        LogDump("CONTROL_PLE_GAP: 0x%llX", vmread(CONTROL_PLE_GAP));
        LogDump("CONTROL_PLE_WINDOW: 0x%llX", vmread(CONTROL_PLE_WINDOW));

        LogDump("CONTROL_VIRTUAL_PROCESSOR_IDENTIFIER: 0x%llX", vmread(VIRTUAL_PROCESSOR_ID));
        LogDump("CONTROL_POSTED_INTERRUPT_NOTIFICATION_VECTOR: 0x%llX", vmread(POSTED_INTERRUPT_NOTIFICATION_VECTOR));
        LogDump("CONTROL_EPTP_INDEX: 0x%llX", vmread(EPTP_INDEX));

        LogDump("EXIT_QUALIFICATION: 0x%llX", vmread(EXIT_QUALIFICATION));
        LogDump("IO_RCX: 0x%llX", vmread(IO_RCX));
        LogDump("IO_RSI: 0x%llX", vmread(IO_RSI));
        LogDump("IO_RDI: 0x%llX", vmread(IO_RDI));
        LogDump("IO_RIP: 0x%llX", vmread(IO_RIP));
        LogDump("GUEST_LINEAR_ADDRESS: 0x%llX", vmread(GUEST_LINEAR_ADDRESS));

        LogDump("GUEST_PHYSICAL_ADDRESS: 0x%llX", vmread(GUEST_PHYSICAL_ADDRESS));

        LogDump("VM_INSTRUCTION_ERROR: 0x%llX", vmread(VM_INSTRUCTION_ERROR));
        LogDump("EXIT_REASON: 0x%llX", vmread(VM_EXIT_REASON));
        LogDump("VM_EXIT_INTERRUPTION_INFORMATION: 0x%llX", vmread(VM_EXIT_INTERRUPTION_INFORMATION));
        LogDump("VM_EXIT_INTERRUPTION_ERROR_CODE: 0x%llX", vmread(VM_EXIT_INTERRUPTION_ERROR_CODE));
        LogDump("IDT_VECTORING_INFORMATION_FIELD: 0x%llX", vmread(IDT_VECTORING_INFO_FIELD));
        LogDump("IDT_VECTORING_ERROR_CODE: 0x%llX", vmread(IDT_VECTORING_ERROR_CODE));
        LogDump("VM_EXIT_INSTRUCTION_LENGTH: 0x%llX", vmread(VM_EXIT_INSTRUCTION_LENGTH));
        LogDump("VM_EXIT_INSTRUCTION_INFORMATION: 0x%llX", vmread(VM_EXIT_INSTRUCTION_INFORMATION));

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "-----------------------------------VMCS CORE %u DUMP-----------------------------------\r\n", KeGetCurrentProcessorIndex());

        spinlock::unlock(&vmcs_dump_lock);
    }
}

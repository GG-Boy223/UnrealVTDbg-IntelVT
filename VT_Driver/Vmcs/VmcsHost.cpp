#include "VmcsInternal.h"

void fill_vmcs_host_fields(__vcpu* vcpu)
{
    hv::vmwrite<unsigned __int64>(HOST_CS_SELECTOR, hv::host_cs_selector.flags);
    hv::vmwrite<unsigned __int64>(HOST_SS_SELECTOR, 0);
    hv::vmwrite<unsigned __int64>(HOST_DS_SELECTOR, 0);
    hv::vmwrite<unsigned __int64>(HOST_ES_SELECTOR, 0);
    hv::vmwrite<unsigned __int64>(HOST_FS_SELECTOR, 0);
    hv::vmwrite<unsigned __int64>(HOST_GS_SELECTOR, 0);
    hv::vmwrite<unsigned __int64>(HOST_TR_SELECTOR, hv::host_tr_selector.flags);

    hv::vmwrite<unsigned __int64>(HOST_FS_BASE, reinterpret_cast<size_t>(vcpu));
    hv::vmwrite<unsigned __int64>(HOST_GS_BASE, 0);
    hv::vmwrite<unsigned __int64>(HOST_TR_BASE, reinterpret_cast<size_t>(&vcpu->host_tss));
    hv::vmwrite<unsigned __int64>(HOST_GDTR_BASE, reinterpret_cast<size_t>(&vcpu->host_gdt));
    hv::vmwrite<unsigned __int64>(HOST_IDTR_BASE, reinterpret_cast<size_t>(&vcpu->host_idt));

    hv::vmwrite<unsigned __int64>(HOST_CR0, __readcr0());

    cr3 host_cr3;
    host_cr3.flags = 0;
    host_cr3.page_level_cache_disable = 0;
    host_cr3.page_level_write_through = 0;
    host_cr3.address_of_page_directory = MmGetPhysicalAddress(&hv::ghv.host_page_tables.pml4).QuadPart >> 12;
    hv::vmwrite<unsigned __int64>(HOST_CR3, host_cr3.flags);

    cr4 host_cr4;
    host_cr4.flags = __readcr4();
    host_cr4.fsgsbase_enable = 1;
    host_cr4.os_xsave = 1;
    host_cr4.smap_enable = 0;
    host_cr4.smep_enable = 0;
    hv::vmwrite<unsigned __int64>(HOST_CR4, host_cr4.flags);

    auto const rsp = ((reinterpret_cast<size_t>(vcpu->host_stack) + VMM_STACK_SIZE) & ~0b1111ull) - 8;
    hv::vmwrite<unsigned __int64>(HOST_RSP, rsp);
    hv::vmwrite<void*>(HOST_RIP, hv::vm_exit);

    hv::vmwrite<unsigned __int64>(HOST_IA32_SYSENTER_CS, 0);
    hv::vmwrite<unsigned __int64>(HOST_IA32_SYSENTER_ESP, 0);
    hv::vmwrite<unsigned __int64>(HOST_IA32_SYSENTER_EIP, 0);
    hv::vmwrite<unsigned __int64>(HOST_EFER, __readmsr(IA32_EFER));

    ia32_pat_register host_pat;
    host_pat.flags = 0;
    host_pat.pa0 = MEMORY_TYPE_WRITE_BACK;
    host_pat.pa1 = MEMORY_TYPE_WRITE_THROUGH;
    host_pat.pa2 = MEMORY_TYPE_UNCACHEABLE_MINUS;
    host_pat.pa3 = MEMORY_TYPE_UNCACHEABLE;
    host_pat.pa4 = MEMORY_TYPE_WRITE_BACK;
    host_pat.pa5 = MEMORY_TYPE_WRITE_THROUGH;
    host_pat.pa6 = MEMORY_TYPE_UNCACHEABLE_MINUS;
    host_pat.pa7 = MEMORY_TYPE_UNCACHEABLE;
    hv::vmwrite<unsigned __int64>(HOST_PAT, host_pat.flags);

    hv::vmwrite<unsigned __int64>(HOST_PERF_GLOBAL_CTRL, 0);
}

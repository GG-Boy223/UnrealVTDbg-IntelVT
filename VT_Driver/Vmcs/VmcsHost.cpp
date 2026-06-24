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
    outFile("[信息][VMCS Host] HOST_CR3=0x%llX HostPML4_PA=0x%llX", host_cr3.flags, host_cr3.address_of_page_directory << 12);

    constexpr unsigned __int64 cr4_cet_bit = 1ull << 23;
    constexpr unsigned __int64 cr4_pks_bit = 1ull << 24;
    auto const raw_host_cr4 = __readcr4();
    auto const raw_la57_enabled = (raw_host_cr4 & (1ull << 12)) != 0;
    auto const raw_cet_enabled = (raw_host_cr4 & cr4_cet_bit) != 0;
    auto const raw_pks_enabled = (raw_host_cr4 & cr4_pks_bit) != 0;

    cr4 host_cr4;
    // host_cr4.flags = __readcr4(); // 旧逻辑：直接复制CR4会把LA57/CET/PKS带入自建Host页表/Host栈，部分新平台首次VM-exit会卡死。
    host_cr4.flags = raw_host_cr4;
    host_cr4.linear_addresses_57_bit = 0; // 当前dwm.exe Host页表链路构造的是4级PML4，HOST_CR4必须与4级页表匹配。
    host_cr4.flags &= ~cr4_cet_bit; // VMX Root Host栈没有配置shadow stack，清Host CET不改变Guest CET/VT功能。
    host_cr4.flags &= ~cr4_pks_bit; // 未启用VM-exit load PKRS时清Host PKS，避免继承Guest PKRS造成Host页访问异常。
    host_cr4.fsgsbase_enable = 1;
    host_cr4.os_xsave = 1;
    host_cr4.smap_enable = 0;
    host_cr4.smep_enable = 0;
    __msr fixed0 = { 0 };
    __msr fixed1 = { 0 };
    fixed0.all = __readmsr(IA32_VMX_CR4_FIXED0);
    fixed1.all = __readmsr(IA32_VMX_CR4_FIXED1);
    host_cr4.flags |= fixed0.all; // 按VMX fixed0/fixed1收敛HOST_CR4，保持硬件必要条件不变。
    host_cr4.flags &= fixed1.all;
    hv::vmwrite<unsigned __int64>(HOST_CR4, host_cr4.flags);
    outFile("[信息][VMCS Host] HOST_CR4=0x%llX RawCR4=0x%llX 原始LA57=%u 原始CET=%u 原始PKS=%u",
        host_cr4.flags,
        raw_host_cr4,
        raw_la57_enabled ? 1 : 0,
        raw_cet_enabled ? 1 : 0,
        raw_pks_enabled ? 1 : 0);
    if (raw_la57_enabled)
    {
        outFile("[警告][VMCS Host] 原始CR4.LA57=1，但当前Host页表为4级PML4；已仅在VMX Root Host状态清除LA57，Guest状态不变");
    }
    if (raw_cet_enabled || raw_pks_enabled)
    {
        outFile("[警告][VMCS Host] 原始CR4包含CET/PKS：已仅在VMX Root Host状态清除未配套的CET/PKS，避免Host栈/PKRS异常，Guest状态不变");
    }

    auto const rsp = ((reinterpret_cast<size_t>(vcpu->host_stack) + VMM_STACK_SIZE) & ~0b1111ull) - 8;
    hv::vmwrite<unsigned __int64>(HOST_RSP, rsp);
    hv::vmwrite<void*>(HOST_RIP, hv::vm_exit);
    outFile("[信息][VMCS Host] HOST_RSP=0x%llX HOST_RIP=0x%llX HOST_GDTR=0x%llX HOST_IDTR=0x%llX",
        rsp,
        reinterpret_cast<unsigned __int64>(hv::vm_exit),
        reinterpret_cast<unsigned __int64>(&vcpu->host_gdt),
        reinterpret_cast<unsigned __int64>(&vcpu->host_idt));

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

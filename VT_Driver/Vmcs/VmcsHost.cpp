#include "VmcsInternal.h"

// VM-exit 发生时，CPU 会从 host-state 区域加载宿主寄存器状态
void fill_vmcs_host_fields(__vcpu* vcpu) // 填充宿主字段
{
    // 设置宿主段选择子（使用宿主自己的 GDT）
    hv::vmwrite<unsigned __int64>(HOST_CS_SELECTOR, hv::host_cs_selector.flags); // 写宿主 CS
    hv::vmwrite<unsigned __int64>(HOST_SS_SELECTOR, 0); // 写宿主 SS（空选择子）
    hv::vmwrite<unsigned __int64>(HOST_DS_SELECTOR, 0); // 写宿主 DS（空选择子）
    hv::vmwrite<unsigned __int64>(HOST_ES_SELECTOR, 0); // 写宿主 ES（空选择子）
    hv::vmwrite<unsigned __int64>(HOST_FS_SELECTOR, 0); // 写宿主 FS（空选择子）
    hv::vmwrite<unsigned __int64>(HOST_GS_SELECTOR, 0); // 写宿主 GS（空选择子）
    hv::vmwrite<unsigned __int64>(HOST_TR_SELECTOR, hv::host_tr_selector.flags); // 写宿主 TR

    // 设置宿主段基址
    hv::vmwrite<unsigned __int64>(HOST_FS_BASE, reinterpret_cast<size_t>(vcpu)); // 用 FS 指向 vCPU 结构
    const unsigned __int64 host_gs_base = __readmsr(IA32_GS_BASE); // 当前初始化线程已绑定目标CPU且处于内核态，此值是该CPU的KPCR。
    hv::vmwrite<unsigned __int64>(HOST_GS_BASE, host_gs_base); // 保留Windows x64内核的GS/KPCR语义，Root中调用内核例程时必须有效。
    hv::vmwrite<unsigned __int64>(HOST_TR_BASE, reinterpret_cast<size_t>(&vcpu->host_tss)); // TR 指向宿主 TSS
    hv::vmwrite<unsigned __int64>(HOST_GDTR_BASE, reinterpret_cast<size_t>(&vcpu->host_gdt)); // GDTR 指向宿主 GDT
    hv::vmwrite<unsigned __int64>(HOST_IDTR_BASE, reinterpret_cast<size_t>(&vcpu->host_idt)); // IDTR 指向宿主 IDT

    // 设置宿主控制寄存器
    hv::vmwrite<unsigned __int64>(HOST_CR0, __readcr0()); // 写入宿主 CR0

    cr3 host_cr3; // 定义 CR3 结构
    host_cr3.flags = 0; // 清零
    host_cr3.page_level_cache_disable = 0; // 缓存不禁用
    host_cr3.page_level_write_through = 0; // 不使用写穿透
    host_cr3.address_of_page_directory = MmGetPhysicalAddress(&hv::ghv.host_page_tables.pml4).QuadPart >> 12; // 设置宿主 PML4
    hv::vmwrite<unsigned __int64>(HOST_CR3, host_cr3.flags); // 写入宿主 CR3
    outFile("[信息][VMCS Host] HOST_CR3=0x%llX HostPML4_PA=0x%llX", host_cr3.flags, host_cr3.address_of_page_directory << 12);

    constexpr unsigned __int64 cr4_cet_bit = 1ull << 23;
    constexpr unsigned __int64 cr4_pks_bit = 1ull << 24;
    constexpr unsigned __int64 cr4_pcid_bit = 1ull << 17;
    auto const raw_host_cr4 = __readcr4();
    auto const raw_la57_enabled = (raw_host_cr4 & (1ull << 12)) != 0;
    auto const raw_cet_enabled = (raw_host_cr4 & cr4_cet_bit) != 0;
    auto const raw_pks_enabled = (raw_host_cr4 & cr4_pks_bit) != 0;
    auto const raw_pcid_enabled = (raw_host_cr4 & cr4_pcid_bit) != 0;

    cr4 host_cr4; // 定义 CR4 结构
    // host_cr4.flags = __readcr4(); // 旧逻辑：直接复制CR4会把LA57/CET/PKS带入自建Host页表/Host栈，部分新平台首次VM-exit会卡死。
    host_cr4.flags = raw_host_cr4;
    host_cr4.linear_addresses_57_bit = 0; // 当前dwm.exe Host页表链路构造的是4级PML4，HOST_CR4必须与4级页表匹配。
    host_cr4.flags &= ~cr4_cet_bit; // VMX Root Host栈没有配置shadow stack，清Host CET不改变Guest CET/VT功能。
    host_cr4.flags &= ~cr4_pks_bit; // 未启用VM-exit load PKRS时清Host PKS，避免继承Guest PKRS造成Host页访问异常。
    host_cr4.flags &= ~cr4_pcid_bit; // HOST_CR3固定使用PCID=0；Host不能继承Windows的PCIDE，否则guest与root的TLB条目可能按同一PCID别名。


    __msr fixed0 = { 0 };
    __msr fixed1 = { 0 };
    fixed0.all = __readmsr(IA32_VMX_CR4_FIXED0);
    fixed1.all = __readmsr(IA32_VMX_CR4_FIXED1);
    host_cr4.flags |= fixed0.all; // 按VMX fixed0/fixed1收敛HOST_CR4，保持硬件必要条件不变。
    host_cr4.flags &= fixed1.all;

    // 对齐 HvVmxCore Core Ultra 修复：不要清零 SMEP/SMAP。
    // 新平台（Core Ultra）CR4 fixed1 可能要求 SMEP=1，清零会直接 VM-entry failure；
    // 且 host 模式只访问内核地址，保留 SMEP/SMAP 原值不影响 root 运行。


    hv::vmwrite<unsigned __int64>(HOST_CR4, host_cr4.flags); // 写入宿主 CR4
    outFile("[信息][VMCS Host] HOST_CR4=0x%llX RawCR4=0x%llX 原始LA57=%u 原始CET=%u 原始PKS=%u 原始PCIDE=%u",
        host_cr4.flags,
        raw_host_cr4,
        raw_la57_enabled ? 1 : 0,
        raw_cet_enabled ? 1 : 0,
        raw_pks_enabled ? 1 : 0,
        raw_pcid_enabled ? 1 : 0);
    if (raw_la57_enabled)
    {
        outFile("[警告][VMCS Host] 原始CR4.LA57=1，但当前Host页表为4级PML4；已仅在VMX Root Host状态清除LA57，Guest状态不变");
    }
    if (raw_cet_enabled || raw_pks_enabled)
    {
        outFile("[警告][VMCS Host] 原始CR4包含CET/PKS：已仅在VMX Root Host状态清除未配套的CET/PKS，避免Host栈/PKRS异常，Guest状态不变");
    }
    if (raw_pcid_enabled)
    {
        outFile("[信息][VMCS Host] 原始CR4.PCIDE=1，已在VMX Root Host状态清除；HOST_CR3的PCID固定为0，Guest状态不受影响");
    }

    // 设置宿主栈指针，保证 16 字节对齐
    auto const rsp = ((reinterpret_cast<size_t>(vcpu->host_stack) + VMM_STACK_SIZE) & ~0b1111ull) - 8; // 计算对齐后的 RSP
    hv::vmwrite<unsigned __int64>(HOST_RSP, rsp); // 写入宿主 RSP
    hv::vmwrite<void*>(HOST_RIP, hv::vm_exit); // 写入宿主 RIP（VM-exit 入口）
    outFile("[信息][VMCS Host] HOST_RSP=0x%llX HOST_RIP=0x%llX HOST_GS_BASE=0x%llX HOST_GDTR=0x%llX HOST_IDTR=0x%llX",
        rsp,
        reinterpret_cast<unsigned __int64>(hv::vm_exit),
        host_gs_base,
        reinterpret_cast<unsigned __int64>(&vcpu->host_gdt),
        reinterpret_cast<unsigned __int64>(&vcpu->host_idt));

    // 设置宿主 MSR
    hv::vmwrite<unsigned __int64>(HOST_IA32_SYSENTER_CS, 0); // SYSENTER CS
    hv::vmwrite<unsigned __int64>(HOST_IA32_SYSENTER_ESP, 0); // SYSENTER ESP
    hv::vmwrite<unsigned __int64>(HOST_IA32_SYSENTER_EIP, 0); // SYSENTER EIP
    hv::vmwrite<unsigned __int64>(HOST_EFER, __readmsr(IA32_EFER)); // 写入宿主 EFER

    // VM-exit 已启用 load IA32_PAT，Host PAT 必须精确继承当前 Windows 状态，不能用假定的固定模板覆盖。
    hv::vmwrite<unsigned __int64>(HOST_PAT, __readmsr(IA32_PAT));

    // 对齐 HvVmxCore Core Ultra 修复：HOST_PERF_GLOBAL_CTRL 不能写 0。
    // VM-exit 后系统性能计数器全关，ETW/性能监控/显卡计时都依赖它，写 0 会加重 TDR。
    hv::vmwrite<unsigned __int64>(HOST_PERF_GLOBAL_CTRL, __readmsr(IA32_PERF_GLOBAL_CTRL)); // 继承当前性能计数控制
}

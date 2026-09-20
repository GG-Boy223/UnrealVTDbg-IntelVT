#include "VmcsInternal.h"

// 填充来宾状态字段（guest-state）
void fill_vmcs_guest_fields(__vcpu* vcpu, void* guest_rsp) // 填充来宾字段
{
    __pseudo_descriptor64 gdtr = { 0 }; // GDT 伪描述符
    __pseudo_descriptor64 idtr = { 0 }; // IDT 伪描述符

    __sgdt(&gdtr); // 读取 GDTR
    __sidt(&idtr); // 读取 IDTR

    // 写入来宾 GDTR/IDTR 的界限与基址
    hv::vmwrite<unsigned __int64>(GUEST_GDTR_LIMIT, gdtr.limit); // GDTR 界限
    hv::vmwrite<unsigned __int64>(GUEST_IDTR_LIMIT, idtr.limit); // IDTR 界限
    hv::vmwrite<unsigned __int64>(GUEST_GDTR_BASE, gdtr.base_address); // GDTR 基址
    hv::vmwrite<unsigned __int64>(GUEST_IDTR_BASE, idtr.base_address); // IDTR 基址

    // 写入来宾段选择子
    hv::vmwrite<unsigned __int64>(GUEST_CS_SELECTOR, __read_cs()); // CS
    hv::vmwrite<unsigned __int64>(GUEST_SS_SELECTOR, __read_ss()); // SS
    hv::vmwrite<unsigned __int64>(GUEST_DS_SELECTOR, __read_ds()); // DS
    hv::vmwrite<unsigned __int64>(GUEST_ES_SELECTOR, __read_es()); // ES
    hv::vmwrite<unsigned __int64>(GUEST_FS_SELECTOR, __read_fs()); // FS
    hv::vmwrite<unsigned __int64>(GUEST_GS_SELECTOR, __read_gs()); // GS
    hv::vmwrite<unsigned __int64>(GUEST_TR_SELECTOR, __read_tr()); // TR
    hv::vmwrite<unsigned __int64>(GUEST_LDTR_SELECTOR, __read_ldtr()); // LDTR

    // 写入来宾段基址（使用通用函数计算）
    hv::vmwrite<unsigned __int64>(GUEST_CS_BASE, segment_base(gdtr, __read_cs())); // CS 基址
    hv::vmwrite<unsigned __int64>(GUEST_SS_BASE, segment_base(gdtr, __read_ss())); // SS 基址
    hv::vmwrite<unsigned __int64>(GUEST_DS_BASE, segment_base(gdtr, __read_ds())); // DS 基址
    hv::vmwrite<unsigned __int64>(GUEST_ES_BASE, segment_base(gdtr, __read_es())); // ES 基址
    hv::vmwrite<unsigned __int64>(GUEST_FS_BASE, __readmsr(IA32_FS_BASE)); // FS 基址
    hv::vmwrite<unsigned __int64>(GUEST_GS_BASE, __readmsr(IA32_GS_BASE)); // GS 基址
    hv::vmwrite<unsigned __int64>(GUEST_TR_BASE, segment_base(gdtr, __read_tr())); // TR 基址
    hv::vmwrite<unsigned __int64>(GUEST_LDTR_BASE, segment_base(gdtr, __read_ldtr())); // LDTR 基址

    // 写入来宾段界限
    hv::vmwrite<unsigned __int64>(GUEST_CS_LIMIT, __segmentlimit(__read_cs())); // CS limit
    hv::vmwrite<unsigned __int64>(GUEST_SS_LIMIT, __segmentlimit(__read_ss())); // SS limit
    hv::vmwrite<unsigned __int64>(GUEST_DS_LIMIT, __segmentlimit(__read_ds())); // DS limit
    hv::vmwrite<unsigned __int64>(GUEST_ES_LIMIT, __segmentlimit(__read_es())); // ES limit
    hv::vmwrite<unsigned __int64>(GUEST_FS_LIMIT, __segmentlimit(__read_fs())); // FS limit
    hv::vmwrite<unsigned __int64>(GUEST_GS_LIMIT, __segmentlimit(__read_gs())); // GS limit
    hv::vmwrite<unsigned __int64>(GUEST_TR_LIMIT, __segmentlimit(__read_tr())); // TR limit
    hv::vmwrite<unsigned __int64>(GUEST_LDTR_LIMIT, __segmentlimit(__read_ldtr())); // LDTR limit

    // 写入来宾段访问权限
    hv::vmwrite<unsigned __int64>(GUEST_CS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_cs()).flags); // CS 权限
    hv::vmwrite<unsigned __int64>(GUEST_SS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_ss()).flags); // SS 权限
    hv::vmwrite<unsigned __int64>(GUEST_DS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_ds()).flags); // DS 权限
    hv::vmwrite<unsigned __int64>(GUEST_ES_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_es()).flags); // ES 权限
    hv::vmwrite<unsigned __int64>(GUEST_FS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_fs()).flags); // FS 权限
    hv::vmwrite<unsigned __int64>(GUEST_GS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_gs()).flags); // GS 权限
    hv::vmwrite<unsigned __int64>(GUEST_TR_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_tr()).flags); // TR 权限
    hv::vmwrite<unsigned __int64>(GUEST_LDTR_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_ldtr()).flags); // LDTR 权限

    // 写入 CR0/CR3/CR4 及相关掩码/影子
    hv::vmwrite<unsigned __int64>(GUEST_CR0, __readcr0()); // 来宾 CR0
    hv::vmwrite<unsigned __int64>(CR0_READ_SHADOW, __readcr0()); // CR0 读影子
    hv::vmwrite<unsigned __int64>(CR0_GUEST_HOST_MASK, 0xFFFFFFFF'FFFFFFFF);
    const unsigned __int64 initialCr3 = __readcr3();
    hv::vmwrite<unsigned __int64>(GUEST_CR3, initialCr3);
    hv::vmwrite<unsigned __int64>(CR3_TARGET_COUNT, 0);
    hv::vmwrite<unsigned __int64>(CR3_TARGET_VALUE0, 0);
    hv::vmwrite<unsigned __int64>(CR3_TARGET_VALUE1, 0);
    hv::vmwrite<unsigned __int64>(CR3_TARGET_VALUE2, 0);
    hv::vmwrite<unsigned __int64>(CR3_TARGET_VALUE3, 0);
    RtlZeroMemory(vcpu->cr3_target_values, sizeof(vcpu->cr3_target_values));
    vcpu->cr3_target_count = 0;
    vcpu->cr3_target_cursor = 0;
    hv::vmwrite<unsigned __int64>(GUEST_CR4, __readcr4()); // 来宾 CR4
    hv::vmwrite<unsigned __int64>(CR4_READ_SHADOW, __readcr4() & ~0x2000); // CR4 影子值（屏蔽 VMXE）
    // 监视 VMXE 与 LA57。当前来宾地址翻译和 Host 页表均为 4 级；
    // 若来宾尝试开启 LA57，必须进入 mov_to_cr4_handler 按现有规则拒绝。
    constexpr unsigned __int64 cr4_vmxe_bit = 1ull << 13;
    constexpr unsigned __int64 cr4_la57_bit = 1ull << 12;
    hv::vmwrite<unsigned __int64>(CR4_GUEST_HOST_MASK, cr4_vmxe_bit | cr4_la57_bit);

    // 写入来宾调试寄存器
    hv::vmwrite<unsigned __int64>(GUEST_DR7, __readdr(7)); // DR7
    // 写入来宾 RFLAGS
    hv::vmwrite<unsigned __int64>(GUEST_RFLAGS, __readeflags()); // RFLAGS
    // 来宾 RSP/RIP 在 vmlaunch 前由汇编代码设置
    hv::vmwrite<void*>(GUEST_RSP, 0); // 预留，由汇编初始化
    hv::vmwrite<void*>(GUEST_RIP, 0); // 预留，由汇编初始化

    // 写入来宾 MSR
    hv::vmwrite<unsigned __int64>(GUEST_DEBUG_CONTROL, 0); // 关闭来宾调试控制
    hv::vmwrite<unsigned __int64>(GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS)); // SYSENTER CS
    hv::vmwrite<unsigned __int64>(GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP)); // SYSENTER ESP
    hv::vmwrite<unsigned __int64>(GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP)); // SYSENTER EIP
    hv::vmwrite<unsigned __int64>(GUEST_PAT, __readmsr(IA32_PAT)); // PAT
    hv::vmwrite<unsigned __int64>(GUEST_PERF_GLOBAL_CONTROL, __readmsr(IA32_PERF_GLOBAL_CTRL)); // 性能控制
    hv::vmwrite<unsigned __int64>(GUEST_EFER, __readmsr(IA32_EFER)); // EFER

    // 来宾活动与中断状态
    hv::vmwrite<unsigned __int64>(GUEST_ACTIVITY_STATE, vmx_active); // 活动状态
    hv::vmwrite<unsigned __int64>(GUEST_INTERRUPTIBILITY_STATE, 0); // 中断阻塞状态
    hv::vmwrite<unsigned __int64>(GUEST_SM_BASE, 0); // SMBASE
    hv::vmwrite<unsigned __int64>(GUEST_PENDING_DEBUG_EXCEPTION, 0); // 挂起调试异常
    hv::vmwrite<unsigned __int64>(GUEST_VMX_PREEMPTION_TIMER_VALUE, MAXULONG64); // 预抢占计时器
}

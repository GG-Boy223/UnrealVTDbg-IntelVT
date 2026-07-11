#pragma once

#include "GlobalsEptHookTypes.h"

struct __vcpu // 每逻辑 CPU 的 VMM 状态
{
    // host task state segment
    alignas(0x1000) task_state_segment_64 host_tss; // 宿主 TSS（对齐页）

    // host global descriptor table
    alignas(0x1000) segment_descriptor_32 host_gdt[HOST_GDT_DESCRIPTOR_COUNT]; // 宿主 GDT

    // host interrupt descriptor table
    alignas(0x1000) segment_descriptor_interrupt_gate_64 host_idt[HOST_IDT_DESCRIPTOR_COUNT]; // 宿主 IDT

    alignas(0x1000) vmcs vmcs; // VMCS 区域

    alignas(0x1000) vmxon vmxon; // VMXON 区域

    alignas(0x1000) uint8_t host_stack[VMM_STACK_SIZE]; // 宿主栈

    alignas(0x1000) vmx_msr_bitmap msr_bitmap; // MSR 位图

    // the number of NMIs that need to be delivered
    // 需要交付的 NMI 数量
    uint32_t volatile queued_nmis; // 待注入 NMI 数量

    // current preemption timer
    uint64_t preemption_timer; // VMX 抢占计时器

    bool hide_vm_exit_overhead; // 是否隐藏 VM-Exit 开销
    uint64_t tsc_offset; // TSC 偏移量

    // the overhead caused by world-transitions
    uint64_t vm_exit_tsc_overhead; // VM-Exit TSC 开销
    uint64_t vm_exit_mperf_overhead; // VM-Exit MPERF 开销
    uint64_t vm_exit_ref_tsc_overhead; // VM-Exit 参考 TSC 开销

    cr3 target_cr3; // 目标 CR3（用于地址空间切换）

    //void* vmm_stack;

    // vm-exit MSR store area
    struct alignas(0x10)
    {
        vmx_msr_entry tsc; // 保存 TSC
        vmx_msr_entry perf_global_ctrl; // 保存 PERF_GLOBAL_CTRL
        vmx_msr_entry aperf; // 保存 APERF
        vmx_msr_entry mperf; // 保存 MPERF
    } msr_exit_store; // VM-Exit MSR 保存区

    // vm-entry MSR load area
    struct alignas(0x10)
    {
        vmx_msr_entry aperf; // 载入 APERF
        vmx_msr_entry mperf; // 载入 MPERF
    } msr_entry_load; // VM-Entry MSR 加载区

    struct __vmexit_info
    {
        guest_context* guest_registers; // 指向 guest 寄存器上下文
        PFXSAVE64 fxsave; // 指向 FXSAVE 区域

        unsigned __int64 guest_rip; // 来宾 RIP

        __rflags guest_rflags; // 来宾 RFLAGS

        unsigned __int64 instruction_length; // VM-Exit 指令长度

        unsigned __int64 reason;  //vmexit原因

        unsigned __int64 qualification; // Exit Qualification

        unsigned __int64 instruction_information; // Exit Instruction Info

        unsigned __int64 instruction_error;  //记录VMfailValid失败原因

    }vmexit_info; // VM-Exit 信息

    struct __host_exception_snapshot
    {
        volatile unsigned __int64 count; // Host异常累计次数：只做诊断，不参与VT进入条件判断
        unsigned __int64 vector; // 最近一次Host异常向量，例如#PF=14、#GP=13
        unsigned __int64 error; // 最近一次Host异常错误码，便于判断页表/权限/段错误
        unsigned __int64 rip; // 最近一次Host异常RIP，用于定位VM-exit宿主路径
        unsigned __int64 rsp; // 最近一次Host异常RSP，用于判断是否宿主栈异常
    }host_exception_snapshot; // VMX root宿主异常快照，避免未处理异常没有任何线索

    struct __vmresume_failure_snapshot
    {
        volatile unsigned __int64 count; // VMRESUME返回次数：正常成功不会返回，只记录失败诊断，不参与VT进入条件判断
        unsigned __int64 rflags; // VMRESUME失败后的RFLAGS，用于区分VMfailValid(ZF)和VMfailInvalid(CF)
        unsigned __int64 vmfail_valid; // ZF=1且CF=0时置位，VM_INSTRUCTION_ERROR字段有效
        unsigned __int64 vmfail_invalid; // CF=1时置位，当前VMCS可能不可用，VMREAD字段只能作为尽力快照
        unsigned __int64 vmread_failure_mask; // 每一位代表对应VMREAD失败，避免把0值误认为真实VMCS字段
        unsigned __int64 vm_instruction_error; // VMRESUME失败原因，只有VMfailValid时硬件保证有效
        unsigned __int64 vm_exit_reason; // 最近VM-exit原因，辅助定位是entry failure、EPT还是异常路径
        unsigned __int64 exit_qualification; // 最近exit qualification
        unsigned __int64 guest_rip; // 失败时Guest RIP
        unsigned __int64 guest_rsp; // 失败时Guest RSP
        unsigned __int64 guest_cr3; // 失败时Guest CR3
    }vmresume_failure_snapshot; // VMRESUME失败快照：只写内存，后续在安全上下文统一输出

    struct __vcpu_status
    {
        unsigned __int64 vmx_on; // 是否已 VMXON
        unsigned __int64 vmm_launched;  //vm虚拟机的运行状态
    }vcpu_status; // VCPU 状态

    struct __vmcs_effective_controls
    {
        unsigned __int64 pinbased; // 硬件调整后的 Pin-Based 控制位
        unsigned __int64 primary; // 硬件调整后的 Primary Processor-Based 控制位
        unsigned __int64 secondary; // 硬件调整后的 Secondary Processor-Based 控制位
        unsigned __int64 exit; // 硬件调整后的 VM-Exit 控制位
        unsigned __int64 entry; // 硬件调整后的 VM-Entry 控制位
    }vmcs_effective_controls; // 后续条件 VMCS 写入必须使用该真实生效值

    struct __vmx_off_state
    {
        unsigned __int64  vmx_off_executed;  //关闭虚拟化
        unsigned __int64  guest_rip; // VMXOFF 后恢复 RIP
        unsigned __int64  guest_rsp; // VMXOFF 后恢复 RSP
    }vmx_off_state; // VMXOFF 状态

    struct __vcpu_bitmaps
    {
        unsigned __int8* io_bitmap_a; // IO 位图 A
        unsigned __int64 io_bitmap_a_physical; // IO 位图 A 物理地址

        unsigned __int8* io_bitmap_b; // IO 位图 B
        unsigned __int64 io_bitmap_b_physical; // IO 位图 B 物理地址
    }vcpu_bitmaps; // 位图集合

    __ept_state* ept_state; // EPT 状态指针

    unsigned __int32 logical_processor_index; // KeQueryActiveProcessorCountEx 的全局处理器索引
    PROCESSOR_NUMBER processor_number; // 处理器组内编号
    GROUP_AFFINITY processor_affinity; // 可跨 processor group 的线程亲和性

    CLIENT_ID Cid; //记录触发监视断点的guest的线程id
    unsigned __int64 breakpoint_detected; //记录触发的地址
};

struct __mtrr_info // MTRR 汇总状态
{
    // 88 个 fixed 子范围 + 1 个 SMRR + IA32_MTRRCAP.VCNT 全 8 位可表示的 256 个 variable 范围。
    __mtrr_range_descriptor memory_range[345]; // 容量是架构上限，不通过截断或跳过掩盖平台差异
    unsigned __int32 enabled_memory_ranges; // 有效范围数量
    unsigned __int8 default_memory_type; // 默认内存类型
    mtrr_data mtrrs; // MTRR 详细信息
};

struct __vmm_context // 全局 VMM 上下文
{
    __vcpu* vcpu; //虚拟处理器
    pool_manager::__pool_manager* pool_manager; // 预分配池管理器
    __mtrr_info mtrr_info; // MTRR 信息

    unsigned __int32 processor_count;  //处理器数量
    unsigned __int32 highest_basic_leaf; // CPUID 最大基础叶
    bool hv_presence;  //标识hy对象已经创建
};

// 4级页映射的虚拟地址
union pml4_virtual_address // 解析虚拟地址各级索引
{
    void const* address; // 原始地址指针
    struct
    {
        uint64_t offset : 12; // 页内偏移
        uint64_t pt_idx : 9; // PT 索引
        uint64_t pd_idx : 9; // PD 索引
        uint64_t pdpt_idx : 9; // PDPT 索引
        uint64_t pml4_idx : 9; // PML4 索引
    };
};

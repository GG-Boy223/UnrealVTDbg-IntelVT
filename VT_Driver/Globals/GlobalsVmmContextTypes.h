#pragma once

#include "GlobalsEptHookTypes.h"
#include <Ring0/Hvm/VmmReleasePolicy.h>

constexpr unsigned int kVmExitTelemetryReasonCapacity = 256;
constexpr unsigned int kVmEntryEventQueueCapacity = 8;

struct __pending_vm_entry_event
{
    unsigned __int32 interrupt_info; // VM-entry interruption-information 字段原值
    unsigned __int32 instruction_length; // 软件中断/软件异常注入所需指令长度
    unsigned __int32 error_code; // 需要随异常投递的错误码
    unsigned __int64 debug_register_state; // 与调试异常原子投递的 guest DR6 快照
    unsigned __int8 has_debug_register_state; // 区分普通事件和需要恢复 DR6 的调试异常
};

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

    // Whether an NMI is pending delivery. NMI is edge-triggered and the CPU
    // latches only a single pending NMI, so a boolean matches the hardware.
    // 是否有一个待投递的 NMI；NMI 边沿触发，硬件只有单个 pending 锁存，布尔与硬件一致。
    volatile bool queued_nmi; // host NMI 可异步打断 VM-exit 路径，volatile 保证跨上下文可见

    struct __vm_entry_event_queue
    {
        __pending_vm_entry_event entries[kVmEntryEventQueueCapacity];
        unsigned __int32 head;
        unsigned __int32 tail;
        unsigned __int32 count;
    }pending_vm_entry_events; // 同一 VM-exit 内无法立即写入 VM-entry 字段的事件按 FIFO 延后

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

    struct __fatal_vmx_snapshot
    {
        volatile unsigned __int64 count; // Fatal VMX错误累计次数，主要供崩溃转储检查
        unsigned __int64 reason; // vmx_fatal_reason
        unsigned __int64 detail1; // 失败路径提供的第一诊断参数
        unsigned __int64 detail2; // 失败路径提供的第二诊断参数
        unsigned __int64 vm_instruction_error; // 尽力读取的VM指令错误
        unsigned __int64 vm_exit_reason; // 尽力读取的VM-exit reason
        unsigned __int64 guest_rip; // 尽力读取的Guest RIP
        unsigned __int64 guest_rsp; // 尽力读取的Guest RSP
        unsigned __int64 guest_cr3; // 尽力读取的Guest CR3
    }fatal_vmx_snapshot;

    struct __vcpu_status
    {
        volatile unsigned __int64 vmx_on; // 是否已 VMXON
        volatile unsigned __int64 vmm_launched;  //vm虚拟机的运行状态
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
        volatile unsigned __int64 vmx_off_executed; // VMXOFF 请求已由 VM-exit handler 接受
        volatile unsigned __int64 vmx_off_completed; // VMXOFF 指令成功并已越过硬件完成点
        unsigned __int64 guest_rip; // VMXOFF 后恢复 RIP
        unsigned __int64 guest_rsp; // VMXOFF 后恢复 RSP
        unsigned __int64 guest_rflags; // VMXOFF 后恢复 RFLAGS
        unsigned __int64 guest_cs; // VMXOFF 后恢复 CS
        unsigned __int64 guest_ss; // VMXOFF 后恢复 SS
        unsigned __int64 guest_cr0; // VMXOFF 后恢复有效 CR0
        unsigned __int64 guest_cr4; // VMXOFF 后恢复有效 CR4
    }vmx_off_state; // VMXOFF 状态

    struct __vcpu_bitmaps
    {
        unsigned __int8* io_bitmap_a; // IO 位图 A
        unsigned __int64 io_bitmap_a_physical; // IO 位图 A 物理地址

        unsigned __int8* io_bitmap_b; // IO 位图 B
        unsigned __int64 io_bitmap_b_physical; // IO 位图 B 物理地址
    }vcpu_bitmaps; // 位图集合

    __ept_state* ept_state; // EPT 状态指针

    __guest_mtrr_state guest_mtrr_state; // 每逻辑处理器独立的 guest MTRR 影子状态

    unsigned __int32 logical_processor_index; // KeQueryActiveProcessorCountEx 的全局处理器索引
    PROCESSOR_NUMBER processor_number; // 处理器组内编号
    GROUP_AFFINITY processor_affinity; // 可跨 processor group 的线程亲和性
    unsigned __int32 max_physical_address_bits; // 当前逻辑处理器的 CPUID.80000008H MAXPHYADDR 位数

    CLIENT_ID Cid; //记录触发监视断点的guest的线程id
    unsigned __int64 breakpoint_detected; //记录触发的地址
    int breakpoint_debug_register; //记录触发的 DR0-DR3 槽位，-1 表示无效

    // guest 可见的 DR7 影子（含 GD 与全部 guest 可写位）。
    // 物理 DR7 / GUEST_DR7 都必须剔除 GD（否则 root 的 MOV DR 会触发 #DB → 三重故障），
    // 但 guest 的 MOV DR7 读回必须看到自己写入的 GD，因此单独维护这份影子。
    // 维护点：Vmcs/VmcsGuest.cpp（初始化）、VmExit/VmExitDebugRegisters.cpp（读写）。
    unsigned __int64 guest_dr7_shadow;

    unsigned __int64 cr3_target_values[4];
    unsigned __int32 cr3_target_count;
    unsigned __int32 cr3_target_cursor;

    struct __cr3_candidate_slot
    {
        volatile unsigned __int64 value;
        volatile LONG64 sequence;
        LONG64 consumedSequence;
    }cr3_candidate;

    struct __vmexit_telemetry
    {
        unsigned __int64 reasonCount[kVmExitTelemetryReasonCapacity];
        unsigned __int64 reasonCycles[kVmExitTelemetryReasonCapacity];
        unsigned __int64 overflowCount;
        unsigned __int64 overflowCycles;
    }vmexit_telemetry;
};

static_assert(FIELD_OFFSET(__vcpu::__vmx_off_state, vmx_off_completed) == 0x08, "VMXOFF completion offset must match vm-exit.asm");
static_assert(FIELD_OFFSET(__vcpu::__vmx_off_state, guest_rip) == 0x10, "VMXOFF guest RIP offset must match vm-exit.asm");
static_assert(FIELD_OFFSET(__vcpu::__vmx_off_state, guest_rsp) == 0x18, "VMXOFF guest RSP offset must match vm-exit.asm");
static_assert(FIELD_OFFSET(__vcpu::__vmx_off_state, guest_rflags) == 0x20, "VMXOFF guest RFLAGS offset must match vm-exit.asm");
static_assert(FIELD_OFFSET(__vcpu::__vmx_off_state, guest_cs) == 0x28, "VMXOFF guest CS offset must match vm-exit.asm");
static_assert(FIELD_OFFSET(__vcpu::__vmx_off_state, guest_ss) == 0x30, "VMXOFF guest SS offset must match vm-exit.asm");
static_assert(FIELD_OFFSET(__vcpu::__vmx_off_state, guest_cr0) == 0x38, "VMXOFF guest CR0 offset must match vm-exit.asm");
static_assert(FIELD_OFFSET(__vcpu::__vmx_off_state, guest_cr4) == 0x40, "VMXOFF guest CR4 offset must match vm-exit.asm");
static_assert(sizeof(__vcpu::__vmx_off_state) == 0x48, "VMXOFF state size must match vm-exit.asm");

struct __mtrr_info // MTRR 汇总状态
{
    // 88 个 fixed 子范围 + 1 个 SMRR + IA32_MTRRCAP.VCNT 全 8 位可表示的 256 个 variable 范围。
    __mtrr_range_descriptor memory_range[345]; // 容量是架构上限，不通过截断或跳过掩盖平台差异
    unsigned __int32 enabled_memory_ranges; // 有效范围数量
    unsigned __int8 default_memory_type; // 默认内存类型
};

struct __vmm_context // 全局 VMM 上下文
{
    __vcpu* vcpu; //虚拟处理器
    pool_manager::__pool_manager* pool_manager; // 预分配池管理器
    __mtrr_info mtrr_info; // MTRR 信息

    unsigned __int32 processor_count;  //处理器数量
    unsigned __int32 highest_basic_leaf; // CPUID 最大基础叶
    unsigned __int64 platform_max_physical_end; // 全平台最大物理地址上界（各逻辑处理器 CPUID.80000008H 取最大值），EPT 覆盖范围按它计算
    bool hv_presence;  //标识hy对象已经创建
    volatile hv::vmm_release_policy::lifecycle_state lifecycle_state; // VMM 资源所有权阶段
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

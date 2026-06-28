#pragma once

#include "VmcsFields.h"

// 次级处理器执行控制字段（64 位位域结构）
// 原理：该字段控制“次级功能开关”，例如 EPT、VPID、INVPCID 等硬件加速能力。
// 作用：启用后可以减少 VM-exit 并提升性能，或让某些指令在来宾中直接执行。
// 注意：位的取值必须满足 IA32_VMX_PROCBASED_CTLS2 的允许规则。
union __vmx_secondary_processor_based_control // 次级处理器执行控制位域联合体
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 virtualize_apic_accesses : 1; // 位 0：是否虚拟化 APIC 访问
        unsigned __int64 enable_ept : 1; // 位 1：是否启用 EPT
        unsigned __int64 descriptor_table_exiting : 1; // 位 2：是否拦截描述符表访问
        unsigned __int64 enable_rdtscp : 1; // 位 3：是否允许 RDTSCP 指令
        unsigned __int64 virtualize_x2apic_mode : 1; // 位 4：是否虚拟化 x2APIC 模式
        unsigned __int64 enable_vpid : 1; // 位 5：是否启用 VPID
        unsigned __int64 wbinvd_exiting : 1; // 位 6：是否拦截 WBINVD
        unsigned __int64 unrestricted_guest : 1; // 位 7：是否允许非受限来宾
        unsigned __int64 apic_register_virtualization : 1; // 位 8：是否虚拟化 APIC 寄存器
        unsigned __int64 virtual_interrupt_delivery : 1; // 位 9：是否启用虚拟中断投递
        unsigned __int64 pause_loop_exiting : 1; // 位 10：是否启用 PAUSE 循环退出
        unsigned __int64 rdrand_exiting : 1; // 位 11：是否拦截 RDRAND
        unsigned __int64 enable_invpcid : 1; // 位 12：是否允许 INVPCID
        unsigned __int64 enable_vmfunc : 1; // 位 13：是否启用 VMFUNC
        unsigned __int64 vmcs_shadowing : 1; // 位 14：是否启用 VMCS Shadowing
        unsigned __int64 enable_encls_exiting : 1; // 位 15：是否拦截 ENCLS
        unsigned __int64 rdseed_exiting : 1; // 位 16：是否拦截 RDSEED
        unsigned __int64 enable_pml : 1; // 位 17：是否启用 PML
        unsigned __int64 ept_violation : 1; // 位 18：是否启用 EPT 违规功能
        unsigned __int64 conceal_vmx_from_pt : 1; // 位 19：是否对 PT 隐藏 VMX
        unsigned __int64 enable_xsaves_xrstors : 1; // 位 20：是否允许 XSAVES/XRSTORS
        unsigned __int64 reserved_0 : 1; // 位 21：保留位（按手册要求设置）
        unsigned __int64 mode_based_execute_control_ept : 1; // 位 22：是否启用 EPT 模式基执行控制
        unsigned __int64 sub_page_write_permission_for_ept : 1; // 位 23：是否启用 EPT 子页写权限
        unsigned __int64 intel_pt_uses_guest_physical_address : 1; // 位 24：PT 使用来宾物理地址
        unsigned __int64 use_tsc_scaling : 1; // 位 25：是否启用 TSC 缩放
        unsigned __int64 enable_user_wait_and_pause : 1; // 位 26：是否允许 UMONITOR/UMWAIT/TPAUSE
        unsigned __int64 enable_pconfig : 1; // 位 27：是否允许 PCONFIG
        unsigned __int64 enable_enclv_exiting : 1; // 位 28：是否拦截 ENCLV
    };
};

// 主处理器执行控制字段（64 位位域结构）
// 原理：该字段决定哪些指令/事件触发 VM-exit（例如 HLT、INVLPG、RDTSC）。
// 作用：用于拦截敏感操作、实现虚拟化策略与安全控制。
// 注意：位的取值必须满足 IA32_VMX_PROCBASED_CTLS 的允许规则。
union __vmx_primary_processor_based_control // 主处理器执行控制位域联合体
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 reserved_0 : 2; // 位 1:0 保留
        unsigned __int64 interrupt_window_exiting : 1; // 位 2：中断窗口退出
        unsigned __int64 use_tsc_offsetting : 1; // 位 3：使用 TSC 偏移
        unsigned __int64 reserved_1 : 3; // 位 6:4 保留
        unsigned __int64 hlt_exiting : 1; // 位 7：HLT 退出
        unsigned __int64 reserved_2 : 1; // 位 8 保留
        unsigned __int64 invlpg_exiting : 1; // 位 9：INVLPG 退出
        unsigned __int64 mwait_exiting : 1; // 位 10：MWAIT 退出
        unsigned __int64 rdpmc_exiting : 1; // 位 11：RDPMC 退出
        unsigned __int64 rdtsc_exiting : 1; // 位 12：RDTSC 退出
        unsigned __int64 reserved_3 : 2; // 位 14:13 保留
        unsigned __int64 cr3_load_exiting : 1; // 位 15：CR3 加载退出
        unsigned __int64 cr3_store_exiting : 1; // 位 16：CR3 存储退出
        unsigned __int64 activate_tertiary_controls : 1; // 位 17：启用第三级控制
        unsigned __int64 reserved_4 : 1; // 位 18 保留
        unsigned __int64 cr8_load_exiting : 1; // 位 19：CR8 加载退出
        unsigned __int64 cr8_store_exiting : 1; // 位 20：CR8 存储退出
        unsigned __int64 use_tpr_shadow : 1; // 位 21：使用 TPR Shadow
        unsigned __int64 nmi_window_exiting : 1; // 位 22：NMI 窗口退出
        unsigned __int64 mov_dr_exiting : 1; // 位 23：MOV DR 退出
        unsigned __int64 unconditional_io_exiting : 1; // 位 24：无条件 I/O 退出
        unsigned __int64 use_io_bitmaps : 1; // 位 25：使用 I/O 位图
        unsigned __int64 reserved_5 : 1; // 位 26 保留
        unsigned __int64 monitor_trap_flag : 1; // 位 27：监控陷阱标志（单步）
        unsigned __int64 use_msr_bitmaps : 1; // 位 28：使用 MSR 位图
        unsigned __int64 monitor_exiting : 1; // 位 29：MONITOR 退出
        unsigned __int64 pause_exiting : 1; // 位 30：PAUSE 退出
        unsigned __int64 active_secondary_controls : 1; // 位 31：启用次级控制
    };
};

// 基于引脚的执行控制 MSR 位域结构
// 原理：pinbased 控制主要影响外部中断/NMI 等异步事件是否导致 VM-exit。
// 作用：决定来宾对外部中断的可见性以及中断处理方式。
// 注意：位的取值必须满足 IA32_VMX_PINBASED_CTLS 的允许规则。
union __vmx_pinbased_control_msr // 基于引脚的执行控制位域联合体
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 external_interrupt_exiting : 1; // 位 0：外部中断退出
        unsigned __int64 reserved_0 : 2; // 位 2:1 保留
        unsigned __int64 nmi_exiting : 1; // 位 3：NMI 退出
        unsigned __int64 reserved_1 : 1; // 位 4 保留
        unsigned __int64 virtual_nmis : 1; // 位 5：虚拟 NMI
        unsigned __int64 activate_vmxpreemption_timer : 1; // 位 6：启用 VMX 预抢占计时器
        unsigned __int64 process_posted_interrupts : 1; // 位 7：处理投递中断
    };
};

// 真实控制设置（True Controls），来自相关 MSR
// 原理：MSR 中的 allowed_0/allowed_1 位表示“必须为 0/1 的控制位”。
// 作用：VMM 必须按该规则配置控制字段，否则 VMX 指令会失败。
union __vmx_true_control_settings // 真实控制设置位域联合体
{
    unsigned __int64 all;
    struct
    {
        unsigned __int32 allowed_0_settings; // 允许为 0 的控制位集合
        unsigned __int32 allowed_1_settings; // 允许为 1 的控制位集合
    };
};

// 中断命令寄存器（ICR）的位域结构
// 原理：ICR 用于向指定 CPU 发送 IPI（处理器间中断）。
// 作用：多核情况下用于唤醒、同步或通知其它 CPU。
union __interrupt_command_register // 中断命令寄存器位域联合体
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 vector : 8; // 中断向量号
        unsigned __int64 delivery_mode : 3; // 传递模式（固定、NMI、INIT 等）
        unsigned __int64 destination_mode : 1; // 目标模式（物理或逻辑）
        unsigned __int64 delivery_status : 1; // 传递状态（是否在发送）
        unsigned __int64 reserved_0 : 1; // 保留位
        unsigned __int64 level : 1; // 触发电平（高/低）
        unsigned __int64 trigger_mode : 1; // 触发方式（边沿/电平）
        unsigned __int64 reserved_1 : 2; // 保留位
        unsigned __int64 destination_short : 2; // 简写目标（自身/所有/除自身外）
        unsigned __int64 reserved_3 : 35; // 保留位
        unsigned __int64 destination : 8; // 目标 APIC ID
    };
};

// VM-entry 控制字段位域结构
// 原理：决定进入来宾时需要加载哪些寄存器或模式（如 EFER、PAT、CET）。
// 作用：确保来宾进入时的 CPU 状态正确，避免异常或崩溃。
union __vmx_entry_control // VM-entry 控制位域联合体
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 reserved_0 : 2; // 位 1:0 保留
        unsigned __int64 load_dbg_controls : 1; // 位 2：是否加载调试控制寄存器
        unsigned __int64 reserved_1 : 6; // 位 8:3 保留
        unsigned __int64 ia32e_mode_guest : 1; // 位 9：来宾是否为 IA-32e 模式
        unsigned __int64 entry_to_smm : 1; // 位 10：是否进入 SMM
        unsigned __int64 deactivate_dual_monitor_treament : 1; // 位 11：禁用双重监控处理
        unsigned __int64 reserved_2 : 1; // 位 12 保留
        unsigned __int64 load_ia32_perf_global_control : 1; // 位 13：加载性能全局控制寄存器
        unsigned __int64 load_ia32_pat : 1; // 位 14：加载 PAT
        unsigned __int64 load_ia32_efer : 1; // 位 15：加载 EFER
        unsigned __int64 load_ia32_bndcfgs : 1; // 位 16：加载 BNDCFGS
        unsigned __int64 conceal_vmx_from_pt : 1; // 位 17：对 PT 隐藏 VMX
        unsigned __int64 load_ia32_rtit_ctl : 1; // 位 18：加载 RTIT 控制
        unsigned __int64 reserved_3 : 1; // 位 19 保留
        unsigned __int64 load_cet_state : 1; // 位 20：加载 CET 状态
        unsigned __int64 load_guest_ia32_lbr_ctl : 1; // 位 21：加载来宾 LBR 控制
        unsigned __int64 load_pkrs : 1; // 位 22：加载 PKRS
    };
};

// VM-exit 控制字段位域结构
// 原理：决定退出到宿主时保存/加载哪些状态。
// 作用：保证宿主在 VM-exit 后能继续稳定运行，并保留必要调试信息。
union __vmx_exit_control // VM-exit 控制位域联合体
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 reserved_0 : 2; // 位 1:0 保留
        unsigned __int64 save_dbg_controls : 1; // 位 2：保存调试控制寄存器
        unsigned __int64 reserved_1 : 6; // 位 8:3 保留
        unsigned __int64 host_address_space_size : 1; // 位 9：宿主地址空间大小（0=32 位，1=64 位）
        unsigned __int64 reserved_2 : 2; // 位 11:10 保留
        unsigned __int64 load_ia32_perf_global_control : 1; // 位 12：加载性能全局控制
        unsigned __int64 reserved_3 : 2; // 位 14:13 保留
        unsigned __int64 acknowledge_interrupt_on_exit : 1; // 位 15：退出时确认中断
        unsigned __int64 reserved_4 : 2; // 位 17:16 保留
        unsigned __int64 save_ia32_pat : 1; // 位 18：保存 PAT
        unsigned __int64 load_ia32_pat : 1; // 位 19：加载 PAT
        unsigned __int64 save_ia32_efer : 1; // 位 20：保存 EFER
        unsigned __int64 load_ia32_efer : 1; // 位 21：加载 EFER
        unsigned __int64 save_vmx_preemption_timer_value : 1; // 位 22：保存 VMX 预抢占计时器值
        unsigned __int64 clear_ia32_bndcfgs : 1; // 位 23：清除 BNDCFGS
        unsigned __int64 conceal_vmx_from_pt : 1; // 位 24：对 PT 隐藏 VMX
        unsigned __int64 clear_ia32_rtit_ctl : 1; // 位 25：清除 RTIT 控制
        unsigned __int64 clear_ia32_lbr_ctl : 1; // 位 26：清除 LBR 控制
        unsigned __int64 reserved_5 : 1; // 位 27 保留
        unsigned __int64 load_cet_state : 1; // 位 28：加载 CET 状态
        unsigned __int64 load_pkrs : 1; // 位 29：加载 PKRS
        unsigned __int64 save_ia32_perf_global_ctl : 1; // 位 30：保存性能全局控制
        unsigned __int64 activate_secondary_controls : 1; // 位 31：启用次级控制
    };
};

// 来宾挂起的调试异常位域结构
// 原理：记录来宾中断点与单步等调试状态，用于 VM-exit 后恢复正确的调试语义。
// 作用：保证调试相关行为在虚拟化环境下不丢失。
union __vmx_pending_debug_exceptions // 挂起调试异常位域联合体
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 b0 : 1; // 断点 0 状态
        unsigned __int64 b1 : 1; // 断点 1 状态
        unsigned __int64 b2 : 1; // 断点 2 状态
        unsigned __int64 b3 : 1; // 断点 3 状态
        unsigned __int64 reserved1 : 8; // 保留位
        unsigned __int64 enabled_bp : 1; // 是否启用断点
        unsigned __int64 reserved2 : 1; // 保留位
        unsigned __int64 bs : 1; // 单步标志
        unsigned __int64 reserved3 : 1; // 保留位
        unsigned __int64 rtm : 1; // RTM 相关标志
        unsigned __int64 reserved4 : 47; // 保留位
    };
};

// 来宾中断可接受状态位域结构
// 原理：表示来宾当前是否允许接收中断（例如被 STI/MOV SS 暂时阻塞）。
// 作用：VMM 在注入中断时需要检查该状态，避免破坏指令语义。
union __vmx_interruptibility_state // 中断可接受状态位域联合体
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 blocking_by_sti : 1; // STI 指令导致的中断阻塞
        unsigned __int64 blocking_by_mov_ss : 1; // MOV SS 指令导致的中断阻塞
        unsigned __int64 blocking_by_smi : 1; // SMI 导致的中断阻塞
        unsigned __int64 blocking_by_nmi : 1; // NMI 导致的中断阻塞
        unsigned __int64 enclave_interruption : 1; // SGX enclave 中断状态
        unsigned __int64 reserved : 27; // 保留位
    };
};

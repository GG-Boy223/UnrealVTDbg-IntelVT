#include "VmcsInternal.h"

void set_entry_control(__vmx_entry_control& entry_control) // 设置 VM-entry 控制位
{
    // 当 load_dbg_controls=1 时，VM-entry 会从 guest-state 字段加载调试寄存器
    entry_control.load_dbg_controls = true; // 让 DR7 和 IA32_DEBUGCTL 在 VM-entry 时被加载
    // 当 ia32e_mode_guest=1 时，VM-entry 后来宾处于 64 位 IA-32e 模式
    entry_control.ia32e_mode_guest = true; // 表示来宾运行在 64 位模式（需要 CPU 支持）
    // entry_to_smm 控制是否进入 SMM（系统管理模式）
    entry_control.entry_to_smm = false; // 从非 SMM 环境进入 VM 时必须为 0
    // deactivate_dual_monitor_treament 控制 SMI/SMM 的默认处理
    entry_control.deactivate_dual_monitor_treament = false; // 维持默认 SMM 处理方式
    // 是否在 VM-entry 时加载 IA32_PERF_GLOBAL_CTRL
    entry_control.load_ia32_perf_global_control = true; // 让来宾性能计数控制寄存器被正确加载
    // 是否在 VM-entry 时加载 IA32_PAT（页属性表）
    entry_control.load_ia32_pat = true; // 保证来宾内存类型行为与宿主/来宾设置一致
    // 是否在 VM-entry 时加载 IA32_EFER
    entry_control.load_ia32_efer = false; // 这里选择不加载，避免与宿主控制冲突
    // 是否在 VM-entry 时加载 IA32_BNDCFGS
    entry_control.load_ia32_bndcfgs = false; // 这里未启用 MPX 相关寄存器加载
    // 是否对 Intel PT 隐藏 VMX（不产生 PIP/VMCS 包）
    entry_control.conceal_vmx_from_pt = true; // 避免 PT 暴露虚拟化痕迹
    // 是否在 VM-entry 时加载 IA32_RTIT_CTL
    entry_control.load_ia32_rtit_ctl = false; // 未启用 PT 控制寄存器加载
    // 是否在 VM-entry 时加载 CET 相关 MSR/SSP
    entry_control.load_cet_state = false; // 未启用 CET
    // 是否在 VM-entry 时加载 PKRS
    entry_control.load_pkrs = false; // 未启用 PKRS
}

// 源自 Intel 手册 Volume 3 第 24.7.1 节表 24-11：VM-exit 控制字段
// 作用：决定 VM-exit 时硬件要保存/加载哪些状态
void set_exit_control(__vmx_exit_control& exit_control) // 设置 VM-exit 控制位
{
    // VM-exit 时是否保存调试控制寄存器
    exit_control.save_dbg_controls = true; // 保存 DR7 和 IA32_DEBUGCTL，防止状态丢失
    // VM-exit 后宿主是否处于 64 位模式
    exit_control.host_address_space_size = true; // 宿主为 64 位模式
    // VM-exit 时是否加载 IA32_PERF_GLOBAL_CTRL
    exit_control.load_ia32_perf_global_control = true; // 确保宿主性能计数控制正确恢复
    // VM-exit 时是否确认外部中断
    exit_control.acknowledge_interrupt_on_exit = true; // 允许宿主接收外部中断
    // VM-exit 时是否保存 IA32_PAT
    exit_control.save_ia32_pat = true; // 保存来宾 PAT 以便恢复
    // VM-exit 时是否加载 IA32_PAT
    exit_control.load_ia32_pat = true; // 恢复宿主 PAT
    // VM-exit 时是否保存 IA32_EFER
    exit_control.save_ia32_efer = false; // 这里未保存 EFER
    // VM-exit 时是否加载 IA32_EFER
    exit_control.load_ia32_efer = false; // 这里未加载 EFER
    // VM-exit 时是否保存 VMX 预抢占计时器
    exit_control.save_vmx_preemption_timer_value = false; // 不需要保存计时器值
    // VM-exit 时是否清除 IA32_BNDCFGS
    exit_control.clear_ia32_bndcfgs = false; // 不清除 BNDCFGS
    // 是否对 Intel PT 隐藏 VMX（不产生 PIP/VMCS 包）
    exit_control.conceal_vmx_from_pt = true; // 隐藏虚拟化痕迹
    // VM-exit 时是否清除 IA32_RTIT_CTL
    exit_control.clear_ia32_rtit_ctl = false; // 不清除 PT 控制寄存器
    // VM-exit 时是否加载 CET 相关状态
    exit_control.load_cet_state = false; // 未启用 CET
    // VM-exit 时是否加载 IA32_PKRS
    exit_control.load_pkrs = false; // 未启用 PKRS
}

// 源自 Intel 手册 Volume 3 第 24.6.2 节表 24-6：主处理器执行控制
// 作用：决定哪些指令/事件会触发 VM-exit
void set_primary_controls(__vmx_primary_processor_based_control& primary_controls) // 设置主处理器控制位
{
    // 中断窗口退出：当允许中断且无阻塞时触发 VM-exit
    primary_controls.interrupt_window_exiting = false; // 关闭该功能，减少退出次数
    // 使用 TSC 偏移：对 RDTSC/RDTSCP/相关 RDMSR 返回值加偏移
    primary_controls.use_tsc_offsetting = false; // 不使用偏移，保持真实时间戳
    // HLT 指令是否触发 VM-exit
    primary_controls.hlt_exiting = false; // 不拦截 HLT

// INVLPG 指令是否触发 VM-exit
#ifdef _MINIMAL // 如果启用精简模式
    primary_controls.invlpg_exiting = false; // 不拦截 INVLPG
#else // 非精简模式
    primary_controls.invlpg_exiting = true; // 拦截 INVLPG
#endif // 条件编译结束

    // MWAIT 指令是否触发 VM-exit
    primary_controls.mwait_exiting = false; // 不拦截 MWAIT
    // RDPMC 指令是否触发 VM-exit
    primary_controls.rdpmc_exiting = false; // 不拦截 RDPMC

// RDTSC/RDTSCP 指令是否触发 VM-exit
#ifdef _MINIMAL // 精简模式
    primary_controls.rdtsc_exiting = false; // 不拦截 RDTSC/RDTSCP
#else // 非精简模式
    primary_controls.rdtsc_exiting = true; // 拦截 RDTSC/RDTSCP
#endif // 条件编译结束

// CR3-load exiting：写 CR3 时根据 CR3-target 规则决定是否 VM-exit
#ifdef _MINIMAL // 精简模式
    primary_controls.cr3_load_exiting = true; // 拦截写 CR3
#else // 非精简模式
    primary_controls.cr3_load_exiting = true; // 拦截写 CR3
#endif // 条件编译结束

// CR3-store exiting：读 CR3 时是否 VM-exit
#ifdef _MINIMAL // 精简模式
    primary_controls.cr3_store_exiting = false; // 不拦截读 CR3
#else // 非精简模式
    primary_controls.cr3_store_exiting = true; // 拦截读 CR3
#endif // 条件编译结束

    // CR8 load 是否 VM-exit
    primary_controls.cr8_load_exiting = false; // 不拦截写 CR8
    // CR8 store 是否 VM-exit
    primary_controls.cr8_store_exiting = false; // 不拦截读 CR8
    // 使用 TPR shadow 及 APIC 虚拟化特性
    primary_controls.use_tpr_shadow = false; // 不启用 TPR shadow
    // NMI 窗口退出
    primary_controls.nmi_window_exiting = false; // 不启用 NMI 窗口退出

// MOV DR 指令是否 VM-exit
#ifdef _MINIMAL // 精简模式
    primary_controls.mov_dr_exiting = false; // 不拦截 MOV DR
#else // 非精简模式
    primary_controls.mov_dr_exiting = true; // 拦截 MOV DR
#endif // 条件编译结束

    // 无条件 I/O 退出（所有 IO 指令都 VM-exit）
    primary_controls.unconditional_io_exiting = false; // 不启用无条件 I/O 退出

// 是否启用 I/O 位图控制 I/O 指令拦截
#ifdef _MINIMAL // 精简模式
    primary_controls.use_io_bitmaps = false; // 不使用 I/O 位图
#else // 非精简模式
    primary_controls.use_io_bitmaps = true; // 使用 I/O 位图
#endif // 条件编译结束

    // Monitor Trap Flag（单步调试）
    primary_controls.monitor_trap_flag = false; // 默认关闭单步
    // 是否启用 MSR 位图来控制 RDMSR/WRMSR 拦截
    primary_controls.use_msr_bitmaps = true; // 开启 MSR 位图拦截
    // MONITOR 指令是否 VM-exit
    primary_controls.monitor_exiting = false; // 不拦截 MONITOR
    // PAUSE 指令是否 VM-exit
    primary_controls.pause_exiting = false; // 不拦截 PAUSE
    // 是否启用次级处理器执行控制
    primary_controls.active_secondary_controls = true; // 启用次级控制
}

// 源自 Intel 手册 Volume 3 第 24.6.2 节表 24-7：次级处理器执行控制
// 作用：控制 EPT/VPID/INVPCID 等更高级特性
void set_secondary_controls(__vmx_secondary_processor_based_control& secondary_controls) // 设置次级处理器控制位
{
    // 是否虚拟化 APIC access page 访问
    secondary_controls.virtualize_apic_accesses = false; // 不启用 APIC access page 虚拟化
    // 是否启用 EPT
    secondary_controls.enable_ept = true; // 启用 EPT，提高性能

// 描述符表访问是否 VM-exit
#ifdef _MINIMAL // 精简模式
    secondary_controls.descriptor_table_exiting = false; // 不拦截描述符表访问
#else // 非精简模式
    secondary_controls.descriptor_table_exiting = true; // 拦截描述符表访问
#endif // 条件编译结束

    // 是否允许 RDTSCP
    secondary_controls.enable_rdtscp = true; // 允许 RDTSCP 指令
    // 是否虚拟化 x2APIC 模式
    secondary_controls.virtualize_x2apic_mode = false; // 不启用 x2APIC 虚拟化
    // 是否启用 VPID
    secondary_controls.enable_vpid = true; // 启用 VPID 以减少 TLB 刷新

// WBINVD 是否 VM-exit
#ifdef _MINIMAL // 精简模式
    secondary_controls.wbinvd_exiting = false; // 不拦截 WBINVD
#else // 非精简模式
    secondary_controls.wbinvd_exiting = true; // 拦截 WBINVD
#endif // 条件编译结束

    // 是否允许非受限来宾（实模式/无分页保护模式）
    secondary_controls.unrestricted_guest = false; // 不启用非受限来宾
    // 是否虚拟化 APIC 寄存器访问
    secondary_controls.apic_register_virtualization = false; // 不启用 APIC 寄存器虚拟化
    // 是否启用虚拟中断投递
    secondary_controls.virtual_interrupt_delivery = false; // 不启用虚拟中断投递
    // PAUSE 循环是否触发 VM-exit
    secondary_controls.pause_loop_exiting = false; // 不启用 PAUSE loop exiting

// RDRAND 是否 VM-exit
#ifdef _MINIMAL // 精简模式
    secondary_controls.rdrand_exiting = false; // 不拦截 RDRAND
#else // 非精简模式
    secondary_controls.rdrand_exiting = true; // 拦截 RDRAND
#endif // 条件编译结束

    // 是否允许 INVPCID
    secondary_controls.enable_invpcid = true; // 允许 INVPCID 指令
    // 是否启用 VMFUNC
    secondary_controls.enable_vmfunc = false; // 不启用 VMFUNC
    // 是否启用 VMCS Shadowing
    secondary_controls.vmcs_shadowing = false; // 不启用 VMCS Shadowing
    // 是否拦截 ENCLS
    secondary_controls.enable_encls_exiting = false; // 不拦截 ENCLS

// RDSEED 是否 VM-exit
#ifdef _MINIMAL // 精简模式
    secondary_controls.rdseed_exiting = false; // 不拦截 RDSEED
#else // 非精简模式
    secondary_controls.rdseed_exiting = true; // 拦截 RDSEED
#endif // 条件编译结束

    // 是否启用 PML（页修改日志）
    secondary_controls.enable_pml = false; // 不启用 PML
    // EPT violation 是否触发 #VE
    secondary_controls.ept_violation = false; // 不启用 #VE
    // 是否对 PT 隐藏 VMX
    secondary_controls.conceal_vmx_from_pt = true; // 隐藏虚拟化痕迹
    // 是否允许 XSAVES/XRSTORS
    secondary_controls.enable_xsaves_xrstors = true; // 允许扩展保存/恢复指令
    // 是否启用 EPT 模式基执行控制
    secondary_controls.mode_based_execute_control_ept = false; // 不启用 MBEC
    // 是否启用 EPT 子页写权限
    secondary_controls.sub_page_write_permission_for_ept = false; // 不启用子页写权限
    // PT 是否使用来宾物理地址
    secondary_controls.intel_pt_uses_guest_physical_address = false; // 不启用该特性
    // 是否启用 TSC 缩放
    secondary_controls.use_tsc_scaling = false; // 不启用 TSC 缩放
    // 是否允许 UMONITOR/UMWAIT/TPAUSE
    secondary_controls.enable_user_wait_and_pause = true; // 允许这些指令
    // 是否拦截 ENCLV
    secondary_controls.enable_enclv_exiting = false; // 不拦截 ENCLV
}

// 设置异常位图：决定哪些异常发生时触发 VM-exit
void set_exception_bitmap(__exception_bitmap& exception_bitmap) // 设置异常拦截位图
{
    exception_bitmap.divide_error = false; // 不拦截 #DE
    exception_bitmap.debug = false; // 不拦截 #DB
    exception_bitmap.nmi_interrupt = false; // 不拦截 NMI
    exception_bitmap.breakpoint = false; // 不拦截 #BP
    exception_bitmap.overflow = false; // 不拦截 #OF
    exception_bitmap.bound = false; // 不拦截 #BR
    exception_bitmap.invalid_opcode = false; // 不拦截 #UD
    exception_bitmap.coprocessor_segment_overrun = false; // 不拦截该异常
    exception_bitmap.invalid_tss = false; // 不拦截 #TS
    exception_bitmap.segment_not_present = false; // 不拦截 #NP
    exception_bitmap.stack_segment_fault = false; // 不拦截 #SS
    exception_bitmap.general_protection = false; // 不拦截 #GP
    exception_bitmap.page_fault = false; // 不拦截 #PF
    exception_bitmap.floating_point_error = false; // 不拦截 #MF
    exception_bitmap.alignment_check = false; // 不拦截 #AC
    exception_bitmap.machine_check = false; // 不拦截 #MC
    exception_bitmap.simd_floating_point_exception = false; // 不拦截 #XM
    exception_bitmap.virtualization_exception = false; // 不拦截 #VE
    exception_bitmap.control_protection_exception = false; // 不拦截 #CP
}

// 源自 Intel 手册 Volume 3 第 24.6.1 节表 24-5：基于引脚的执行控制
void set_pinbased_control_msr(__vmx_pinbased_control_msr& pinbased_controls) // 设置引脚控制位
{
    // 外部中断是否触发 VM-exit
    pinbased_controls.external_interrupt_exiting = false; // 允许中断直接进入来宾
    // NMI 是否触发 VM-exit
    pinbased_controls.nmi_exiting = true; // 拦截 NMI
    // 是否启用虚拟 NMI
    pinbased_controls.virtual_nmis = true; // 使用虚拟 NMI 机制
    // 是否启用 VMX 抢占计时器
    pinbased_controls.activate_vmxpreemption_timer = false; // 不启用计时器
    // 是否处理投递中断
    pinbased_controls.process_posted_interrupts = false; // 不启用投递中断处理
}

// 调整控制字段：根据 MSR 的 allowed_0/allowed_1 位过滤控制值
unsigned __int64 ajdust_controls(unsigned __int64 ctl, unsigned long msr) // 根据 MSR 规则修正控制位
{
    __msr msr_value = { 0 }; // 定义 MSR 读取结构
    msr_value.all = __readmsr(msr); // 读取指定 MSR
    ctl &= msr_value.high; // 清除必须为 0 的位
    ctl |= msr_value.low; // 设置必须为 1 的位
    return ctl; // 返回调整后的控制值
}

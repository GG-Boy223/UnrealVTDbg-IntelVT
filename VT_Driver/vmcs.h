// 本头文件定义 VMCS 字段编码和 VMX 控制位结构体，
// 这些定义用于在 VMM 中读写 VMCS、配置进入/退出控制等。
// 为了便于初学者理解，下面对每一行都添加了中文注释。
//
// 原理说明（重要）：
// 1) VMCS 是 CPU 硬件维护的虚拟机控制结构体，里面存放“来宾状态、宿主状态、控制项、退出信息”。
// 2) 当执行 VM-entry 时，CPU 会把“来宾状态字段”装载到真实寄存器；当 VM-exit 时，CPU 会把状态写回到 VMCS。
// 3) VMREAD/VMWRITE 必须使用本文件的“字段编码”，否则无法正确访问 VMCS。
// 4) 控制位（pinbased/primary/secondary/entry/exit）必须满足 MSR 允许的 0/1 规则，否则 VMXON/VM-entry 失败。
// 5) 保留位必须按手册要求设置，不能随意改动。

#pragma once                      // 只编译一次，避免头文件被重复包含导致重定义

#ifndef _VMCS_H                   // 如果还没有定义该宏，则进入包含保护区域
#define _VMCS_H                   // 定义宏，标记该头文件已被包含

// 栈对齐可以提高内存访问的效率
// 在 x64 架构中，栈通常要求 16 字节对齐
#define StackAlignment(x) ((x) & ~0xF) // 将地址的低 4 位清零，实现 16 字节对齐

#define _MINIMAL                  // 该宏可能用于控制编译分支，表示采用精简配置


// VMCS 字段编码（VMCS Encodings）
// 原理：VMCS 里的每个字段都有一个固定的编码，硬件只认识这些编码。
// 作用：VMM 通过 VMREAD/VMWRITE 使用这些编码读写来宾状态、宿主状态、控制项与退出信息。
// 注意：这些编码来自 Intel SDM，不能随意修改，否则读写到的字段会错乱。
enum // VMCS 字段编码枚举
{ // 定义块开始
    VIRTUAL_PROCESSOR_ID = 0x00000000,                   // 虚拟处理器 ID（VCPU 的标识）
    POSTED_INTERRUPT_NOTIFICATION_VECTOR = 0x00000002,   // 投递中断的通知向量号
    EPTP_INDEX = 0x00000004,                             // EPTP 索引（多 EPTP 场景）
    GUEST_ES_SELECTOR = 0x00000800,                      // 来宾 ES 段选择子
    GUEST_CS_SELECTOR = 0x00000802,                      // 来宾 CS 段选择子
    GUEST_SS_SELECTOR = 0x00000804,                      // 来宾 SS 段选择子
    GUEST_DS_SELECTOR = 0x00000806,                      // 来宾 DS 段选择子
    GUEST_FS_SELECTOR = 0x00000808,                      // 来宾 FS 段选择子
    GUEST_GS_SELECTOR = 0x0000080a,                      // 来宾 GS 段选择子
    GUEST_LDTR_SELECTOR = 0x0000080c,                    // 来宾 LDTR 选择子
    GUEST_TR_SELECTOR = 0x0000080e,                      // 来宾 TR 选择子
    GUEST_INTERRUPT_STATUS = 0x00000810,                 // 来宾中断状态字段
    GUEST_PML_INDEX = 0x00000812,                        // 来宾 PML 索引
    HOST_ES_SELECTOR = 0x00000c00,                       // 宿主 ES 段选择子
    HOST_CS_SELECTOR = 0x00000c02,                       // 宿主 CS 段选择子
    HOST_SS_SELECTOR = 0x00000c04,                       // 宿主 SS 段选择子
    HOST_DS_SELECTOR = 0x00000c06,                       // 宿主 DS 段选择子
    HOST_FS_SELECTOR = 0x00000c08,                       // 宿主 FS 段选择子
    HOST_GS_SELECTOR = 0x00000c0a,                       // 宿主 GS 段选择子
    HOST_TR_SELECTOR = 0x00000c0c,                       // 宿主 TR 选择子
    IO_BITMAP_A_ADDRESS = 0x00002000,                    // I/O 位图 A 物理地址低 32 位
    IO_BITMAP_A_HIGH = 0x00002001,                       // I/O 位图 A 物理地址高 32 位
    IO_BITMAP_B_ADDRESS = 0x00002002,                    // I/O 位图 B 物理地址低 32 位
    IO_BITMAP_B_HIGH = 0x00002003,                       // I/O 位图 B 物理地址高 32 位
    MSR_BITMAP_ADDRESS = 0x00002004,                     // MSR 位图物理地址低 32 位
    MSR_BITMAP_HIGH = 0x00002005,                        // MSR 位图物理地址高 32 位
    VM_EXIT_MSR_STORE_ADDR = 0x00002006,                 // VM-exit MSR 保存区地址低 32 位
    VM_EXIT_MSR_STORE_ADDR_HIGH = 0x00002007,            // VM-exit MSR 保存区地址高 32 位
    VM_EXIT_MSR_LOAD_ADDR = 0x00002008,                  // VM-exit MSR 加载区地址低 32 位
    VM_EXIT_MSR_LOAD_ADDR_HIGH = 0x00002009,             // VM-exit MSR 加载区地址高 32 位
    VM_ENTRY_MSR_LOAD_ADDR = 0x0000200a,                 // VM-entry MSR 加载区地址低 32 位
    VM_ENTRY_MSR_LOAD_ADDR_HIGH = 0x0000200b,            // VM-entry MSR 加载区地址高 32 位
    VMCS_EXECUTIVE_POINTER = 0x0000200c,                 // VMCS 执行指针
    PML_ADDRESS = 0x0000200e,                            // PML（页修改日志）地址
    TSC_OFFSET = 0x00002010,                             // TSC 偏移量低 32 位
    TSC_OFFSET_HIGH = 0x00002011,                        // TSC 偏移量高 32 位
    VIRTUAL_APIC_PAGE_ADDR = 0x00002012,                 // 虚拟 APIC 页面地址低 32 位
    VIRTUAL_APIC_PAGE_ADDR_HIGH = 0x00002013,            // 虚拟 APIC 页面地址高 32 位
    APIC_ACCESS_ADDRESS = 0x00002014,                    // APIC 访问地址（APIC access page）
    POSTED_INTERRUPT_DESCRIPTOR_ADDRESS = 0x00002016,    // 投递中断描述符地址低 32 位
    VM_FUNCTION_CONTROLS = 0x00002018,                   // VM 功能控制字段
    EPT_POINTER = 0x0000201a,                            // EPTP（扩展页表指针）低 32 位
    EOI_EXIT_BITMAP_0 = 0x0000201c,                      // EOI 退出位图 0
    EOI_EXIT_BITMAP_1 = 0x0000201e,                      // EOI 退出位图 1
    EOI_EXIT_BITMAP_2 = 0x00002020,                      // EOI 退出位图 2
    EOI_EXIT_BITMAP_3 = 0x00002022,                      // EOI 退出位图 3
    EPTP_LIST_ADDRESS = 0x00002024,                      // EPTP 列表地址低 32 位
    VMREAD_BITMAP_ADDRESS = 0x00002026,                  // VMREAD 位图地址低 32 位
    VMWRITE_BITMAP_ADDRESS = 0x00002028,                 // VMWRITE 位图地址低 32 位
    VIRTUALIZATION_EXCEPTION_INFORMATION_ADDRESS = 0x0000202a, // 虚拟化异常信息地址低 32 位
    XSS_EXITING_BITMAP = 0x0000202c,                     // XSS 退出位图
    ENCLS_EXITING_BITMAP = 0x0000202e,                   // ENCLS 退出位图
    SUB_PAGE_PERMISSION_TABLE_POINTER = 0x00002030,      // 子页权限表指针
    TSC_MULTIPLIER = 0x00002032,                         // TSC 倍频值
    ENCLV_EXITING_BITMAP = 0x00002036,                   // ENCLV 退出位图
    GUEST_PHYSICAL_ADDRESS = 0x00002400,                 // 来宾物理地址（只读）
    VMCS_LINK_POINTER = 0x00002800,                      // VMCS 链接指针低 32 位
    VMCS_LINK_POINTER_HIGH = 0x00002801,                 // VMCS 链接指针高 32 位
    GUEST_DEBUG_CONTROL = 0x00002802,                    // 来宾调试控制寄存器
    GUEST_IA32_DEBUG_CTL_HIGH = 0x00002803,              // 来宾调试控制寄存器高 32 位
    GUEST_PAT = 0x00002804,                              // 来宾 PAT 寄存器
    GUEST_EFER = 0x00002806,                             // 来宾 EFER 寄存器
    GUEST_PERF_GLOBAL_CONTROL = 0x00002808,              // 来宾性能全局控制寄存器
    GUEST_PDPTE0 = 0x0000280a,                           // 来宾 PDPTE0（PAE/LA57 等）
    GUEST_PDPTE1 = 0x0000280c,                           // 来宾 PDPTE1
    GUEST_PDPTE2 = 0x0000280e,                           // 来宾 PDPTE2
    GUEST_PDPTE3 = 0x00002810,                           // 来宾 PDPTE3
    GUEST_BNDCFGS = 0x00002812,                          // 来宾 BNDCFGS
    GUEST_RTIT_CTL = 0x00002814,                         // 来宾 RTIT 控制
    GUEST_PKRS = 0x00002816,                             // 来宾 PKRS
    HOST_PAT = 0x00002c00,                               // 宿主 PAT 寄存器
    HOST_EFER = 0x00002c02,                              // 宿主 EFER 寄存器
    HOST_PERF_GLOBAL_CTRL = 0x00002c04,                  // 宿主性能全局控制寄存器
    HOST_PKRS = 0x00002c06,                              // 宿主 PKRS
    PIN_BASED_VM_EXEC_CONTROL = 0x00004000,              // 基于引脚的 VM 执行控制
    PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL = 0x00004002,// 主处理器执行控制
    EXCEPTION_BITMAP = 0x00004004,                       // 异常位图
    PAGE_FAULT_ERROR_CODE_MASK = 0x00004006,             // 缺页错误码掩码
    PAGE_FAULT_ERROR_CODE_MATCH = 0x00004008,            // 缺页错误码匹配值
    CR3_TARGET_COUNT = 0x0000400a,                       // CR3 目标数量
    VM_EXIT_CONTROLS = 0x0000400c,                       // VM-exit 控制字段
    VM_EXIT_MSR_STORE_COUNT = 0x0000400e,                // VM-exit MSR 保存数量
    VM_EXIT_MSR_LOAD_COUNT = 0x00004010,                 // VM-exit MSR 加载数量
    VM_ENTRY_CONTROLS = 0x00004012,                      // VM-entry 控制字段
    VM_ENTRY_MSR_LOAD_COUNT = 0x00004014,                // VM-entry MSR 加载数量
    VM_ENTRY_INTERRUPTION_INFO_FIELD = 0x00004016,       // VM-entry 中断信息字段
    VM_ENTRY_EXCEPTION_ERROR_CODE = 0x00004018,          // VM-entry 异常错误码
    VM_ENTRY_INSTRUCTION_LENGTH = 0x0000401a,            // VM-entry 指令长度
    TPR_THRESHOLD = 0x0000401c,                          // TPR 阈值
    SECONDARY_PROCESSOR_BASED_VM_EXEC_CONTROL = 0x0000401e, // 次级处理器执行控制
    CONTROL_PLE_GAP = 0x00004020,                        // PLE 间隔
    CONTROL_PLE_WINDOW = 0x00004022,                     // PLE 窗口
    VM_INSTRUCTION_ERROR = 0x00004400,                   // VM 指令错误码
    VM_EXIT_REASON = 0x00004402,                         // VM-exit 原因
    VM_EXIT_INTERRUPTION_INFORMATION = 0x00004404,       // VM-exit 中断信息
    VM_EXIT_INTERRUPTION_ERROR_CODE = 0x00004406,        // VM-exit 中断错误码
    IDT_VECTORING_INFO_FIELD = 0x00004408,               // IDT 向量信息字段
    IDT_VECTORING_ERROR_CODE = 0x0000440a,               // IDT 向量错误码
    VM_EXIT_INSTRUCTION_LENGTH = 0x0000440c,             // VM-exit 指令长度
    VM_EXIT_INSTRUCTION_INFORMATION = 0x0000440e,        // VM-exit 指令信息
    GUEST_ES_LIMIT = 0x00004800,                         // 来宾 ES 段界限
    GUEST_CS_LIMIT = 0x00004802,                         // 来宾 CS 段界限
    GUEST_SS_LIMIT = 0x00004804,                         // 来宾 SS 段界限
    GUEST_DS_LIMIT = 0x00004806,                         // 来宾 DS 段界限
    GUEST_FS_LIMIT = 0x00004808,                         // 来宾 FS 段界限
    GUEST_GS_LIMIT = 0x0000480a,                         // 来宾 GS 段界限
    GUEST_LDTR_LIMIT = 0x0000480c,                       // 来宾 LDTR 段界限
    GUEST_TR_LIMIT = 0x0000480e,                         // 来宾 TR 段界限
    GUEST_GDTR_LIMIT = 0x00004810,                       // 来宾 GDTR 界限
    GUEST_IDTR_LIMIT = 0x00004812,                       // 来宾 IDTR 界限
    GUEST_ES_ACCESS_RIGHTS = 0x00004814,                 // 来宾 ES 访问权限
    GUEST_CS_ACCESS_RIGHTS = 0x00004816,                 // 来宾 CS 访问权限
    GUEST_SS_ACCESS_RIGHTS = 0x00004818,                 // 来宾 SS 访问权限
    GUEST_DS_ACCESS_RIGHTS = 0x0000481a,                 // 来宾 DS 访问权限
    GUEST_FS_ACCESS_RIGHTS = 0x0000481c,                 // 来宾 FS 访问权限
    GUEST_GS_ACCESS_RIGHTS = 0x0000481e,                 // 来宾 GS 访问权限
    GUEST_LDTR_ACCESS_RIGHTS = 0x00004820,               // 来宾 LDTR 访问权限
    GUEST_TR_ACCESS_RIGHTS = 0x00004822,                 // 来宾 TR 访问权限
    GUEST_INTERRUPTIBILITY_STATE = 0x00004824,           // 来宾中断可接受状态
    GUEST_ACTIVITY_STATE = 0x00004826,                   // 来宾活动状态
    GUEST_SM_BASE = 0x00004828,                          // 来宾 SMBASE
    GUEST_SYSENTER_CS = 0x0000482a,                      // 来宾 SYSENTER CS
    GUEST_VMX_PREEMPTION_TIMER_VALUE = 0x0000482e,       // 来宾 VMX 预抢占计时器值
    HOST_IA32_SYSENTER_CS = 0x00004c00,                  // 宿主 SYSENTER CS
    CR0_GUEST_HOST_MASK = 0x00006000,                    // CR0 来宾/宿主掩码
    CR4_GUEST_HOST_MASK = 0x00006002,                    // CR4 来宾/宿主掩码
    CR0_READ_SHADOW = 0x00006004,                        // CR0 读影子值
    CR4_READ_SHADOW = 0x00006006,                        // CR4 读影子值
    CR3_TARGET_VALUE0 = 0x00006008,                      // CR3 目标值 0
    CR3_TARGET_VALUE1 = 0x0000600a,                      // CR3 目标值 1
    CR3_TARGET_VALUE2 = 0x0000600c,                      // CR3 目标值 2
    CR3_TARGET_VALUE3 = 0x0000600e,                      // CR3 目标值 3
    EXIT_QUALIFICATION = 0x00006400,                     // VM-exit 资格信息
    IO_RCX = 0x00006402,                                 // I/O 相关 RCX
    IO_RSI = 0x00006404,                                 // I/O 相关 RSI
    IO_RDI = 0x00006406,                                 // I/O 相关 RDI
    IO_RIP = 0x00006408,                                 // I/O 相关 RIP
    GUEST_LINEAR_ADDRESS = 0x0000640a,                   // 来宾线性地址
    GUEST_CR0 = 0x00006800,                              // 来宾 CR0
    GUEST_CR3 = 0x00006802,                              // 来宾 CR3
    GUEST_CR4 = 0x00006804,                              // 来宾 CR4
    GUEST_ES_BASE = 0x00006806,                          // 来宾 ES 基址
    GUEST_CS_BASE = 0x00006808,                          // 来宾 CS 基址
    GUEST_SS_BASE = 0x0000680a,                          // 来宾 SS 基址
    GUEST_DS_BASE = 0x0000680c,                          // 来宾 DS 基址
    GUEST_FS_BASE = 0x0000680e,                          // 来宾 FS 基址
    GUEST_GS_BASE = 0x00006810,                          // 来宾 GS 基址
    GUEST_LDTR_BASE = 0x00006812,                        // 来宾 LDTR 基址
    GUEST_TR_BASE = 0x00006814,                          // 来宾 TR 基址
    GUEST_GDTR_BASE = 0x00006816,                        // 来宾 GDTR 基址
    GUEST_IDTR_BASE = 0x00006818,                        // 来宾 IDTR 基址
    GUEST_DR7 = 0x0000681a,                              // 来宾 DR7
    GUEST_RSP = 0x0000681c,                              // 来宾 RSP
    GUEST_RIP = 0x0000681e,                              // 来宾 RIP
    GUEST_RFLAGS = 0x00006820,                           // 来宾 RFLAGS
    GUEST_PENDING_DEBUG_EXCEPTION = 0x00006822,          // 来宾挂起的调试异常
    GUEST_SYSENTER_ESP = 0x00006824,                     // 来宾 SYSENTER ESP
    GUEST_SYSENTER_EIP = 0x00006826,                     // 来宾 SYSENTER EIP
    GUEST_S_CET = 0x00006828,                            // 来宾 CET 相关寄存器
    GUEST_SSP = 0x0000682a,                              // 来宾 SSP
    GUEST_INTERRUPT_SSP_TABLE_ADDR = 0x0000682c,         // 来宾中断 SSP 表地址
    HOST_CR0 = 0x00006c00,                               // 宿主 CR0
    HOST_CR3 = 0x00006c02,                               // 宿主 CR3
    HOST_CR4 = 0x00006c04,                               // 宿主 CR4
    HOST_FS_BASE = 0x00006c06,                           // 宿主 FS 基址
    HOST_GS_BASE = 0x00006c08,                           // 宿主 GS 基址
    HOST_TR_BASE = 0x00006c0a,                           // 宿主 TR 基址
    HOST_GDTR_BASE = 0x00006c0c,                         // 宿主 GDTR 基址
    HOST_IDTR_BASE = 0x00006c0e,                         // 宿主 IDTR 基址
    HOST_IA32_SYSENTER_ESP = 0x00006c10,                 // 宿主 SYSENTER ESP
    HOST_IA32_SYSENTER_EIP = 0x00006c12,                 // 宿主 SYSENTER EIP
    HOST_RSP = 0x00006c14,                               // 宿主 RSP
    HOST_RIP = 0x00006c16,                               // 宿主 RIP
    Host_IA32_S_CET = 0x00006c18,                        // 宿主 CET 相关寄存器
    HOST_SSP = 0x00006c1a,                               // 宿主 SSP
    Host_IA32_INTERRUPT_SSP_TABLE_ADDR = 0x00006c1c,     // 宿主中断 SSP 表地址
}; // 定义块结束

// 次级处理器执行控制字段（64 位位域结构）
// 原理：该字段控制“次级功能开关”，例如 EPT、VPID、INVPCID 等硬件加速能力。
// 作用：启用后可以减少 VM-exit 并提升性能，或让某些指令在来宾中直接执行。
// 注意：位的取值必须满足 IA32_VMX_PROCBASED_CTLS2 的允许规则。
union __vmx_secondary_processor_based_control // 次级处理器执行控制位域联合体
{ // 定义块开始
    unsigned __int64 all;                                // 该联合体的完整 64 位值
    struct // 位域字段定义
    { // 定义块开始
        unsigned __int64 virtualize_apic_accesses : 1;   // 位 0：是否虚拟化 APIC 访问
        unsigned __int64 enable_ept : 1;                 // 位 1：是否启用 EPT
        unsigned __int64 descriptor_table_exiting : 1;   // 位 2：是否拦截描述符表访问
        unsigned __int64 enable_rdtscp : 1;              // 位 3：是否允许 RDTSCP 指令
        unsigned __int64 virtualize_x2apic_mode : 1;     // 位 4：是否虚拟化 x2APIC 模式
        unsigned __int64 enable_vpid : 1;                // 位 5：是否启用 VPID
        unsigned __int64 wbinvd_exiting : 1;             // 位 6：是否拦截 WBINVD
        unsigned __int64 unrestricted_guest : 1;         // 位 7：是否允许非受限来宾
        unsigned __int64 apic_register_virtualization : 1; // 位 8：是否虚拟化 APIC 寄存器
        unsigned __int64 virtual_interrupt_delivery : 1; // 位 9：是否启用虚拟中断投递
        unsigned __int64 pause_loop_exiting : 1;         // 位 10：是否启用 PAUSE 循环退出
        unsigned __int64 rdrand_exiting : 1;             // 位 11：是否拦截 RDRAND
        unsigned __int64 enable_invpcid : 1;             // 位 12：是否允许 INVPCID
        unsigned __int64 enable_vmfunc : 1;              // 位 13：是否启用 VMFUNC
        unsigned __int64 vmcs_shadowing : 1;             // 位 14：是否启用 VMCS Shadowing
        unsigned __int64 enable_encls_exiting : 1;       // 位 15：是否拦截 ENCLS
        unsigned __int64 rdseed_exiting : 1;             // 位 16：是否拦截 RDSEED
        unsigned __int64 enable_pml : 1;                 // 位 17：是否启用 PML
        unsigned __int64 ept_violation : 1;              // 位 18：是否启用 EPT 违规功能
        unsigned __int64 conceal_vmx_from_pt : 1;        // 位 19：是否对 PT 隐藏 VMX
        unsigned __int64 enable_xsaves_xrstors : 1;       // 位 20：是否允许 XSAVES/XRSTORS
        unsigned __int64 reserved_0 : 1;                 // 位 21：保留位（按手册要求设置）
        unsigned __int64 mode_based_execute_control_ept : 1; // 位 22：是否启用 EPT 模式基执行控制
        unsigned __int64 sub_page_write_permission_for_ept : 1; // 位 23：是否启用 EPT 子页写权限
        unsigned __int64 intel_pt_uses_guest_physical_address : 1; // 位 24：PT 使用来宾物理地址
        unsigned __int64 use_tsc_scaling : 1;            // 位 25：是否启用 TSC 缩放
        unsigned __int64 enable_user_wait_and_pause : 1; // 位 26：是否允许 UMONITOR/UMWAIT/TPAUSE
        unsigned __int64 enable_pconfig : 1;             // 位 27：是否允许 PCONFIG
        unsigned __int64 enable_enclv_exiting : 1;       // 位 28：是否拦截 ENCLV
    }; // 定义块结束
}; // 定义块结束

// 主处理器执行控制字段（64 位位域结构）
// 原理：该字段决定哪些指令/事件触发 VM-exit（例如 HLT、INVLPG、RDTSC）。
// 作用：用于拦截敏感操作、实现虚拟化策略与安全控制。
// 注意：位的取值必须满足 IA32_VMX_PROCBASED_CTLS 的允许规则。
union __vmx_primary_processor_based_control // 主处理器执行控制位域联合体
{ // 定义块开始
    unsigned __int64 all;                                // 该联合体的完整 64 位值
    struct // 位域字段定义
    { // 定义块开始
        unsigned __int64 reserved_0 : 2;                 // 位 1:0 保留
        unsigned __int64 interrupt_window_exiting : 1;   // 位 2：中断窗口退出
        unsigned __int64 use_tsc_offsetting : 1;         // 位 3：使用 TSC 偏移
        unsigned __int64 reserved_1 : 3;                 // 位 6:4 保留
        unsigned __int64 hlt_exiting : 1;                // 位 7：HLT 退出
        unsigned __int64 reserved_2 : 1;                 // 位 8 保留
        unsigned __int64 invlpg_exiting : 1;             // 位 9：INVLPG 退出
        unsigned __int64 mwait_exiting : 1;              // 位 10：MWAIT 退出
        unsigned __int64 rdpmc_exiting : 1;              // 位 11：RDPMC 退出
        unsigned __int64 rdtsc_exiting : 1;              // 位 12：RDTSC 退出
        unsigned __int64 reserved_3 : 2;                 // 位 14:13 保留
        unsigned __int64 cr3_load_exiting : 1;           // 位 15：CR3 加载退出
        unsigned __int64 cr3_store_exiting : 1;          // 位 16：CR3 存储退出
        unsigned __int64 activate_tertiary_controls : 1; // 位 17：启用第三级控制
        unsigned __int64 reserved_4 : 1;                 // 位 18 保留
        unsigned __int64 cr8_load_exiting : 1;           // 位 19：CR8 加载退出
        unsigned __int64 cr8_store_exiting : 1;          // 位 20：CR8 存储退出
        unsigned __int64 use_tpr_shadow : 1;             // 位 21：使用 TPR Shadow
        unsigned __int64 nmi_window_exiting : 1;         // 位 22：NMI 窗口退出
        unsigned __int64 mov_dr_exiting : 1;             // 位 23：MOV DR 退出
        unsigned __int64 unconditional_io_exiting : 1;   // 位 24：无条件 I/O 退出
        unsigned __int64 use_io_bitmaps : 1;             // 位 25：使用 I/O 位图
        unsigned __int64 reserved_5 : 1;                 // 位 26 保留
        unsigned __int64 monitor_trap_flag : 1;          // 位 27：监控陷阱标志（单步）
        unsigned __int64 use_msr_bitmaps : 1;            // 位 28：使用 MSR 位图
        unsigned __int64 monitor_exiting : 1;            // 位 29：MONITOR 退出
        unsigned __int64 pause_exiting : 1;              // 位 30：PAUSE 退出
        unsigned __int64 active_secondary_controls : 1;  // 位 31：启用次级控制
    }; // 定义块结束
}; // 定义块结束

// 基于引脚的执行控制 MSR 位域结构
// 原理：pinbased 控制主要影响外部中断/NMI 等异步事件是否导致 VM-exit。
// 作用：决定来宾对外部中断的可见性以及中断处理方式。
// 注意：位的取值必须满足 IA32_VMX_PINBASED_CTLS 的允许规则。
union __vmx_pinbased_control_msr // 基于引脚的执行控制位域联合体
{ // 定义块开始
    unsigned __int64 all;                                // 该联合体的完整 64 位值
    struct // 位域字段定义
    { // 定义块开始
        unsigned __int64 external_interrupt_exiting : 1; // 位 0：外部中断退出
        unsigned __int64 reserved_0 : 2;                 // 位 2:1 保留
        unsigned __int64 nmi_exiting : 1;                // 位 3：NMI 退出
        unsigned __int64 reserved_1 : 1;                 // 位 4 保留
        unsigned __int64 virtual_nmis : 1;               // 位 5：虚拟 NMI
        unsigned __int64 activate_vmxpreemption_timer : 1; // 位 6：启用 VMX 预抢占计时器
        unsigned __int64 process_posted_interrupts : 1;  // 位 7：处理投递中断
    }; // 定义块结束
}; // 定义块结束

// 真实控制设置（True Controls），来自相关 MSR
// 原理：MSR 中的 allowed_0/allowed_1 位表示“必须为 0/1 的控制位”。
// 作用：VMM 必须按该规则配置控制字段，否则 VMX 指令会失败。
union __vmx_true_control_settings // 真实控制设置位域联合体
{ // 定义块开始
    unsigned __int64 all;                                // 该联合体的完整 64 位值
    struct // 位域字段定义
    { // 定义块开始
        unsigned __int32 allowed_0_settings;             // 允许为 0 的控制位集合
        unsigned __int32 allowed_1_settings;             // 允许为 1 的控制位集合
    }; // 定义块结束
}; // 定义块结束

// 中断命令寄存器（ICR）的位域结构
// 原理：ICR 用于向指定 CPU 发送 IPI（处理器间中断）。
// 作用：多核情况下用于唤醒、同步或通知其它 CPU。
union __interrupt_command_register // 中断命令寄存器位域联合体
{ // 定义块开始
    unsigned __int64 all;                                // 该联合体的完整 64 位值
    struct // 位域字段定义
    { // 定义块开始
        unsigned __int64 vector : 8;                     // 中断向量号
        unsigned __int64 delivery_mode : 3;              // 传递模式（固定、NMI、INIT 等）
        unsigned __int64 destination_mode : 1;           // 目标模式（物理或逻辑）
        unsigned __int64 delivery_status : 1;            // 传递状态（是否在发送）
        unsigned __int64 reserved_0 : 1;                 // 保留位
        unsigned __int64 level : 1;                      // 触发电平（高/低）
        unsigned __int64 trigger_mode : 1;               // 触发方式（边沿/电平）
        unsigned __int64 reserved_1 : 2;                 // 保留位
        unsigned __int64 destination_short : 2;          // 简写目标（自身/所有/除自身外）
        unsigned __int64 reserved_3 : 35;                // 保留位
        unsigned __int64 destination : 8;                // 目标 APIC ID
    }; // 定义块结束
}; // 定义块结束

// VM-entry 控制字段位域结构
// 原理：决定进入来宾时需要加载哪些寄存器或模式（如 EFER、PAT、CET）。
// 作用：确保来宾进入时的 CPU 状态正确，避免异常或崩溃。
union __vmx_entry_control // VM-entry 控制位域联合体
{ // 定义块开始
    unsigned __int64 all;                                // 该联合体的完整 64 位值
    struct // 位域字段定义
    { // 定义块开始
        unsigned __int64 reserved_0 : 2;                 // 位 1:0 保留
        unsigned __int64 load_dbg_controls : 1;          // 位 2：是否加载调试控制寄存器
        unsigned __int64 reserved_1 : 6;                 // 位 8:3 保留
        unsigned __int64 ia32e_mode_guest : 1;           // 位 9：来宾是否为 IA-32e 模式
        unsigned __int64 entry_to_smm : 1;               // 位 10：是否进入 SMM
        unsigned __int64 deactivate_dual_monitor_treament : 1; // 位 11：禁用双重监控处理
        unsigned __int64 reserved_2 : 1;                 // 位 12 保留
        unsigned __int64 load_ia32_perf_global_control : 1; // 位 13：加载性能全局控制寄存器
        unsigned __int64 load_ia32_pat : 1;              // 位 14：加载 PAT
        unsigned __int64 load_ia32_efer : 1;             // 位 15：加载 EFER
        unsigned __int64 load_ia32_bndcfgs : 1;          // 位 16：加载 BNDCFGS
        unsigned __int64 conceal_vmx_from_pt : 1;        // 位 17：对 PT 隐藏 VMX
        unsigned __int64 load_ia32_rtit_ctl : 1;         // 位 18：加载 RTIT 控制
        unsigned __int64 reserved_3 : 1;                 // 位 19 保留
        unsigned __int64 load_cet_state : 1;             // 位 20：加载 CET 状态
        unsigned __int64 load_guest_ia32_lbr_ctl : 1;     // 位 21：加载来宾 LBR 控制
        unsigned __int64 load_pkrs : 1;                  // 位 22：加载 PKRS
    }; // 定义块结束
}; // 定义块结束

// VM-exit 控制字段位域结构
// 原理：决定退出到宿主时保存/加载哪些状态。
// 作用：保证宿主在 VM-exit 后能继续稳定运行，并保留必要调试信息。
union __vmx_exit_control // VM-exit 控制位域联合体
{ // 定义块开始
    unsigned __int64 all;                                // 该联合体的完整 64 位值
    struct // 位域字段定义
    { // 定义块开始
        unsigned __int64 reserved_0 : 2;                 // 位 1:0 保留
        unsigned __int64 save_dbg_controls : 1;          // 位 2：保存调试控制寄存器
        unsigned __int64 reserved_1 : 6;                 // 位 8:3 保留
        unsigned __int64 host_address_space_size : 1;    // 位 9：宿主地址空间大小（0=32 位，1=64 位）
        unsigned __int64 reserved_2 : 2;                 // 位 11:10 保留
        unsigned __int64 load_ia32_perf_global_control : 1; // 位 12：加载性能全局控制
        unsigned __int64 reserved_3 : 2;                 // 位 14:13 保留
        unsigned __int64 acknowledge_interrupt_on_exit : 1; // 位 15：退出时确认中断
        unsigned __int64 reserved_4 : 2;                 // 位 17:16 保留
        unsigned __int64 save_ia32_pat : 1;              // 位 18：保存 PAT
        unsigned __int64 load_ia32_pat : 1;              // 位 19：加载 PAT
        unsigned __int64 save_ia32_efer : 1;             // 位 20：保存 EFER
        unsigned __int64 load_ia32_efer : 1;             // 位 21：加载 EFER
        unsigned __int64 save_vmx_preemption_timer_value : 1; // 位 22：保存 VMX 预抢占计时器值
        unsigned __int64 clear_ia32_bndcfgs : 1;         // 位 23：清除 BNDCFGS
        unsigned __int64 conceal_vmx_from_pt : 1;        // 位 24：对 PT 隐藏 VMX
        unsigned __int64 clear_ia32_rtit_ctl : 1;        // 位 25：清除 RTIT 控制
        unsigned __int64 clear_ia32_lbr_ctl : 1;         // 位 26：清除 LBR 控制
        unsigned __int64 reserved_5 : 1;                 // 位 27 保留
        unsigned __int64 load_cet_state : 1;             // 位 28：加载 CET 状态
        unsigned __int64 load_pkrs : 1;                  // 位 29：加载 PKRS
        unsigned __int64 save_ia32_perf_global_ctl : 1;  // 位 30：保存性能全局控制
        unsigned __int64 activate_secondary_controls : 1;// 位 31：启用次级控制
    }; // 定义块结束
}; // 定义块结束

// 来宾挂起的调试异常位域结构
// 原理：记录来宾中断点与单步等调试状态，用于 VM-exit 后恢复正确的调试语义。
// 作用：保证调试相关行为在虚拟化环境下不丢失。
union __vmx_pending_debug_exceptions // 挂起调试异常位域联合体
{ // 定义块开始
    unsigned __int64 all;                                // 该联合体的完整 64 位值
    struct // 位域字段定义
    { // 定义块开始
        unsigned __int64 b0 : 1;                         // 断点 0 状态
        unsigned __int64 b1 : 1;                         // 断点 1 状态
        unsigned __int64 b2 : 1;                         // 断点 2 状态
        unsigned __int64 b3 : 1;                         // 断点 3 状态
        unsigned __int64 reserved1 : 8;                  // 保留位
        unsigned __int64 enabled_bp : 1;                 // 是否启用断点
        unsigned __int64 reserved2 : 1;                  // 保留位
        unsigned __int64 bs : 1;                         // 单步标志
        unsigned __int64 reserved3 : 1;                  // 保留位
        unsigned __int64 rtm : 1;                        // RTM 相关标志
        unsigned __int64 reserved4 : 47;                 // 保留位
    }; // 定义块结束

}; // 定义块结束

// 来宾中断可接受状态位域结构
// 原理：表示来宾当前是否允许接收中断（例如被 STI/MOV SS 暂时阻塞）。
// 作用：VMM 在注入中断时需要检查该状态，避免破坏指令语义。
union __vmx_interruptibility_state // 中断可接受状态位域联合体
{ // 定义块开始
    unsigned __int64 all;                                // 该联合体的完整 64 位值
    struct // 位域字段定义
    { // 定义块开始
        unsigned __int64 blocking_by_sti : 1;            // STI 指令导致的中断阻塞
        unsigned __int64 blocking_by_mov_ss : 1;         // MOV SS 指令导致的中断阻塞
        unsigned __int64 blocking_by_smi : 1;            // SMI 导致的中断阻塞
        unsigned __int64 blocking_by_nmi : 1;            // NMI 导致的中断阻塞
        unsigned __int64 enclave_interruption : 1;       // SGX enclave 中断状态
        unsigned __int64 reserved : 27;                  // 保留位
    }; // 定义块结束
}; // 定义块结束

void fill_vmcs(__vcpu* vcpu, void* guest_rsp);           // 根据 vCPU 状态填充 VMCS（函数声明）

#endif // !_VMCS_H                                       // 结束包含保护


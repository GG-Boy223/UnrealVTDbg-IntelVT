#include "HypervisorInternal.h"

namespace hv
{
    /// <summary>
    /// 转储整个 VMCS 结构（用于调试）
    /// </summary>
    void dump_vmcs() // 输出完整 VMCS 字段，便于调试
    {
        // 原 vmcs_dump_lock 已删除：用一把跨核自旋锁保护调试输出，代价大于收益。
        // 说明：本函数中约 150 行 LogDump 在未定义 DEBUG_WinSys 时是空宏（见 Common/Ring0/SymbolicAccess/Utils/Log.h:60），
        // 当前构建真正执行的只有首尾两行 DbgPrintEx + 150 次 VMREAD，持锁时间本就极短，锁无实际意义。
        // 并发转储时输出可能交错，属调试功能的可接受代价。

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "-----------------------------------VMCS CORE %u DUMP-----------------------------------\r\n", KeGetCurrentProcessorIndex()); // 打印标题并标注 CPU 编号

        // 来宾自然宽度寄存器状态字段
        LogDump("GUEST_CR0: 0x%llX", vmread(GUEST_CR0)); // 输出来宾 CR0
        LogDump("GUEST_CR3: 0x%llX", vmread(GUEST_CR3)); // 输出来宾 CR3
        LogDump("GUEST_CR4: 0x%llX", vmread(GUEST_CR4)); // 输出来宾 CR4
        LogDump("GUEST_ES_BASE: 0x%llX", vmread(GUEST_ES_BASE)); // 输出来宾 ES 段基址
        LogDump("GUEST_CS_BASE: 0x%llX", vmread(GUEST_CS_BASE)); // 输出来宾 CS 段基址
        LogDump("GUEST_SS_BASE: 0x%llX", vmread(GUEST_SS_BASE)); // 输出来宾 SS 段基址
        LogDump("GUEST_DS_BASE: 0x%llX", vmread(GUEST_DS_BASE)); // 输出来宾 DS 段基址
        LogDump("GUEST_FS_BASE: 0x%llX", vmread(GUEST_FS_BASE)); // 输出来宾 FS 段基址
        LogDump("GUEST_GS_BASE: 0x%llX", vmread(GUEST_GS_BASE)); // 输出来宾 GS 段基址
        LogDump("GUEST_LDTR_BASE: 0x%llX", vmread(GUEST_LDTR_BASE)); // 输出来宾 LDTR 基址
        LogDump("GUEST_TR_BASE: 0x%llX", vmread(GUEST_TR_BASE)); // 输出来宾 TR 基址
        LogDump("GUEST_GDTR_BASE: 0x%llX", vmread(GUEST_GDTR_BASE)); // 输出来宾 GDTR 基址
        LogDump("GUEST_IDTR_BASE: 0x%llX", vmread(GUEST_IDTR_BASE)); // 输出来宾 IDTR 基址
        LogDump("GUEST_DR7: 0x%llX", vmread(GUEST_DR7)); // 输出来宾 DR7
        LogDump("GUEST_RSP: 0x%llX", vmread(GUEST_RSP)); // 输出来宾 RSP
        LogDump("GUEST_RIP: 0x%llX", vmread(GUEST_RIP)); // 输出来宾 RIP
        LogDump("GUEST_RFLAGS: 0x%llX", vmread(GUEST_RFLAGS)); // 输出来宾 RFLAGS
        LogDump("GUEST_SYSENTER_ESP: 0x%llX", vmread(GUEST_SYSENTER_ESP)); // 输出来宾 SYSENTER ESP
        LogDump("GUEST_SYSENTER_EIP: 0x%llX", vmread(GUEST_SYSENTER_EIP)); // 输出来宾 SYSENTER EIP
        LogDump("GUEST_S_CET: 0x%llX", vmread(GUEST_S_CET)); // 输出来宾 CET 相关寄存器
        LogDump("GUEST_SSP: 0x%llX", vmread(GUEST_SSP)); // 输出来宾 SSP
        LogDump("GUEST_INTERRUPT_SSP_TABLE_ADDR: 0x%llX", vmread(GUEST_INTERRUPT_SSP_TABLE_ADDR)); // 输出来宾中断 SSP 表地址

        // 64 位来宾寄存器状态字段
        LogDump("GUEST_VMCS_LINK_POINTER: 0x%llX", vmread(VMCS_LINK_POINTER)); // 输出 VMCS 链接指针
        LogDump("GUEST_DEBUG_CONTROL: 0x%llX", vmread(GUEST_DEBUG_CONTROL)); // 输出来宾调试控制寄存器
        LogDump("GUEST_PAT: 0x%llX", vmread(GUEST_PAT)); // 输出来宾 PAT
        LogDump("GUEST_EFER: 0x%llX", vmread(GUEST_EFER)); // 输出来宾 EFER
        LogDump("GUEST_PERF_GLOBAL_CONTROL: 0x%llX", vmread(GUEST_PERF_GLOBAL_CONTROL)); // 输出来宾性能全局控制
        LogDump("GUEST_PDPTE0: 0x%llX", vmread(GUEST_PDPTE0)); // 输出来宾 PDPTE0
        LogDump("GUEST_PDPTE1: 0x%llX", vmread(GUEST_PDPTE1)); // 输出来宾 PDPTE1
        LogDump("GUEST_PDPTE2: 0x%llX", vmread(GUEST_PDPTE2)); // 输出来宾 PDPTE2
        LogDump("GUEST_PDPTE3: 0x%llX", vmread(GUEST_PDPTE3)); // 输出来宾 PDPTE3
        LogDump("GUEST_BNDCFGS: 0x%llX", vmread(GUEST_BNDCFGS)); // 输出来宾 BNDCFGS
        LogDump("GUEST_RTIT_CTL: 0x%llX", vmread(GUEST_RTIT_CTL)); // 输出来宾 RTIT 控制
        LogDump("GUEST_PKRS: 0x%llX", vmread(GUEST_PKRS)); // 输出来宾 PKRS

        // 32 位来宾寄存器状态字段
        LogDump("GUEST_ES_LIMIT: 0x%llX", vmread(GUEST_ES_LIMIT)); // 输出来宾 ES 段界限
        LogDump("GUEST_CS_LIMIT: 0x%llX", vmread(GUEST_CS_LIMIT)); // 输出来宾 CS 段界限
        LogDump("GUEST_SS_LIMIT: 0x%llX", vmread(GUEST_SS_LIMIT)); // 输出来宾 SS 段界限
        LogDump("GUEST_DS_LIMIT: 0x%llX", vmread(GUEST_DS_LIMIT)); // 输出来宾 DS 段界限
        LogDump("GUEST_FS_LIMIT: 0x%llX", vmread(GUEST_FS_LIMIT)); // 输出来宾 FS 段界限
        LogDump("GUEST_GS_LIMIT: 0x%llX", vmread(GUEST_GS_LIMIT)); // 输出来宾 GS 段界限
        LogDump("GUEST_LDTR_LIMIT: 0x%llX", vmread(GUEST_LDTR_LIMIT)); // 输出来宾 LDTR 段界限
        LogDump("GUEST_TR_LIMIT: 0x%llX", vmread(GUEST_TR_LIMIT)); // 输出来宾 TR 段界限
        LogDump("GUEST_GDTR_LIMIT: 0x%llX", vmread(GUEST_GDTR_LIMIT)); // 输出来宾 GDTR 界限
        LogDump("GUEST_IDTR_LIMIT: 0x%llX", vmread(GUEST_IDTR_LIMIT)); // 输出来宾 IDTR 界限
        LogDump("GUEST_ES_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_ES_ACCESS_RIGHTS)); // 输出来宾 ES 访问权限
        LogDump("GUEST_CS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_CS_ACCESS_RIGHTS)); // 输出来宾 CS 访问权限
        LogDump("GUEST_SS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_SS_ACCESS_RIGHTS)); // 输出来宾 SS 访问权限
        LogDump("GUEST_DS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_DS_ACCESS_RIGHTS)); // 输出来宾 DS 访问权限
        LogDump("GUEST_FS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_FS_ACCESS_RIGHTS)); // 输出来宾 FS 访问权限
        LogDump("GUEST_GS_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_GS_ACCESS_RIGHTS)); // 输出来宾 GS 访问权限
        LogDump("GUEST_LDTR_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_LDTR_ACCESS_RIGHTS)); // 输出来宾 LDTR 访问权限
        LogDump("GUEST_TR_ACCESS_RIGHTS: 0x%llX", vmread(GUEST_TR_ACCESS_RIGHTS)); // 输出来宾 TR 访问权限
        LogDump("GUEST_INTERRUPTIBILITY_STATE: 0x%llX", vmread(GUEST_INTERRUPTIBILITY_STATE)); // 输出来宾中断可接受状态
        LogDump("GUEST_ACTIVITY_STATE: 0x%llX", vmread(GUEST_ACTIVITY_STATE)); // 输出来宾活动状态
        LogDump("GUEST_SMBASE: 0x%llX", vmread(GUEST_SM_BASE)); // 输出来宾 SMBASE
        LogDump("GUEST_SYSENTER_CS: 0x%llX", vmread(GUEST_SYSENTER_CS)); // 输出来宾 SYSENTER CS
        LogDump("GUEST_VMX_PREEMPTION_TIMER_VALUE: 0x%llX", vmread(GUEST_VMX_PREEMPTION_TIMER_VALUE)); // 输出来宾 VMX 预抢占计时器值

        // 16 位来宾寄存器状态字段
        LogDump("GUEST_ES_SELECTOR: 0x%llX", vmread(GUEST_ES_SELECTOR)); // 输出来宾 ES 选择子
        LogDump("GUEST_CS_SELECTOR: 0x%llX", vmread(GUEST_CS_SELECTOR)); // 输出来宾 CS 选择子
        LogDump("GUEST_SS_SELECTOR: 0x%llX", vmread(GUEST_SS_SELECTOR)); // 输出来宾 SS 选择子
        LogDump("GUEST_DS_SELECTOR: 0x%llX", vmread(GUEST_DS_SELECTOR)); // 输出来宾 DS 选择子
        LogDump("GUEST_FS_SELECTOR: 0x%llX", vmread(GUEST_FS_SELECTOR)); // 输出来宾 FS 选择子
        LogDump("GUEST_GS_SELECTOR: 0x%llX", vmread(GUEST_GS_SELECTOR)); // 输出来宾 GS 选择子
        LogDump("GUEST_LDTR_SELECTOR: 0x%llX", vmread(GUEST_LDTR_SELECTOR)); // 输出来宾 LDTR 选择子
        LogDump("GUEST_TR_SELECTOR: 0x%llX", vmread(GUEST_TR_SELECTOR)); // 输出来宾 TR 选择子
        LogDump("GUEST_GUEST_INTERRUPT_STATUS: 0x%llX", vmread(GUEST_INTERRUPT_STATUS)); // 输出来宾中断状态
        LogDump("GUEST_PML_INDEX: 0x%llX", vmread(GUEST_PML_INDEX)); // 输出来宾 PML 索引

        // 宿主自然宽度寄存器状态字段
        LogDump("HOST_CR0: 0x%llX", vmread(HOST_CR0)); // 输出宿主 CR0
        LogDump("HOST_CR3: 0x%llX", vmread(HOST_CR3)); // 输出宿主 CR3
        LogDump("HOST_CR4: 0x%llX", vmread(HOST_CR4)); // 输出宿主 CR4
        LogDump("HOST_FS_BASE: 0x%llX", vmread(HOST_FS_BASE)); // 输出宿主 FS 基址
        LogDump("HOST_GS_BASE: 0x%llX", vmread(HOST_GS_BASE)); // 输出宿主 GS 基址
        LogDump("HOST_TR_BASE: 0x%llX", vmread(HOST_TR_BASE)); // 输出宿主 TR 基址
        LogDump("HOST_GDTR_BASE: 0x%llX", vmread(HOST_GDTR_BASE)); // 输出宿主 GDTR 基址
        LogDump("HOST_IDTR_BASE: 0x%llX", vmread(HOST_IDTR_BASE)); // 输出宿主 IDTR 基址
        LogDump("HOST_SYSENTER_ESP: 0x%llX", vmread(HOST_IA32_SYSENTER_ESP)); // 输出宿主 SYSENTER ESP
        LogDump("HOST_SYSENTER_EIP: 0x%llX", vmread(HOST_IA32_SYSENTER_EIP)); // 输出宿主 SYSENTER EIP
        LogDump("HOST_RSP: 0x%llX", vmread(HOST_RSP)); // 输出宿主 RSP
        LogDump("HOST_RIP: 0x%llX", vmread(HOST_RIP)); // 输出宿主 RIP
        LogDump("HOST_S_CET: 0x%llX", vmread(Host_IA32_S_CET)); // 输出宿主 CET 相关寄存器
        LogDump("HOST_SSP: 0x%llX", vmread(HOST_SSP)); // 输出宿主 SSP
        LogDump("HOST_INTERRUPT_SSP_TABLE_ADDR: 0x%llX", vmread(Host_IA32_INTERRUPT_SSP_TABLE_ADDR)); // 输出宿主中断 SSP 表地址

        // 64 位宿主寄存器状态字段
        LogDump("HOST_PAT: 0x%llX", vmread(HOST_PAT)); // 输出宿主 PAT
        LogDump("HOST_EFER: 0x%llX", vmread(HOST_EFER)); // 输出宿主 EFER
        LogDump("HOST_PERF_GLOBAL_CTRL: 0x%llX", vmread(HOST_PERF_GLOBAL_CTRL)); // 输出宿主性能全局控制
        LogDump("HOST_PKRS: 0x%llX", vmread(HOST_PKRS)); // 输出宿主 PKRS

        // 32 位宿主寄存器状态字段
        LogDump("HOST_SYSENTER_CS: 0x%llX", vmread(HOST_IA32_SYSENTER_CS)); // 输出宿主 SYSENTER CS

        // 16 位宿主寄存器状态字段
        LogDump("HOST_ES_SELECTOR: 0x%llX", vmread(HOST_ES_SELECTOR)); // 输出宿主 ES 选择子
        LogDump("HOST_CS_SELECTOR: 0x%llX", vmread(HOST_CS_SELECTOR)); // 输出宿主 CS 选择子
        LogDump("HOST_SS_SELECTOR: 0x%llX", vmread(HOST_SS_SELECTOR)); // 输出宿主 SS 选择子
        LogDump("HOST_DS_SELECTOR: 0x%llX", vmread(HOST_DS_SELECTOR)); // 输出宿主 DS 选择子
        LogDump("HOST_FS_SELECTOR: 0x%llX", vmread(HOST_FS_SELECTOR)); // 输出宿主 FS 选择子
        LogDump("HOST_GS_SELECTOR: 0x%llX", vmread(HOST_GS_SELECTOR)); // 输出宿主 GS 选择子
        LogDump("HOST_TR_SELECTOR: 0x%llX", vmread(HOST_TR_SELECTOR)); // 输出宿主 TR 选择子

        // 自然宽度控制字段
        LogDump("CONTROL_CR0_GUEST_HOST_MASK: 0x%llX", vmread(CR0_GUEST_HOST_MASK)); // 输出 CR0 客/主掩码
        LogDump("CONTROL_CR4_GUEST_HOST_MASK: 0x%llX", vmread(CR4_GUEST_HOST_MASK)); // 输出 CR4 客/主掩码
        LogDump("CONTROL_CR0_READ_SHADOW: 0x%llX", vmread(CR0_READ_SHADOW)); // 输出 CR0 读影子值
        LogDump("CONTROL_CR4_READ_SHADOW: 0x%llX", vmread(CR4_READ_SHADOW)); // 输出 CR4 读影子值
        LogDump("CONTROL_CR3_TARGET_VALUE_0: 0x%llX", vmread(CR3_TARGET_VALUE0)); // 输出 CR3 目标值 0
        LogDump("CONTROL_CR3_TARGET_VALUE_1: 0x%llX", vmread(CR3_TARGET_VALUE1)); // 输出 CR3 目标值 1
        LogDump("CONTROL_CR3_TARGET_VALUE_2: 0x%llX", vmread(CR3_TARGET_VALUE2)); // 输出 CR3 目标值 2
        LogDump("CONTROL_CR3_TARGET_VALUE_3: 0x%llX", vmread(CR3_TARGET_VALUE3)); // 输出 CR3 目标值 3

        // 64 位控制字段
        LogDump("CONTROL_BITMAP_IO_A_ADDRESS: 0x%llX", vmread(IO_BITMAP_A_ADDRESS)); // 输出 IO 位图 A 地址
        LogDump("CONTROL_BITMAP_IO_B_ADDRESS: 0x%llX", vmread(IO_BITMAP_B_ADDRESS)); // 输出 IO 位图 B 地址
        LogDump("CONTROL_MSR_BITMAPS_ADDRESS: 0x%llX", vmread(MSR_BITMAP_ADDRESS)); // 输出 MSR 位图地址
        LogDump("CONTROL_VMEXIT_MSR_STORE_ADDRESS: 0x%llX", vmread(VM_EXIT_MSR_STORE_ADDR)); // 输出 VM-exit MSR 存储地址
        LogDump("CONTROL_VMEXIT_MSR_LOAD_ADDRESS: 0x%llX", vmread(VM_EXIT_MSR_LOAD_ADDR)); // 输出 VM-exit MSR 载入地址
        LogDump("CONTROL_VMENTER_MSR_LOAD_ADDRESS: 0x%llX", vmread(VM_ENTRY_MSR_LOAD_ADDR)); // 输出 VM-entry MSR 载入地址
        LogDump("CONTROL_VMCS_EXECUTIVE_POINTER: 0x%llX", vmread(VMCS_EXECUTIVE_POINTER)); // 输出 VMCS 执行指针
        LogDump("CONTROL_PML_ADDRESS: 0x%llX", vmread(PML_ADDRESS)); // 输出 PML 地址
        LogDump("CONTROL_TSC_OFFSET: 0x%llX", vmread(TSC_OFFSET)); // 输出 TSC 偏移量
        LogDump("CONTROL_VIRTUAL_APIC_ADDRESS: 0x%llX", vmread(VIRTUAL_APIC_PAGE_ADDR)); // 输出虚拟 APIC 地址
        LogDump("CONTROL_APIC_ACCESS_ADDRESS: 0x%llX", vmread(APIC_ACCESS_ADDRESS)); // 输出 APIC 访问地址
        LogDump("CONTROL_POSTED_INTERRUPT_DESCRIPTOR_ADDRESS: 0x%llX", vmread(POSTED_INTERRUPT_DESCRIPTOR_ADDRESS)); // 输出投递中断描述符地址
        LogDump("CONTROL_VM_FUNCTION_CONTROLS: 0x%llX", vmread(VM_FUNCTION_CONTROLS)); // 输出 VM 功能控制
        LogDump("CONTROL_EPT_POINTER: 0x%llX", vmread(EPT_POINTER)); // 输出 EPTP
        LogDump("CONTROL_EOI_EXIT_BITMAP_0: 0x%llX", vmread(EOI_EXIT_BITMAP_0)); // 输出 EOI 退出位图 0
        LogDump("CONTROL_EOI_EXIT_BITMAP_1: 0x%llX", vmread(EOI_EXIT_BITMAP_1)); // 输出 EOI 退出位图 1
        LogDump("CONTROL_EOI_EXIT_BITMAP_2: 0x%llX", vmread(EOI_EXIT_BITMAP_2)); // 输出 EOI 退出位图 2
        LogDump("CONTROL_EOI_EXIT_BITMAP_3: 0x%llX", vmread(EOI_EXIT_BITMAP_3)); // 输出 EOI 退出位图 3
        LogDump("CONTROL_EPTP_LIST_ADDRESS: 0x%llX", vmread(EPTP_LIST_ADDRESS)); // 输出 EPTP 列表地址
        LogDump("CONTROL_VMREAD_BITMAP_ADDRESS: 0x%llX", vmread(VMREAD_BITMAP_ADDRESS)); // 输出 VMREAD 位图地址
        LogDump("CONTROL_VMWRITE_BITMAP_ADDRESS: 0x%llX", vmread(VMWRITE_BITMAP_ADDRESS)); // 输出 VMWRITE 位图地址
        LogDump("CONTROL_VIRTUALIZATION_EXCEPTION_INFORMATION_ADDRESS: 0x%llX", vmread(VIRTUALIZATION_EXCEPTION_INFORMATION_ADDRESS)); // 输出虚拟化异常信息地址
        LogDump("CONTROL_XSS_EXITING_BITMAP: 0x%llX", vmread(XSS_EXITING_BITMAP)); // 输出 XSS 退出位图
        LogDump("CONTROL_ENCLS_EXITING_BITMAP: 0x%llX", vmread(ENCLS_EXITING_BITMAP)); // 输出 ENCLS 退出位图
        LogDump("CONTROL_SUB_PAGE_PERMISSION_TABLE_POINTER: 0x%llX", vmread(SUB_PAGE_PERMISSION_TABLE_POINTER)); // 输出子页权限表指针
        LogDump("CONTROL_TSC_MULTIPLIER: 0x%llX", vmread(TSC_MULTIPLIER)); // 输出 TSC 倍频
        LogDump("CONTROL_ENCLV_EXITING_BITMAP: 0x%llX", vmread(ENCLV_EXITING_BITMAP)); // 输出 ENCLV 退出位图

        // 32 位控制字段
        LogDump("CONTROL_PIN_BASED_VM_EXECUTION_CONTROLS: 0x%llX", vmread(PIN_BASED_VM_EXEC_CONTROL)); // 输出针脚相关执行控制
        LogDump("CONTROL_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS: 0x%llX", vmread(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL)); // 输出主处理器执行控制
        LogDump("CONTROL_EXCEPTION_BITMAP: 0x%llX", vmread(EXCEPTION_BITMAP)); // 输出异常位图
        LogDump("CONTROL_PAGE_FAULT_ERROR_CODE_MASK: 0x%llX", vmread(PAGE_FAULT_ERROR_CODE_MASK)); // 输出 #PF 错误码掩码
        LogDump("CONTROL_PAGE_FAULT_ERROR_CODE_MATCH: 0x%llX", vmread(PAGE_FAULT_ERROR_CODE_MATCH)); // 输出 #PF 错误码匹配值
        LogDump("CONTROL_CR3_TARGET_COUNT: 0x%llX", vmread(CR3_TARGET_COUNT)); // 输出 CR3 目标值数量
        LogDump("CONTROL_VM_EXIT_CONTROLS: 0x%llX", vmread(VM_EXIT_CONTROLS)); // 输出 VM-exit 控制
        LogDump("CONTROL_VM_EXIT_MSR_STORE_COUNT: 0x%llX", vmread(VM_EXIT_MSR_STORE_COUNT)); // 输出 VM-exit MSR 存储数量
        LogDump("CONTROL_VM_EXIT_MSR_LOAD_COUNT: 0x%llX", vmread(VM_EXIT_MSR_LOAD_COUNT)); // 输出 VM-exit MSR 载入数量
        LogDump("CONTROL_VM_ENTRY_CONTROLS: 0x%llX", vmread(VM_ENTRY_CONTROLS)); // 输出 VM-entry 控制
        LogDump("CONTROL_VM_ENTRY_MSR_LOAD_COUNT: 0x%llX", vmread(VM_ENTRY_MSR_LOAD_COUNT)); // 输出 VM-entry MSR 载入数量
        LogDump("CONTROL_VM_ENTRY_INTERRUPTION_INFORMATION_FIELD: 0x%llX", vmread(VM_ENTRY_INTERRUPTION_INFO_FIELD)); // 输出 VM-entry 中断信息字段
        LogDump("CONTROL_VM_ENTRY_EXCEPTION_ERROR_CODE: 0x%llX", vmread(VM_ENTRY_EXCEPTION_ERROR_CODE)); // 输出 VM-entry 异常错误码
        LogDump("CONTROL_VM_ENTRY_INSTRUCTION_LENGTH: 0x%llX", vmread(VM_ENTRY_INSTRUCTION_LENGTH)); // 输出 VM-entry 指令长度
        LogDump("CONTROL_TPR_THRESHOLD: 0x%llX", vmread(TPR_THRESHOLD)); // 输出 TPR 阈值
        LogDump("CONTROL_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS: 0x%llX", vmread(SECONDARY_PROCESSOR_BASED_VM_EXEC_CONTROL)); // 输出次级处理器执行控制
        LogDump("CONTROL_PLE_GAP: 0x%llX", vmread(CONTROL_PLE_GAP)); // 输出 PLE 间隔
        LogDump("CONTROL_PLE_WINDOW: 0x%llX", vmread(CONTROL_PLE_WINDOW)); // 输出 PLE 窗口

        // 16 位控制字段
        LogDump("CONTROL_VIRTUAL_PROCESSOR_IDENTIFIER: 0x%llX", vmread(VIRTUAL_PROCESSOR_ID)); // 输出虚拟处理器 ID
        LogDump("CONTROL_POSTED_INTERRUPT_NOTIFICATION_VECTOR: 0x%llX", vmread(POSTED_INTERRUPT_NOTIFICATION_VECTOR)); // 输出投递中断通知向量
        LogDump("CONTROL_EPTP_INDEX: 0x%llX", vmread(EPTP_INDEX)); // 输出 EPTP 索引

        // 自然宽度只读状态字段
        LogDump("EXIT_QUALIFICATION: 0x%llX", vmread(EXIT_QUALIFICATION)); // 输出 VM-exit 资格信息
        LogDump("IO_RCX: 0x%llX", vmread(IO_RCX)); // 输出 IO 指令相关 RCX
        LogDump("IO_RSI: 0x%llX", vmread(IO_RSI)); // 输出 IO 指令相关 RSI
        LogDump("IO_RDI: 0x%llX", vmread(IO_RDI)); // 输出 IO 指令相关 RDI
        LogDump("IO_RIP: 0x%llX", vmread(IO_RIP)); // 输出 IO 指令相关 RIP
        LogDump("GUEST_LINEAR_ADDRESS: 0x%llX", vmread(GUEST_LINEAR_ADDRESS)); // 输出来宾线性地址

        // 64 位只读状态字段
        LogDump("GUEST_PHYSICAL_ADDRESS: 0x%llX", vmread(GUEST_PHYSICAL_ADDRESS)); // 输出来宾物理地址

        // 32 位只读状态字段
        LogDump("VM_INSTRUCTION_ERROR: 0x%llX", vmread(VM_INSTRUCTION_ERROR)); // 输出 VM 指令错误码
        LogDump("EXIT_REASON: 0x%llX", vmread(VM_EXIT_REASON)); // 输出 VM-exit 原因
        LogDump("VM_EXIT_INTERRUPTION_INFORMATION: 0x%llX", vmread(VM_EXIT_INTERRUPTION_INFORMATION)); // 输出 VM-exit 中断信息
        LogDump("VM_EXIT_INTERRUPTION_ERROR_CODE: 0x%llX", vmread(VM_EXIT_INTERRUPTION_ERROR_CODE)); // 输出 VM-exit 中断错误码
        LogDump("IDT_VECTORING_INFORMATION_FIELD: 0x%llX", vmread(IDT_VECTORING_INFO_FIELD)); // 输出 IDT 向量信息
        LogDump("IDT_VECTORING_ERROR_CODE: 0x%llX", vmread(IDT_VECTORING_ERROR_CODE)); // 输出 IDT 向量错误码
        LogDump("VM_EXIT_INSTRUCTION_LENGTH: 0x%llX", vmread(VM_EXIT_INSTRUCTION_LENGTH)); // 输出 VM-exit 指令长度
        LogDump("VM_EXIT_INSTRUCTION_INFORMATION: 0x%llX", vmread(VM_EXIT_INSTRUCTION_INFORMATION)); // 输出 VM-exit 指令信息

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "-----------------------------------VMCS CORE %u DUMP-----------------------------------\r\n", KeGetCurrentProcessorIndex()); // 打印结束分隔线
    }
}

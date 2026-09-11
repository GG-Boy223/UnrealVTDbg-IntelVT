// 作用：当虚拟机退出时，VMM 通过这些结构解析退出原因和指令信息，并做出处理。
// 原理：CPU 在 VM-exit 时会把“退出原因/指令信息/资格信息”等写入 VMCS，
//       本文件的结构体用于把这些位域解析成可读的字段。

#pragma once                         // 只编译一次，避免重复包含

#ifndef _VMEXIT_HANDLER_H            // 如果还未定义该宏，进入包含保护
#define _VMEXIT_HANDLER_H            // 定义宏，标记该头文件已包含

// VM-exit 退出原因枚举
// 原理：硬件把 VM-exit 原因写入 VMCS 的 VM_EXIT_REASON 字段，本枚举用于解释该值
// 作用：VMM 根据原因分发处理逻辑（例如 CPUID、MSR、EPT 违规等）
enum __vm_exit_reason // VM-exit 退出原因
{ // 枚举开始
	EXIT_REASON_EXCEPTION_NMI,                       // 退出原因：异常或 NMI
	EXIT_REASON_EXTERNAL_INTERRUPT,                 // 退出原因：外部中断
	EXIT_REASON_TRIPLE_FAULT,                       // 退出原因：三重故障
	EXIT_REASON_INIT_SIGNAL,                        // 退出原因：INIT 信号
	EXIT_REASON_SIPI,                               // 退出原因：SIPI
	EXIT_REASON_SMI,                                // 退出原因：SMI
	EXIT_REASON_OTHER_SMI,                          // 退出原因：其他 SMI
	EXIT_REASON_INTERRUPT_WINDOW,                   // 退出原因：中断窗口
	EXIT_REASON_NMI_WINDOW,                         // 退出原因：NMI 窗口
	EXIT_REASON_TASK_SWITCH,                        // 退出原因：任务切换
	EXIT_REASON_CPUID,                              // 退出原因：执行 CPUID
	EXIT_REASON_GETSEC,                             // 退出原因：执行 GETSEC
	EXIT_REASON_HLT,                                // 退出原因：执行 HLT
	EXIT_REASON_INVD,                               // 退出原因：执行 INVD
	EXIT_REASON_INVLPG,                             // 退出原因：执行 INVLPG
	EXIT_REASON_RDPMC,                              // 退出原因：执行 RDPMC
	EXIT_REASON_RDTSC,                              // 退出原因：执行 RDTSC
	EXIT_REASON_RSM,                                // 退出原因：执行 RSM
	EXIT_REASON_VMCALL,                             // 退出原因：执行 VMCALL
	EXIT_REASON_VMCLEAR,                            // 退出原因：执行 VMCLEAR
	EXIT_REASON_VMLAUNCH,                           // 退出原因：执行 VMLAUNCH
	EXIT_REASON_VMPTRLD,                            // 退出原因：执行 VMPTRLD
	EXIT_REASON_VMPTRST,                            // 退出原因：执行 VMPTRST
	EXIT_REASON_VMREAD,                             // 退出原因：执行 VMREAD
	EXIT_REASON_VMRESUME,                           // 退出原因：执行 VMRESUME
	EXIT_REASON_VMWRITE,                            // 退出原因：执行 VMWRITE
	EXIT_REASON_VMXOFF,                             // 退出原因：执行 VMXOFF
	EXIT_REASON_VMXON,                              // 退出原因：执行 VMXON
	EXIT_REASON_CR_ACCESSES,                        // 退出原因：CR 寄存器访问
	EXIT_REASON_MOV_DR,                             // 退出原因：MOV DR
	EXIT_REASON_IO_INSTRUCTION,                     // 退出原因：I/O 指令
	EXIT_REASON_RDMSR,                              // 退出原因：RDMSR
	EXIT_REASON_WRMSR,                              // 退出原因：WRMSR
	EXIT_REASON_VM_ENTRY_FAILURE_INVALID_GUEST_STATE, // 退出原因：VM-entry 来宾状态无效
	EXIT_REASON_VM_ENTRY_FAILURE_MSR_LODAING,       // 退出原因：VM-entry MSR 加载失败
	EXIT_REASON_RESERVED1,                          // 保留值
	EXIT_REASON_MWAIT,                              // 退出原因：MWAIT
	EXIT_REASON_MONITOR_TRAP_FLAG,                  // 退出原因：MTF 触发
	EXIT_REASON_RESERVED2,                          // 保留值
	EXIT_REASON_MONITOR,                            // 退出原因：MONITOR
	EXIT_REASON_PAUSE,                              // 退出原因：PAUSE
	EXIT_REASON_VM_ENTRY_FAILURE_MACHINE_CHECK_EVENT, // 退出原因：VM-entry 机器检查失败
	EXIT_REASON_RESERVED3,                          // 保留值
	EXIT_REASON_TPR_BELOW_THRESHOLD,                // 退出原因：TPR 低于阈值
	EXIT_REASON_APIC_ACCESS,                        // 退出原因：APIC 访问
	EXIT_REASON_VIRTUALIZED_EOI,                    // 退出原因：虚拟化 EOI
	EXIT_REASON_ACCESS_TO_GDTR_OR_IDTR,             // 退出原因：访问 GDTR/IDTR
	EXIT_REASON_ACCESS_TO_LDTR_OR_TR,               // 退出原因：访问 LDTR/TR
	EXIT_REASON_EPT_VIOLATION,                      // 退出原因：EPT 违规
	EXIT_REASON_EPT_MISCONFIGURATION,               // 退出原因：EPT 配置错误
	EXIT_REASON_INVEPT,                             // 退出原因：INVEPT
	EXIT_REASON_RDTSCP,                             // 退出原因：RDTSCP
	EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED,       // 退出原因：VMX 预抢占计时器到期
	EXIT_REASON_INVVPID,                            // 退出原因：INVVPID
	EXIT_REASON_WBINVD,                             // 退出原因：WBINVD
	EXIT_REASON_XSETBV,                             // 退出原因：XSETBV
	EXIT_REASON_APIC_WRITE,                         // 退出原因：APIC 写
	EXIT_REASON_RDRAND,                             // 退出原因：RDRAND
	EXIT_REASON_INVPCID,                            // 退出原因：INVPCID
	EXIT_REASON_VMFUNC,                             // 退出原因：VMFUNC
	EXIT_REASON_ENCLS,                              // 退出原因：ENCLS
	EXIT_REASON_RDSEED,                             // 退出原因：RDSEED
	EXIT_REASON_PAGE_MODIFICATION_LOG_FULL,         // 退出原因：PML 满
	EXIT_REASON_XSAVES,                             // 退出原因：XSAVES
	EXIT_REASON_XRSTORS,                            // 退出原因：XRSTORS
	EXIT_REASON_RESERVED4,                          // 保留值
	EXIT_REASON_SPP_RELATED_EVENT,                  // 退出原因：SPP 相关事件
	EXIT_REASON_UMWAIT,                             // 退出原因：UMWAIT
	EXIT_REASON_TPAUSE,                             // 退出原因：TPAUSE
	EXIT_REASON_LAST                                // 退出原因：枚举结束标记
}; // 枚举结束

// 表 27-8：INS/OUTS 指令的 VM-exit 指令信息格式
union __vmexit_instruction_information1 // INS/OUTS 相关指令信息
{ // 联合体开始
	unsigned __int64 all; // 原始 64 位值
	struct // 位域结构
	{ // 定义块开始
		unsigned __int64 reserved1 : 7;     // 保留位
		unsigned __int64 address_size : 3; // 地址大小
		unsigned __int64 reserved2 : 5;    // 保留位
		unsigned __int64 segment_register : 3; // 段寄存器选择
		unsigned __int64 reserved3 : 14;   // 保留位
	}; // 位域结束
}; // 联合体结束

// 表 27-9：INVEPT/INVPCID/INVVPID 指令的 VM-exit 指令信息格式
union __vmexit_instruction_information2 // INVEPT/INVPCID/INVVPID 相关指令信息
{ // 联合体开始
	unsigned __int64 all; // 原始 64 位值
	struct // 位域结构
	{ // 定义块开始
		unsigned __int64 scaling : 2;          // 缩放因子
		unsigned __int64 reserved1 : 5;        // 保留位
		unsigned __int64 address_size : 3;     // 地址大小
		unsigned __int64 reserved2 : 1;        // 保留位
		unsigned __int64 reserved3 : 4;        // 保留位
		unsigned __int64 segment_register : 3; // 段寄存器
		unsigned __int64 index_reg : 4;        // 变址寄存器
		unsigned __int64 index_reg_invalid : 1;// 变址寄存器是否无效
		unsigned __int64 base_reg : 4;         // 基址寄存器
		unsigned __int64 base_reg_invalid : 1; // 基址寄存器是否无效
		unsigned __int64 reg2 : 4;             // 第二寄存器
	}; // 位域结束
}; // 联合体结束

// 表 27-10：LIDT/LGDT/SIDT/SGDT 指令的 VM-exit 指令信息格式
union __vmexit_instruction_information3 // LIDT/LGDT/SIDT/SGDT 相关指令信息
{ // 联合体开始
	unsigned __int64 all; // 原始 64 位值
	struct // 位域结构
	{ // 定义块开始
		unsigned __int64 scaling : 2;          // 缩放因子
		unsigned __int64 reserved1 : 5;        // 保留位
		unsigned __int64 address_size : 3;     // 地址大小
		unsigned __int64 reserved2 : 1;        // 保留位
		unsigned __int64 operand_size : 1;     // 操作数大小
		unsigned __int64 reserved3 : 3;        // 保留位
		unsigned __int64 segment_register : 3; // 段寄存器
		unsigned __int64 index_reg : 4;        // 变址寄存器
		unsigned __int64 index_reg_invalid : 1;// 变址寄存器是否无效
		unsigned __int64 base_reg : 4;         // 基址寄存器
		unsigned __int64 base_reg_invalid : 1; // 基址寄存器是否无效
		unsigned __int64 instruction_identity : 2; // 指令类型标识
		unsigned __int64 reserved4 : 2;        // 保留位
	}; // 位域结束
}; // 联合体结束

// 表 27-11：LLDT/LTR/SLDT/STR 指令的 VM-exit 指令信息格式
union __vmexit_instruction_information4 // LLDT/LTR/SLDT/STR 相关指令信息
{ // 联合体开始
	unsigned __int64 all; // 原始 64 位值
	struct // 位域结构
	{ // 定义块开始
		unsigned __int64 scaling : 2;          // 缩放因子
		unsigned __int64 reserved1 : 1;        // 保留位
		unsigned __int64 reg1 : 4;             // 寄存器 1
		unsigned __int64 address_size : 3;     // 地址大小
		unsigned __int64 mem_reg : 1;          // 内存/寄存器选择
		unsigned __int64 reserved2 : 4;        // 保留位
		unsigned __int64 segment_register : 3; // 段寄存器
		unsigned __int64 index_reg : 4;        // 变址寄存器
		unsigned __int64 index_reg_invalid : 1;// 变址寄存器是否无效
		unsigned __int64 base_reg : 4;         // 基址寄存器
		unsigned __int64 base_reg_invalid : 1; // 基址寄存器是否无效
		unsigned __int64 instruction_identity : 2; // 指令类型标识
		unsigned __int64 reserved4 : 2;        // 保留位
	}; // 位域结束
}; // 联合体结束

// 表 27-12：RDRAND/RDSEED/TPAUSE/UMWAIT 指令的 VM-exit 指令信息格式
union __vmexit_instruction_information5 // RDRAND/RDSEED/TPAUSE/UMWAIT 相关指令信息
{ // 联合体开始
	unsigned __int64 all; // 原始 64 位值
	struct // 位域结构
	{ // 定义块开始
		unsigned __int64 reserved1 : 3;        // 保留位
		unsigned __int64 operand_register : 4;// 操作数寄存器
		unsigned __int64 reserved2 : 4;        // 保留位
		unsigned __int64 operand_size : 2;     // 操作数大小
		unsigned __int64 reserved3 : 19;       // 保留位
	}; // 位域结束
}; // 联合体结束

// 表 27-13：VMCLEAR/VMPTRLD/VMPTRST/VMXON/XRSTORS/XSAVES 指令的 VM-exit 指令信息格式
union __vmexit_instruction_information6 // VMCLEAR/VMPTRLD/VMXON/XSAVES 相关指令信息
{ // 联合体开始
	unsigned __int64 all; // 原始 64 位值
	struct // 位域结构
	{ // 定义块开始
		unsigned __int64 scaling : 2;          // 缩放因子
		unsigned __int64 reserved1 : 5;        // 保留位
		unsigned __int64 address_size : 3;     // 地址大小
		unsigned __int64 reserved2 : 1;        // 保留位
		unsigned __int64 reserved3 : 4;        // 保留位
		unsigned __int64 segment_register : 3; // 段寄存器
		unsigned __int64 index_reg : 4;        // 变址寄存器
		unsigned __int64 index_reg_invalid : 1;// 变址寄存器是否无效
		unsigned __int64 base_reg : 4;         // 基址寄存器
		unsigned __int64 base_reg_invalid : 1; // 基址寄存器是否无效
		unsigned __int64 instruction_identity : 2; // 指令类型标识
		unsigned __int64 reserved4 : 2;        // 保留位
	}; // 位域结束
}; // 联合体结束

// 表 27-14：VMREAD/VMWRITE 指令的 VM-exit 指令信息格式
union __vmexit_instruction_information7 // VMREAD/VMWRITE 相关指令信息
{ // 联合体开始
	unsigned __int64 all; // 原始 64 位值
	struct // 位域结构
	{ // 定义块开始
		unsigned __int64 scaling : 2;          // 缩放因子
		unsigned __int64 reserved1 : 1;        // 保留位
		unsigned __int64 reg1 : 4;             // 寄存器 1
		unsigned __int64 address_size : 3;     // 地址大小
		unsigned __int64 mem_reg : 1;          // 内存/寄存器选择
		unsigned __int64 reserved2 : 4;        // 保留位
		unsigned __int64 segment_register : 3; // 段寄存器
		unsigned __int64 index_reg : 4;        // 变址寄存器
		unsigned __int64 index_reg_invalid : 1;// 变址寄存器是否无效
		unsigned __int64 base_reg : 4;         // 基址寄存器
		unsigned __int64 base_reg_invalid : 1; // 基址寄存器是否无效
		unsigned __int64 reg2 : 4;             // 寄存器 2
	}; // 位域结束
}; // 联合体结束

// 表 27-5：I/O 指令的 Exit Qualification 格式
union __exit_qualification_io // I/O 指令退出资格信息
{ // 联合体开始
	unsigned __int64 all; // 原始 64 位值
	struct // 位域结构
	{ // 定义块开始
		unsigned __int64 access_size : 3;      // 访问宽度：1/2/4 字节
		unsigned __int64 direction : 1;        // 方向：0=OUT，1=IN
		unsigned __int64 string_instruction : 1; // 是否字符串指令
		unsigned __int64 rep : 1;              // 是否带 REP 前缀
		unsigned __int64 operand_encoding : 1; // 操作数来源：0=DX，1=立即数
		unsigned __int64 reserved1 : 9;        // 保留位
		unsigned __int64 port_number : 16;     // I/O 端口号
		unsigned __int64 reserved2 : 32;       // 保留位
	}; // 位域结束
}; // 联合体结束

// MOV DR 相关的 Exit Qualification 格式
union __exit_qualification_dr // MOV DR 指令退出资格信息
{ // 联合体开始
	unsigned __int64 all; // 原始 64 位值
	struct // 位域结构
	{ // 定义块开始
		unsigned __int64 debug_register_number : 3; // DR 寄存器编号
		unsigned __int64 reserved1 : 1;             // 保留位
		unsigned __int64 access_direction : 1;      // 方向：0=写 DR，1=读 DR
		unsigned __int64 reserved2 : 3;             // 保留位
		unsigned __int64 gp_register : 4;           // 通用寄存器编号
		unsigned __int64 reserved3 : 52;            // 保留位
	}; // 位域结束
}; // 联合体结束

EXTERN_C // 声明外部 C 接口，避免 C++ 名字改编
unsigned __int64 return_rsp_for_vmxoff(); // 返回 VMXOFF 时的 RSP

EXTERN_C // 声明外部 C 接口，避免 C++ 名字改编
unsigned __int64 return_rip_for_vmxoff(); // 返回 VMXOFF 时的 RIP

EXTERN_C // 声明外部 C 接口，避免 C++ 名字改编
bool vmexit_handler(guest_context* guest_registers, PFXSAVE64 fxsave); // VM-exit 入口函数

// 调整 RIP 指向下一条指令
void adjust_rip(__vcpu* vcpu); // 更新来宾 RIP，跳过已执行指令

void skip_instruction(); // 直接跳过当前指令

void dispatch_vm_exit(__vcpu* vcpu); // 根据退出原因分发处理逻辑

// 恢复来宾状态
void RestoreGuest(); // 将来宾寄存器状态恢复到继续执行

#endif // !_VMEXIT_HANDLER_H        // 结束包含保护

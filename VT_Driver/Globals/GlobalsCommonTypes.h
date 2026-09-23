#pragma once

#include "GlobalsConstants.h"
#include "GlobalsMacros.h"
#include "../../Common/Ring0/Lifecycle/EptWatchTransactionPolicy.h"

// 前向声明
namespace pool_manager
{
    struct __pool_manager;
}

#pragma pack(push, 1) // 压缩结构体对齐为 1 字节，确保与硬件布局一致
struct __pseudo_descriptor64 // 64 位伪描述符（用于 GDTR/IDTR 加载）
{
    unsigned __int16 limit; // 描述符表限制（长度 - 1）
    unsigned __int64 base_address; // 描述符表基址
};
#pragma pack(pop) // 恢复默认对齐方式

#pragma pack(push, 1) // 压缩结构体对齐为 1 字节，确保与硬件布局一致
struct __pseudo_descriptor32 // 32 位伪描述符（用于兼容场景）
{
    unsigned __int16 limit; // 描述符表限制（长度 - 1）
    unsigned __int32 base_address; // 描述符表基址
};
#pragma pack(pop) // 恢复默认对齐方式

enum EPT_WATCH_ENTRY_STATE : int
{
    EPT_WATCH_FREE = 0,
    EPT_WATCH_INSTALLING = 1,
    EPT_WATCH_ACTIVE = 2,
    EPT_WATCH_REMOVING = 3
};

typedef struct // EPT 监视断点条目（描述一个监视点）
{
    unsigned __int64 cr3; // 目标进程的 cr3
    unsigned __int64 VirtualAddress; // 监视的虚拟地址
    unsigned __int64 PhysicalAddress;    // 虚拟地址对应的物理地址
    unsigned __int64 Type; // 读、写、执行
    unsigned long Options; // 断点选项（虚拟断点/其他扩展）
    unsigned __int64 LoopUserMode; // 用户态回环地址（断点触发后跳转）
    unsigned __int64 LoopKernelMode; // 内核态回环地址（断点触发后跳转）
    int Size; // 断点覆盖的字节数
    int inuse; // EPT_WATCH_ENTRY_STATE：区分预留、生效和删除事务
    int bpType; // 断点类型 3=int3  1=debugreg
    int debugRegister; // 命中的 DR0-DR3 槽位
    int GroupId; // 多页 watch 的根句柄；删除任一成员时撤销整组
    int Cr3TrackerId; // CR3 shuffle 跟踪器槽位，watch 生命周期持有一份引用
    unsigned char OriginalByte; // 原始字节（int3 断点恢复用）
} EPTWatchEntry, * PEPTWatchEntry; // 定义监视条目与指针类型

inline bool ept_watch_entry_is_reserved(EPTWatchEntry const& entry)
{
    return ept::watch_transaction_policy::IsReserved(
        static_cast<ept::watch_transaction_policy::state>(entry.inuse));
}

inline bool ept_watch_entry_is_active(EPTWatchEntry const& entry)
{
    return ept::watch_transaction_policy::IsPublished(
        static_cast<ept::watch_transaction_policy::state>(entry.inuse));
}

inline bool ept_watch_entry_is_effective(EPTWatchEntry const& entry)
{
    return ept::watch_transaction_policy::ContributesPermissions(
        static_cast<ept::watch_transaction_policy::state>(entry.inuse));
}

inline bool ept_watch_entry_owns_hardware(EPTWatchEntry const& entry)
{
    return ept::watch_transaction_policy::OwnsHardware(
        static_cast<ept::watch_transaction_policy::state>(entry.inuse));
}

typedef volatile struct _criticalSection // 自定义自旋锁式临界区结构
{
    volatile int locked; // 锁状态（0 未锁定，1 锁定）
    volatile int apicid; // 当前持锁 CPU 的 APIC ID
    volatile int lockcount; // 重入/调试计数
    char* name; // 锁名称（用于调试输出）
    int debuglevel; // 调试级别
#ifdef DEBUG // 仅在 DEBUG 编译下有效
    int ignorelock; // 调试辅助：忽略锁检查
#endif
} criticalSection, * PcriticalSection; // 临界区与指针类型

union __rflags // RFLAGS 寄存器位字段封装
{
    unsigned __int64 all; // 整体 64 位值
    struct
    {
        unsigned __int64 carry_flag : 1;  // bit0 进位标志
        unsigned __int64 read_as_1 : 1;  // bit1 固定为 1
        unsigned __int64 parity_flag : 1; // bit2 奇偶标志
        unsigned __int64 reserved_1 : 1;  // bit3 保留
        unsigned __int64 auxiliary_carry_flag : 1; // bit4 辅助进位
        unsigned __int64 reserved_2 : 1;  // bit5 保留
        unsigned __int64 zero_flag : 1;  // bit6 零标志
        unsigned __int64 sign_flag : 1;  // bit7 符号标志
        unsigned __int64 trap_flag : 1;  // bit8 单步标志
        unsigned __int64 interrupt_enable_flag : 1;  // bit9 中断使能
        unsigned __int64 direction_flag : 1; // bit10 字符串方向
        unsigned __int64 overflow_flag : 1; // bit11 溢出标志
        unsigned __int64 io_privilege_level : 2; // bit13:12 I/O 特权级
        unsigned __int64 nested_task_flag : 1; // bit14 嵌套任务
        unsigned __int64 reserved_3 : 1; // bit15 保留
        unsigned __int64 resume_flag : 1; // bit16 恢复标志
        unsigned __int64 virtual_8086_mode_flag : 1; // bit17 v8086 模式
        unsigned __int64 alignment_check_flag : 1; // bit18 对齐检查
        unsigned __int64 virtual_interrupt_flag : 1; // bit19 虚拟中断
        unsigned __int64 virtual_interrupt_pending_flag : 1; // bit20 虚拟中断挂起
        unsigned __int64 identification_flag : 1; // bit21 ID 标志
    };
};

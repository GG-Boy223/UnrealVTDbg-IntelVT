#pragma once

#ifndef _DRX_H
#define _DRX_H

union __dr6
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 breakpoint_condition : 4;
        unsigned __int64 reserved_1 : 8; // always 1
        unsigned __int64 reserved_2 : 1; // always 0
        unsigned __int64 debug_register_access_detected : 1;
        unsigned __int64 single_instruction : 1;
        unsigned __int64 task_switch : 1;
        unsigned __int64 restricted_transactional_memory : 1;
        unsigned __int64 reserved_3 : 15; // always 1
    };
};

union __dr7
{
    unsigned __int64 all;
    struct
    {
        unsigned __int64 local_breakpoint_0 : 1;
        unsigned __int64 global_breakpoint_0 : 1;
        unsigned __int64 local_breakpoint_1 : 1;
        unsigned __int64 global_breakpoint_1 : 1;
        unsigned __int64 local_breakpoint_2 : 1;
        unsigned __int64 global_breakpoint_2 : 1;
        unsigned __int64 local_breakpoint_3 : 1;
        unsigned __int64 global_breakpoint_3 : 1;
        unsigned __int64 local_exact_breakpoint : 1;
        unsigned __int64 global_exact_breakpoint : 1;
        unsigned __int64 reserved_1 : 1; // always 1
        unsigned __int64 restricted_transactional_memory : 1;
        unsigned __int64 reserved_2 : 1; // always 0
        unsigned __int64 general_detect : 1;
        unsigned __int64 reserved_3 : 2; // always 0
        unsigned __int64 read_write_0 : 2;
        unsigned __int64 length_0 : 2;
        unsigned __int64 read_write_1 : 2;
        unsigned __int64 length_1 : 2;
        unsigned __int64 read_write_2 : 2;
        unsigned __int64 length_2 : 2;
        unsigned __int64 read_write_3 : 2;
        unsigned __int64 length_3 : 2;
    };
};

// ---------------------------------------------------------------------------
// DR7 位掩码（Intel SDM Vol3B 17.2.4 表 17-2）
//   bit 0-7   : L0/G0/L1/G1/L2/G2/L3/G3 —— 局部/全局断点使能（guest 可写）
//   bit 8-9   : LE/GE —— 精确断点（guest 可写）
//   bit 10    : 保留，必须为 1
//   bit 11    : RTM —— 事务性内存断点使能（guest 可写）
//   bit 12    : 保留，必须为 0
//   bit 13    : GD —— general detect（guest 可写，但本驱动不落物理寄存器，见下）
//   bit 14-15 : 保留，必须为 0
//   bit 16-31 : RWn / LENn（guest 可写）
//   bit 32-63 : 保留，必须为 0（"load debug controls" = 1 时）
// ---------------------------------------------------------------------------
constexpr unsigned __int64 kDr7FixedOne = 1ull << 10;
constexpr unsigned __int64 kDr7GdBit = 1ull << 13;
constexpr unsigned __int64 kDr7WritableMask =
    0x3FFull | (1ull << 11) | kDr7GdBit | 0xFFFF0000ull;

// 归一化为"可以安全载入物理 DR7"的值：保留 guest 可写位与 bit10，剔除 GD 与保留位。
//
// 为什么必须剔除 GD：
//   VM-exit 没有 host DR7 字段（Intel SDM Vol3C 27.5.1），root 运行期的物理 DR7 就是
//   guest 的 DR7。一旦 GD=1，root 里任何 MOV DR 都会先产生 #DB（SDM Vol3B 17.2.1），
//   而 ASM/vm-exit.asm 序言与收尾会无条件执行 mov drN —— #DB 落到 root IDT 的三重故障
//   路径即宿主蓝屏。因此 GUEST_DR7 必须永不携带 GD；GD 的 guest 可见语义由
//   __vcpu::guest_dr7_shadow 与 VmExit/VmExitDebugRegisters.cpp 单独仿真。
constexpr unsigned __int64 sanitize_guest_dr7(unsigned __int64 value)
{
    return (value & kDr7WritableMask & ~kDr7GdBit) | kDr7FixedOne;
}

// guest 可见的 DR7 影子：保留 GD，使 guest 的 MOV DR7 读回语义与真实硬件一致。
constexpr unsigned __int64 normalize_guest_dr7_shadow(unsigned __int64 value)
{
    return (value & kDr7WritableMask) | kDr7FixedOne;
}

#endif // !_DRX_H

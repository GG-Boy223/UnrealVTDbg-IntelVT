#pragma once

#include "GlobalsGuestTypes.h"

struct __vmcs // VMCS 内存布局（4KB 对齐）
{
    union
    {
        unsigned int all; // VMCS 头部整体 32 位
        struct
        {
            unsigned int revision_identifier : 31; // VMCS 版本号
            unsigned int shadow_vmcs_indicator : 1; // 是否为 shadow VMCS
        };
    } header; // VMCS 头部字段
    unsigned int abort_indicator; // VMCS 中止原因字段
    char data[0x1000 - 2 * sizeof(unsigned)]; // VMCS 正文（剩余空间）
};

struct vmx_msr_entry // VMX MSR 存储/加载区条目
{
    uint32_t msr_idx; // MSR 索引
    uint32_t _reserved; // 保留
    uint64_t msr_data; // MSR 数据
};

struct __mtrr_range_descriptor // MTRR 区间描述
{
    unsigned __int64 physcial_base_address; // 物理基址
    unsigned __int64 physcial_end_address; // 物理结束地址
    unsigned __int8 memory_type; // 内存类型（UC/WB/WT 等）
    bool fixed_range; // 是否为固定范围 MTRR
};

union __mtrr_physmask_reg // MTRR 物理掩码寄存器
{
    unsigned __int64 all; // 전체 값
    struct
    {
        unsigned __int64 reserved : 11; // 保留
        unsigned __int64 valid : 1; // 有效位
        unsigned __int64 physmask : 36; // 物理掩码字段
        unsigned __int64 reserved2 : 16; // 保留
    };
};

union __mtrr_physbase_reg // MTRR 物理基址寄存器
{
    unsigned __int64 all; // 전체 值
    struct
    {
        unsigned __int64 type : 8; // 内存类型
        unsigned __int64 reserved : 4; // 保留
        unsigned __int64 physbase : 36; // 物理基址
        unsigned __int64 reserved2 : 16; // 保留
    };
};

union __mtrr_cap_reg // MTRR 能力寄存器
{
    unsigned __int64 all; // 전체 值
    struct
    {
        unsigned __int64 range_register_number : 8; //bit 7:0  处理器中可变MTRRs寄存器的数量。
        unsigned __int64 fixed_range_support : 1; //bit 8
        unsigned __int64 reserved : 1; //bit 9
        unsigned __int64 write_combining_support : 1; //bit 10
        unsigned __int64 smrr_support : 1; //bit 11
        unsigned __int64 reserved2 : 52; //bit 63:12
    };
};

union __mtrr_def_type // MTRR 默认类型寄存器
{
    unsigned __int64 all; // 전체 值
    struct
    {
        unsigned __int64 memory_type : 3;  //bit 2:0
        unsigned __int64 reserved1 : 7;  //bit 9:3
        unsigned __int64 fixed_range_mtrr_enabled : 1;  //bit 10
        unsigned __int64 mtrr_enabled : 1;  //bit 11
        unsigned __int64 reserved2 : 52;  //bit 63:12
    };
};

union __mtrr_fixed_range_type // 固定范围 MTRR 类型数组
{
    unsigned __int64 all; // 전체 值
    struct
    {
        unsigned __int8 types[8]; // 8 个子范围的类型
    };
};

struct mtrr_data // MTRR 汇总数据结构
{
    ia32_mtrr_capabilities_register cap; // MTRR 能力寄存器
    ia32_mtrr_def_type_register def_type; // MTRR 默认类型

    // fixed-range MTRRs
    // 固定范围MTRRs
    struct
    {
        // TODO: implement
    } fixed; // 固定范围占位

    // variable-range MTRRs
    // 可变范围MTRRs寄存器组  处理器一般实现8个
    struct
    {
        ia32_mtrr_physbase_register base; // 可变 MTRR 基址
        ia32_mtrr_physmask_register mask; // 可变 MTRR 掩码
    } variable[64]; // 可变范围数组

    // number of valid variable-range MTRRs
    // 有效的可变范围 MTRRs 的数量
    size_t var_count; // 实际有效条目数
};

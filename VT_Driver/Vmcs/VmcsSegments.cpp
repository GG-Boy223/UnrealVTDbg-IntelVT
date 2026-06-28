#include "VmcsInternal.h"

// 根据 GDTR 与选择子计算段基址（64 位描述符版本）
uint64_t segment_base(__pseudo_descriptor64 const& gdtr, unsigned __int16 arg_selector) // 计算段基址
{
    segment_selector selector; // 段选择子结构
    selector.flags = arg_selector; // 解析选择子

    // 如果是空选择子，段基址为 0
    if (selector.index == 0) // 判断是否空选择子
    {
        return 0;
    }

    // 从 GDT 中取出描述符
    auto const descriptor = reinterpret_cast<segment_descriptor_64*>(gdtr.base_address + static_cast<uint64_t>(selector.index) * 8); // 8 字节一个描述符
    // 计算 32 位段基址
    auto base_address = // 计算段基址的临时变量
        (uint64_t)descriptor->base_address_low | // 低 16 位
        ((uint64_t)descriptor->base_address_middle << 16) | // 中间 8 位
        ((uint64_t)descriptor->base_address_high << 24); // 高 8 位

    // 系统段在 64 位模式下扩展为 16 字节描述符
    if (descriptor->descriptor_type == SEGMENT_DESCRIPTOR_TYPE_SYSTEM) // 判断是否系统段
    {
        base_address |= (uint64_t)descriptor->base_address_upper << 32; // 拼接高 32 位
    }

    return base_address; // 返回段基址
}

vmx_segment_access_rights segment_access(unsigned __int8* gdt_base, unsigned __int16 selector)
{
    // 从 GDT 中取出描述符
    auto const descriptor = reinterpret_cast<segment_descriptor_64*>(gdt_base + (selector & ~0x7)); // 根据选择子定位描述符

    segment_selector selector2; // 选择子结构
    selector2.flags = selector; // 解析选择子字段

    vmx_segment_access_rights access; // 访问权限结构
    access.flags = 0; // 先清零
    // 按 Intel 手册规则填写访问权限各字段
    access.type = descriptor->type; // 段类型
    access.descriptor_type = descriptor->descriptor_type; // 系统段或代码/数据段
    access.descriptor_privilege_level = descriptor->descriptor_privilege_level; // DPL
    access.present = descriptor->present; // 是否存在
    access.available_bit = descriptor->system; // 可用位
    access.long_mode = descriptor->long_mode; // 64 位代码段标志
    access.default_big = descriptor->default_big; // 默认操作数大小
    access.granularity = descriptor->granularity; // 粒度（字节/页）
    access.unusable = (selector2.index == 0); // 空选择子视为不可用

    return access; // 返回访问权限
}

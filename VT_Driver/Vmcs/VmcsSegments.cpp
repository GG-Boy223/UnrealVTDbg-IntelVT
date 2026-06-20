#include "VmcsInternal.h"

uint64_t segment_base(__pseudo_descriptor64 const& gdtr, unsigned __int16 arg_selector)
{
    segment_selector selector;
    selector.flags = arg_selector;

    if (selector.index == 0)
    {
        return 0;
    }

    auto const descriptor = reinterpret_cast<segment_descriptor_64*>(gdtr.base_address + static_cast<uint64_t>(selector.index) * 8);
    auto base_address =
        (uint64_t)descriptor->base_address_low |
        ((uint64_t)descriptor->base_address_middle << 16) |
        ((uint64_t)descriptor->base_address_high << 24);

    if (descriptor->descriptor_type == SEGMENT_DESCRIPTOR_TYPE_SYSTEM)
    {
        base_address |= (uint64_t)descriptor->base_address_upper << 32;
    }

    return base_address;
}

vmx_segment_access_rights segment_access(unsigned __int8* gdt_base, unsigned __int16 selector)
{
    auto const descriptor = reinterpret_cast<segment_descriptor_64*>(gdt_base + (selector & ~0x7));

    segment_selector selector2;
    selector2.flags = selector;

    vmx_segment_access_rights access;
    access.flags = 0;
    access.type = descriptor->type;
    access.descriptor_type = descriptor->descriptor_type;
    access.descriptor_privilege_level = descriptor->descriptor_privilege_level;
    access.present = descriptor->present;
    access.available_bit = descriptor->system;
    access.long_mode = descriptor->long_mode;
    access.default_big = descriptor->default_big;
    access.granularity = descriptor->granularity;
    access.unusable = (selector2.index == 0);

    return access;
}

#pragma once

namespace mtrr::type_policy
{
    constexpr unsigned __int8 uncacheable = 0;
    constexpr unsigned __int8 write_combining = 1;
    constexpr unsigned __int8 write_through = 4;
    constexpr unsigned __int8 write_protected = 5;
    constexpr unsigned __int8 write_back = 6;
    constexpr unsigned __int8 invalid = 0xff;

    constexpr unsigned __int8 merge(unsigned __int8 current, unsigned __int8 next)
    {
        if (current == invalid)
        {
            return next;
        }
        if (current == next)
        {
            return current;
        }
        if (current == uncacheable || next == uncacheable)
        {
            return uncacheable;
        }
        if ((current == write_through && next == write_back) ||
            (current == write_back && next == write_through))
        {
            return write_through;
        }

        // Intel defines all other variable-MTRR overlap combinations as undefined.
        // A hypervisor must not turn an undefined combination into a cacheable EPT mapping.
        return uncacheable;
    }
}

#pragma once

namespace hv::capability_policy
{
    constexpr unsigned __int32 required_primary_allowed1 = 1u << 31;
    constexpr unsigned __int32 required_secondary_allowed1 = (1u << 1) | (1u << 5);

    constexpr unsigned __int64 required_ept_vpid_capabilities =
        (1ull << 0) |  // execute-only EPT entries
        (1ull << 6) |  // four-level EPT walk
        (1ull << 14) | // WB EPTP
        (1ull << 16) | // 2MB EPT pages
        (1ull << 20) | // INVEPT instruction
        (1ull << 25) | // single-context INVEPT
        (1ull << 26) | // all-context INVEPT
        (1ull << 32) | // INVVPID instruction
        (1ull << 40) | // individual-address INVVPID
        (1ull << 41) | // single-context INVVPID
        (1ull << 42) | // all-context INVVPID
        (1ull << 43);  // single-context retaining globals INVVPID

    constexpr unsigned __int32 missing_primary_controls(unsigned __int32 allowed1)
    {
        return required_primary_allowed1 & ~allowed1;
    }

    constexpr unsigned __int32 missing_secondary_controls(unsigned __int32 allowed1)
    {
        return required_secondary_allowed1 & ~allowed1;
    }

    constexpr unsigned __int64 missing_ept_vpid_capabilities(unsigned __int64 capabilities)
    {
        return required_ept_vpid_capabilities & ~capabilities;
    }

    constexpr bool is_supported(
        unsigned __int32 primary_allowed1,
        unsigned __int32 secondary_allowed1,
        unsigned __int64 ept_vpid_capabilities)
    {
        return missing_primary_controls(primary_allowed1) == 0 &&
            missing_secondary_controls(secondary_allowed1) == 0 &&
            missing_ept_vpid_capabilities(ept_vpid_capabilities) == 0;
    }
}

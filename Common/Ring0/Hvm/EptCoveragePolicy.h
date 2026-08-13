#pragma once

namespace ept::coverage_policy
{
    constexpr unsigned __int64 bytes_per_pdpte = 0x40000000ull;
    constexpr unsigned __int32 minimum_pdpte_count = 512u;
    constexpr unsigned __int32 maximum_pdpte_count = 512u * 512u;

    enum class status
    {
        ok,
        missing_physical_ranges,
        arithmetic_overflow,
        exceeds_four_level_limit,
    };

    struct result
    {
        unsigned __int32 pdpte_count;
        status calculation_status;
    };

    constexpr result calculate(unsigned __int64 highest_physical_end)
    {
        if (highest_physical_end == 0)
        {
            return { 0, status::missing_physical_ranges };
        }

        constexpr unsigned __int64 maximum_u64 = ~0ull;
        if (highest_physical_end > maximum_u64 - (bytes_per_pdpte - 1))
        {
            return { 0, status::arithmetic_overflow };
        }

        unsigned __int64 required =
            (highest_physical_end + bytes_per_pdpte - 1) / bytes_per_pdpte;
        if (required < minimum_pdpte_count)
        {
            required = minimum_pdpte_count;
        }
        if (required > maximum_pdpte_count)
        {
            return { 0, status::exceeds_four_level_limit };
        }

        return { static_cast<unsigned __int32>(required), status::ok };
    }
}

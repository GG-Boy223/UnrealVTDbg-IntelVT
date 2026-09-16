#pragma once

namespace ept::coverage_policy
{
    constexpr unsigned __int64 bytes_per_pdpte = 0x40000000ull;
    constexpr unsigned __int32 minimum_pdpte_count = 512u;
    constexpr unsigned __int32 maximum_pdpte_count = 512u * 512u;

    // A 4-level EPT hierarchy (PML4E -> PDPTE -> PDE -> PTE) expresses at most 512 * 512
    // 1GB regions, i.e. a guest-physical address limit of 2^48 (256TB). That limit comes
    // from the paging structures themselves and is independent of the CPU's MAXPHYADDR:
    // MKTME/TDX capable parts (and other SKUs with wide physical addressing) report
    // MAXPHYADDR=52, where 1<<MAXPHYADDR cannot be expressed by a 4-level EPT.
    // Clamp to the limit instead of refusing to build the EPT.
    constexpr unsigned __int64 four_level_limit_bytes = bytes_per_pdpte * maximum_pdpte_count;

    enum class status
    {
        ok,
        missing_physical_ranges,
        arithmetic_overflow,
        clamped_to_four_level_limit,
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
            return { maximum_pdpte_count, status::clamped_to_four_level_limit };
        }

        return { static_cast<unsigned __int32>(required), status::ok };
    }
}

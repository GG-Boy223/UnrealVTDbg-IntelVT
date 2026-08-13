#include "pch.h"

#include <Ring0/Hvm/EptCoveragePolicy.h>
#include <Ring0/Hvm/MtrrTypePolicy.h>
#include <Ring0/Hvm/VmxCapabilityPolicy.h>

namespace
{
    int failures = 0;

    void expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", message);
            ++failures;
        }
    }
}

int main()
{
    using namespace hv::capability_policy;

    expect(is_supported(
        required_primary_allowed1,
        required_secondary_allowed1,
        required_ept_vpid_capabilities),
        "complete capability set must be accepted");

    for (unsigned int bit = 0; bit < 32; ++bit)
    {
        const unsigned __int32 mask = 1u << bit;
        if ((required_primary_allowed1 & mask) != 0)
        {
            expect(!is_supported(
                required_primary_allowed1 & ~mask,
                required_secondary_allowed1,
                required_ept_vpid_capabilities),
                "each required primary control must be enforced");
        }

        if ((required_secondary_allowed1 & mask) != 0)
        {
            expect(!is_supported(
                required_primary_allowed1,
                required_secondary_allowed1 & ~mask,
                required_ept_vpid_capabilities),
                "each required secondary control must be enforced");
        }
    }

    for (unsigned int bit = 0; bit < 64; ++bit)
    {
        const unsigned __int64 mask = 1ull << bit;
        if ((required_ept_vpid_capabilities & mask) != 0)
        {
            expect(!is_supported(
                required_primary_allowed1,
                required_secondary_allowed1,
                required_ept_vpid_capabilities & ~mask),
                "each required EPT/VPID capability must be enforced");
        }
    }

    const unsigned __int64 unrelated_capability = 1ull << 17;
    expect(is_supported(
        required_primary_allowed1,
        required_secondary_allowed1,
        required_ept_vpid_capabilities | unrelated_capability),
        "unrelated capabilities must not change the decision");

    using ept::coverage_policy::calculate;
    using ept::coverage_policy::status;

    expect(calculate(0).calculation_status == status::missing_physical_ranges,
        "missing physical ranges must fail closed");
    expect(calculate(1).pdpte_count == ept::coverage_policy::minimum_pdpte_count,
        "small systems must retain the 512GB compatibility floor");
    expect(calculate(512ull * ept::coverage_policy::bytes_per_pdpte).pdpte_count == 512u,
        "the 512GB boundary must use exactly 512 PDPTEs");
    expect(calculate(512ull * ept::coverage_policy::bytes_per_pdpte + 1).pdpte_count == 513u,
        "coverage must round an exclusive physical end upward");
    expect(calculate(1ull << 48).pdpte_count == ept::coverage_policy::maximum_pdpte_count,
        "four-level EPT must cover the complete 48-bit GPA range");
    expect(calculate((1ull << 48) + 1).calculation_status == status::exceeds_four_level_limit,
        "coverage beyond the four-level EPT GPA range must fail");
    expect(calculate(~0ull).calculation_status == status::arithmetic_overflow,
        "coverage rounding overflow must fail");

    using namespace mtrr::type_policy;
    expect(merge(invalid, write_back) == write_back,
        "the first matching MTRR type must initialize the result");
    expect(merge(write_back, write_back) == write_back,
        "identical overlapping MTRRs must preserve their type");
    expect(merge(write_back, write_through) == write_through,
        "WT must override WB regardless of ordering");
    expect(merge(write_through, write_back) == write_through,
        "WT/WB precedence must be symmetric");
    expect(merge(write_back, uncacheable) == uncacheable,
        "UC must override every cacheable type");
    expect(merge(write_combining, write_back) == uncacheable,
        "undefined MTRR overlap combinations must fail closed to UC");
    expect(merge(write_protected, write_through) == uncacheable,
        "unsupported cacheable overlap combinations must fail closed to UC");

    if (failures != 0)
    {
        std::fprintf(stderr, "%d capability policy test(s) failed\n", failures);
        return 1;
    }

    std::puts("EPT capability policy tests passed");
    return 0;
}

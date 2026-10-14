#pragma once

namespace ept::coverage_policy
{
    constexpr unsigned __int64 bytes_per_pdpte = 0x40000000ull;
    constexpr unsigned __int32 maximum_pdpte_count = 512u * 512u;

    // A 4-level EPT hierarchy (PML4E -> PDPTE -> PDE -> PTE) expresses at most 512 * 512
    // 1GB regions, i.e. a guest-physical address limit of 2^48 (256TB). That limit comes
    // from the paging structures themselves and is independent of the CPU's MAXPHYADDR:
    // MKTME/TDX capable parts (and other SKUs with wide physical addressing) report
    // MAXPHYADDR=52, where 1<<MAXPHYADDR cannot be expressed by a 4-level EPT.
    //
    // 覆盖策略对齐 HyperDbg EptAllocateAndCreateIdentityPageTable：构建期一次性把整个
    // 4 级 EPT 可表达的空间（512 * 512 个 1GB 区域 = 2^48 = 256TB）都做 identity 映射，
    // 而不是按 MAXPHYADDR 裁剪：
    //   - 0 ~ 512GB：每个 1GB 区域一张 PD 页，填满 512 个 2MB 条目
    //                （等价 HyperDbg 的内嵌 PML2[512][512]）。
    //   - 512GB ~ 256TB：每个 1GB 区域一个 1GB UC 大页条目
    //                （等价 HyperDbg 的 PML3_RSVD）。
    // 这样来宾访问任意高位 MMIO/BAR 都有现成映射，不再依赖运行期补映射。
    constexpr unsigned __int64 identity_map_bytes = bytes_per_pdpte * maximum_pdpte_count;

    enum class status
    {
        ok,
        missing_physical_ranges,
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

        // RAM 上界仅用于确认物理内存范围可用；覆盖范围恒定取整个 4 级空间。
        return { maximum_pdpte_count, status::ok };
    }
}

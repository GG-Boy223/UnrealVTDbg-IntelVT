#pragma once

#include <cstdint>

namespace hv::vmm_release_policy
{
    enum class lifecycle_state : std::int32_t
    {
        empty = 0,
        allocated = 1,
        running = 2,
        stopping = 3,
        stopped = 4,
    };

    constexpr bool IsVcpuQuiescent(
        std::uint64_t vmxOn,
        std::uint64_t vmmLaunched,
        std::uint64_t vmxoffRequested)
    {
        return vmxOn == 0 &&
            vmmLaunched == 0 &&
            vmxoffRequested == 0;
    }

    constexpr bool CanRelease(
        lifecycle_state state,
        bool allVcpusQuiescent)
    {
        return allVcpusQuiescent &&
            state != lifecycle_state::running &&
            state != lifecycle_state::stopping;
    }
}
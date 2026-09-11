#pragma once

#include <cstdint>

namespace hv::cr3_exit_policy
{
    constexpr std::uint32_t kCr3LoadExiting = 1u << 15;
    constexpr std::uint32_t kCr3StoreExiting = 1u << 16;
    constexpr std::uint32_t kTargetCapacity = 4;

    constexpr std::uint32_t Apply(
        std::uint32_t primaryControls,
        bool observationRequired)
    {
        const std::uint32_t withoutCr3Exits = primaryControls &
            ~(kCr3LoadExiting | kCr3StoreExiting);
        return observationRequired ?
            withoutCr3Exits | kCr3LoadExiting :
            withoutCr3Exits;
    }

    constexpr std::uint32_t SelectTargetSlot(
        std::uint32_t targetCount,
        std::uint32_t cursor)
    {
        return targetCount < kTargetCapacity ?
            targetCount : cursor % kTargetCapacity;
    }

    constexpr std::uint32_t AdvanceTargetCursor(
        std::uint32_t targetCount,
        std::uint32_t selectedSlot)
    {
        return targetCount < kTargetCapacity ?
            targetCount + 1 :
            (selectedSlot + 1) % kTargetCapacity;
    }
}
#pragma once

#include <cstdint>

namespace dbgk::rundown_policy
{
    enum class restore_action : std::uint32_t
    {
        complete_without_write = 0,
        write_original = 1,
        fail_stop = 2,
    };

    constexpr restore_action SelectRestoreAction(
        bool callbackOwned,
        bool objectTypeAvailable,
        bool callbackReadable,
        std::uintptr_t currentCallback,
        std::uintptr_t installedCallback)
    {
        if (!callbackOwned)
        {
            return restore_action::complete_without_write;
        }

        if (!objectTypeAvailable || !callbackReadable)
        {
            return restore_action::fail_stop;
        }

        if (currentCallback == installedCallback)
        {
            return restore_action::write_original;
        }

        return restore_action::complete_without_write;
    }

    constexpr bool CanReleaseOwnedState(
        bool callbackOwned,
        bool restorationConfirmed)
    {
        return !callbackOwned || restorationConfirmed;
    }
}

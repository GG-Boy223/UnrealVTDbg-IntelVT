#pragma once

#include <cstdint>

namespace ept::watch_transaction_policy
{
    enum class state : std::int32_t
    {
        free = 0,
        installing = 1,
        active = 2,
        removing = 3,
    };

    constexpr bool IsReserved(state value)
    {
        return value != state::free;
    }

    constexpr bool IsPublished(state value)
    {
        return value == state::active;
    }

    constexpr bool OwnsHardware(state value)
    {
        return value == state::installing ||
            value == state::active ||
            value == state::removing;
    }

    constexpr bool ContributesPermissions(state value)
    {
        return value == state::installing ||
            value == state::active;
    }

    constexpr bool CanPublish(
        bool allPagesInstalled,
        bool allProcessorsInstalled,
        bool guestIdWritten)
    {
        return allPagesInstalled &&
            allProcessorsInstalled &&
            guestIdWritten;
    }

    constexpr bool CanBeginRemoval(
        bool completeGroupDiscovered,
        bool allResourcesPresent)
    {
        return completeGroupDiscovered && allResourcesPresent;
    }
}

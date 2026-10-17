#pragma once

#include "Bridge.h"

#include <cstddef>
#include <string>
#include <vector>

namespace unrealdbg_native
{
    void CollectDetailedSystemInformation(
        const SystemInformation& system,
        const std::wstring& applicationDirectory,
        std::vector<SystemDetailItem>& items);

    std::vector<ExternalLogFile> CollectExternalLogs(
        const SystemInformation& system,
        const std::wstring& applicationDirectory);

    bool ReadLogTailText(const std::wstring& path, size_t maximumBytes,
        std::wstring& text, std::wstring& error);
}

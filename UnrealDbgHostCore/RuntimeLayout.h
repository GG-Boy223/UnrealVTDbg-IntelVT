#pragma once

#include "Runtime.h"

namespace unrealdbg::host::detail
{
    class RuntimeLayout final
    {
    public:
        RuntimeLayout() = default;

        [[nodiscard]] static InspectionReport Inspect(RuntimeLayout& layout) noexcept;

        [[nodiscard]] const std::filesystem::path& Directory() const noexcept;
        [[nodiscard]] std::filesystem::path UnrealDbgDllPath() const;

    private:
        std::filesystem::path runtime_directory_;
    };
}

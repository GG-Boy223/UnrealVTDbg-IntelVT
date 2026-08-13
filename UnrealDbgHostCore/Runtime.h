#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace unrealdbg::host
{
    enum class RuntimeState : std::uint8_t
    {
        Cold,
        Ready,
        Active,
        Faulted
    };

    enum class InspectionIssueCode : std::uint8_t
    {
        HostExecutableUnavailable,
        RuntimeDirectoryUnavailable,
        MissingDependency,
        DependencyNotRegularFile,
        DependencyProbeFailed,
        InternalFailure
    };

    struct InspectionIssue final
    {
        InspectionIssueCode code{ InspectionIssueCode::InternalFailure };
        std::filesystem::path subject;
        std::wstring message;
    };

    struct InspectionReport final
    {
        std::filesystem::path runtime_directory;
        std::vector<InspectionIssue> issues;

        [[nodiscard]] bool Succeeded() const noexcept
        {
            return issues.empty();
        }
    };

    enum class OperationError : std::uint8_t
    {
        None,
        RuntimeUnavailable,
        InvalidState,
        AlreadyActivated,
        InspectionFailed,
        AdapterLoadFailed,
        MissingExport,
        ActivationFailed,
        InvalidDebuggerPath,
        DebuggerNotFound,
        LaunchFailed,
        InternalFailure
    };

    struct OperationResult final
    {
        OperationError error{ OperationError::None };
        std::wstring message;

        [[nodiscard]] bool Succeeded() const noexcept
        {
            return error == OperationError::None;
        }
    };

    class Runtime final
    {
    public:
        Runtime() noexcept;
        ~Runtime() noexcept;

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;
        Runtime(Runtime&&) = delete;
        Runtime& operator=(Runtime&&) = delete;

        [[nodiscard]] InspectionReport Inspect() noexcept;
        [[nodiscard]] OperationResult Activate() noexcept;
        [[nodiscard]] OperationResult Launch(const std::filesystem::path& debugger_path) noexcept;
        [[nodiscard]] RuntimeState State() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}

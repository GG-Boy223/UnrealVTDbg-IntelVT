#include "pch.h"

#include "RuntimeLayout.h"

namespace unrealdbg::host::detail
{
    namespace
    {
        constexpr std::array<std::wstring_view, 8> kRequiredRuntimeFiles{
            L"UnrealDbgDll.dll",
            L"AIHelper.dll",
            L"Hook.dll",
            L"Hook64.dll",
            L"VT_Driver.sys",
            L"DbgkSysWin10.sys",
            L"DbgkSysWin11.sys",
            L"VMProtectSDK64.dll"
        };

        void AddIssue(
            InspectionReport& report,
            const InspectionIssueCode code,
            const std::filesystem::path& subject,
            std::wstring message)
        {
            report.issues.push_back(InspectionIssue{ code, subject, std::move(message) });
        }

        bool DiscoverHostExecutable(
            std::filesystem::path& host_executable,
            InspectionReport& report)
        {
            constexpr std::size_t kInitialBufferSize = 512;
            constexpr std::size_t kMaximumBufferSize = 32768;

            std::vector<wchar_t> buffer(kInitialBufferSize);
            for (;;)
            {
                ::SetLastError(ERROR_SUCCESS);
                const DWORD copied = ::GetModuleFileNameW(
                    nullptr,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()));

                if (copied == 0)
                {
                    const DWORD error = ::GetLastError();
                    AddIssue(
                        report,
                        InspectionIssueCode::HostExecutableUnavailable,
                        {},
                        L"Unable to determine the host executable path (Win32 error "
                            + std::to_wstring(error) + L").");
                    return false;
                }

                if (static_cast<std::size_t>(copied) < buffer.size())
                {
                    host_executable = std::filesystem::path(
                        std::wstring(buffer.data(), static_cast<std::size_t>(copied)));
                    return true;
                }

                if (buffer.size() >= kMaximumBufferSize)
                {
                    AddIssue(
                        report,
                        InspectionIssueCode::HostExecutableUnavailable,
                        {},
                        L"The host executable path exceeds the supported Windows path length.");
                    return false;
                }

                const std::size_t next_size =
                    (buffer.size() * 2 < kMaximumBufferSize)
                    ? buffer.size() * 2
                    : kMaximumBufferSize;
                buffer.resize(next_size);
            }
        }

        InspectionReport InternalFailureReport() noexcept
        {
            InspectionReport report;
            try
            {
                AddIssue(
                    report,
                    InspectionIssueCode::InternalFailure,
                    {},
                    L"Runtime dependency inspection failed unexpectedly.");
            }
            catch (...)
            {
            }
            return report;
        }
    }

    InspectionReport RuntimeLayout::Inspect(RuntimeLayout& layout) noexcept
    {
        try
        {
            layout.runtime_directory_.clear();

            InspectionReport report;
            std::filesystem::path host_executable;
            if (!DiscoverHostExecutable(host_executable, report))
            {
                return report;
            }

            const std::filesystem::path runtime_directory = host_executable.parent_path();
            if (runtime_directory.empty())
            {
                AddIssue(
                    report,
                    InspectionIssueCode::RuntimeDirectoryUnavailable,
                    host_executable,
                    L"The host executable has no discoverable parent directory.");
                return report;
            }

            layout.runtime_directory_ = runtime_directory;
            report.runtime_directory = runtime_directory;

            for (const std::wstring_view file_name : kRequiredRuntimeFiles)
            {
                const std::filesystem::path dependency =
                    runtime_directory / std::filesystem::path(file_name);
                std::error_code error;
                const std::filesystem::file_status status =
                    std::filesystem::status(dependency, error);

                if (error == std::errc::no_such_file_or_directory
                    || status.type() == std::filesystem::file_type::not_found)
                {
                    AddIssue(
                        report,
                        InspectionIssueCode::MissingDependency,
                        dependency,
                        L"Required runtime dependency is missing.");
                    continue;
                }

                if (error)
                {
                    AddIssue(
                        report,
                        InspectionIssueCode::DependencyProbeFailed,
                        dependency,
                        L"Unable to inspect the runtime dependency (filesystem error "
                            + std::to_wstring(error.value()) + L").");
                    continue;
                }

                if (!std::filesystem::is_regular_file(status))
                {
                    AddIssue(
                        report,
                        InspectionIssueCode::DependencyNotRegularFile,
                        dependency,
                        L"Required runtime dependency is not a regular file.");
                }
            }

            return report;
        }
        catch (...)
        {
            layout.runtime_directory_.clear();
            return InternalFailureReport();
        }
    }

    const std::filesystem::path& RuntimeLayout::Directory() const noexcept
    {
        return runtime_directory_;
    }

    std::filesystem::path RuntimeLayout::UnrealDbgDllPath() const
    {
        return runtime_directory_ / L"UnrealDbgDll.dll";
    }
}

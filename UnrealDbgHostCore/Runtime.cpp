#include "pch.h"

#include "Runtime.h"

#include "LegacyUnrealDbgDllAdapter.h"
#include "RuntimeLayout.h"
#include "Shared/DriverProtocol.h"

namespace unrealdbg::host
{
    namespace
    {
        OperationResult Success(std::wstring message)
        {
            return OperationResult{
                OperationError::None,
                std::move(message)
            };
        }

        OperationResult Failure(
            const OperationError error,
            std::wstring message)
        {
            return OperationResult{
                error,
                std::move(message)
            };
        }

        OperationResult InternalOperationFailure() noexcept
        {
            OperationResult result;
            result.error = OperationError::InternalFailure;
            try
            {
                result.message = L"The runtime operation failed unexpectedly.";
            }
            catch (...)
            {
            }
            return result;
        }

        OperationResult RuntimeUnavailableFailure() noexcept
        {
            OperationResult result;
            result.error = OperationError::RuntimeUnavailable;
            try
            {
                result.message = L"Runtime storage could not be allocated.";
            }
            catch (...)
            {
            }
            return result;
        }

        InspectionReport InternalInspectionFailure() noexcept
        {
            InspectionReport report;
            try
            {
                report.issues.push_back(InspectionIssue{
                    InspectionIssueCode::InternalFailure,
                    {},
                    L"The runtime inspection failed unexpectedly."
                });
            }
            catch (...)
            {
            }
            return report;
        }
    }

    struct Runtime::Impl final
    {
        Impl() noexcept = default;

        InspectionReport InspectLocked()
        {
            detail::RuntimeLayout candidate;
            InspectionReport report = detail::RuntimeLayout::Inspect(candidate);
            if (report.Succeeded())
            {
                layout.emplace(std::move(candidate));
                if (state != RuntimeState::Active && state != RuntimeState::Faulted)
                {
                    state = RuntimeState::Ready;
                }
            }
            else if (state != RuntimeState::Active && state != RuntimeState::Faulted)
            {
                layout.reset();
                state = RuntimeState::Cold;
            }

            return report;
        }

        OperationResult ActivateLocked()
        {
            if (activation_attempted)
            {
                return Failure(
                    OperationError::AlreadyActivated,
                    state == RuntimeState::Active
                        ? L"Runtime activation has already completed."
                        : L"Runtime activation has already been attempted.");
            }

            activation_attempted = true;

            const InspectionReport inspection = InspectLocked();
            if (!inspection.Succeeded() || !layout.has_value())
            {
                state = RuntimeState::Faulted;
                return Failure(
                    OperationError::InspectionFailed,
                    L"Runtime activation was blocked by dependency inspection.");
            }

            const detail::LegacyAdapterResult load_result =
                adapter.Load(layout->UnrealDbgDllPath());
            if (!load_result.Succeeded())
            {
                state = RuntimeState::Faulted;
                return Failure(
                    load_result.error == detail::LegacyAdapterError::MissingExport
                        ? OperationError::MissingExport
                        : OperationError::AdapterLoadFailed,
                    load_result.message);
            }

            const detail::LegacyAdapterResult activation_result =
                adapter.Initialize(static_cast<std::uint64_t>(VT_INIT_HANDSHAKE_KEY));
            if (!activation_result.Succeeded())
            {
                state = RuntimeState::Faulted;
                return Failure(
                    OperationError::ActivationFailed,
                    activation_result.message);
            }

            state = RuntimeState::Active;
            return Success(L"Runtime is active.");
        }

        OperationResult LaunchLocked(const std::filesystem::path& debugger_path)
        {
            if (state != RuntimeState::Active || !layout.has_value())
            {
                return Failure(
                    OperationError::InvalidState,
                    L"Debugger launch requires an Active runtime.");
            }

            if (debugger_path.empty() || !debugger_path.is_absolute())
            {
                return Failure(
                    OperationError::InvalidDebuggerPath,
                    L"Debugger path must be an absolute executable path.");
            }

            std::error_code error;
            const std::filesystem::file_status debugger_status =
                std::filesystem::status(debugger_path, error);
            if (error == std::errc::no_such_file_or_directory
                || debugger_status.type() == std::filesystem::file_type::not_found)
            {
                return Failure(
                    OperationError::DebuggerNotFound,
                    L"Debugger executable does not exist.");
            }
            if (error || !std::filesystem::is_regular_file(debugger_status))
            {
                return Failure(
                    OperationError::InvalidDebuggerPath,
                    L"Debugger path is not an accessible regular file.");
            }

            const detail::LegacyAdapterResult launch_result =
                adapter.StartProcess(debugger_path, layout->Directory());
            if (!launch_result.Succeeded())
            {
                return Failure(
                    OperationError::LaunchFailed,
                    launch_result.message);
            }

            return Success(L"Debugger launched successfully.");
        }

        mutable std::mutex mutex;
        RuntimeState state{ RuntimeState::Cold };
        bool activation_attempted{ false };
        std::optional<detail::RuntimeLayout> layout;
        detail::LegacyUnrealDbgDllAdapter adapter;
    };

    Runtime::Runtime() noexcept
        : impl_(new (std::nothrow) Impl())
    {
    }

    Runtime::~Runtime() noexcept = default;

    InspectionReport Runtime::Inspect() noexcept
    {
        if (!impl_)
        {
            return InternalInspectionFailure();
        }

        try
        {
            const std::lock_guard lock(impl_->mutex);
            return impl_->InspectLocked();
        }
        catch (...)
        {
            return InternalInspectionFailure();
        }
    }

    OperationResult Runtime::Activate() noexcept
    {
        if (!impl_)
        {
            return RuntimeUnavailableFailure();
        }

        try
        {
            const std::lock_guard lock(impl_->mutex);
            return impl_->ActivateLocked();
        }
        catch (...)
        {
            try
            {
                const std::lock_guard lock(impl_->mutex);
                impl_->activation_attempted = true;
                impl_->state = RuntimeState::Faulted;
            }
            catch (...)
            {
            }
            return InternalOperationFailure();
        }
    }

    OperationResult Runtime::Launch(
        const std::filesystem::path& debugger_path) noexcept
    {
        if (!impl_)
        {
            return RuntimeUnavailableFailure();
        }

        try
        {
            const std::lock_guard lock(impl_->mutex);
            return impl_->LaunchLocked(debugger_path);
        }
        catch (...)
        {
            return InternalOperationFailure();
        }
    }

    RuntimeState Runtime::State() const noexcept
    {
        if (!impl_)
        {
            return RuntimeState::Faulted;
        }

        try
        {
            const std::lock_guard lock(impl_->mutex);
            return impl_->state;
        }
        catch (...)
        {
            return RuntimeState::Faulted;
        }
    }
}

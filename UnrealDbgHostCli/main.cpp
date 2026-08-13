#include "pch.h"

#include "Runtime.h"

namespace
{
    using unrealdbg::host::InspectionIssueCode;
    using unrealdbg::host::InspectionReport;
    using unrealdbg::host::OperationResult;
    using unrealdbg::host::Runtime;
    using unrealdbg::host::RuntimeState;

    enum class ExitCode : int
    {
        Success = 0,
        Usage = 2,
        UnsupportedArchitecture = 10,
        AdministratorProbeFailed = 11,
        AdministratorRequired = 12,
        DependencyCheckFailed = 13,
        ActivationFailed = 20,
        InvalidDebuggerPath = 30,
        LaunchFailed = 31,
        InputFailure = 40,
        InternalFailure = 70
    };

    struct AdministratorStatus final
    {
        bool query_succeeded{ false };
        bool is_administrator{ false };
        DWORD win32_error{ ERROR_SUCCESS };
    };

    void ConfigureUnicodeIo() noexcept
    {
        ::SetConsoleCP(CP_UTF8);
        ::SetConsoleOutputCP(CP_UTF8);

        try
        {
            const std::locale utf8_locale(".UTF-8");
            std::wcin.imbue(utf8_locale);
            std::wcout.imbue(utf8_locale);
            std::wcerr.imbue(utf8_locale);
        }
        catch (...)
        {
        }
    }

    template <typename... Values>
    void WriteLine(Values&&... values) noexcept
    {
        try
        {
            std::wosyncstream output(std::wcout);
            (output << ... << std::forward<Values>(values));
            output << L'\n';
        }
        catch (...)
        {
        }
    }

    void WritePrompt() noexcept
    {
        try
        {
            std::wosyncstream output(std::wcout);
            output << L"host> " << std::flush;
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] constexpr bool IsX64Process() noexcept
    {
#if defined(_WIN64)
        return sizeof(void*) == 8;
#else
        return false;
#endif
    }

    [[nodiscard]] AdministratorStatus QueryAdministratorStatus() noexcept
    {
        AdministratorStatus result;
        std::array<unsigned char, SECURITY_MAX_SID_SIZE> sid_buffer{};
        DWORD sid_size = static_cast<DWORD>(sid_buffer.size());

        if (::CreateWellKnownSid(
            WinBuiltinAdministratorsSid,
            nullptr,
            sid_buffer.data(),
            &sid_size) == FALSE)
        {
            result.win32_error = ::GetLastError();
            return result;
        }

        BOOL is_member = FALSE;
        if (::CheckTokenMembership(
            nullptr,
            reinterpret_cast<PSID>(sid_buffer.data()),
            &is_member) == FALSE)
        {
            result.win32_error = ::GetLastError();
            return result;
        }

        result.query_succeeded = true;
        result.is_administrator = is_member != FALSE;
        return result;
    }

    [[nodiscard]] std::wstring_view RuntimeStateName(
        const RuntimeState state) noexcept
    {
        switch (state)
        {
        case RuntimeState::Cold:
            return L"Cold";
        case RuntimeState::Ready:
            return L"Ready";
        case RuntimeState::Active:
            return L"Active";
        case RuntimeState::Faulted:
            return L"Faulted";
        }
        return L"Unknown";
    }

    [[nodiscard]] std::wstring_view InspectionIssueName(
        const InspectionIssueCode code) noexcept
    {
        switch (code)
        {
        case InspectionIssueCode::HostExecutableUnavailable:
            return L"host-path";
        case InspectionIssueCode::RuntimeDirectoryUnavailable:
            return L"runtime-directory";
        case InspectionIssueCode::MissingDependency:
            return L"missing-dependency";
        case InspectionIssueCode::DependencyNotRegularFile:
            return L"invalid-dependency";
        case InspectionIssueCode::DependencyProbeFailed:
            return L"dependency-probe";
        case InspectionIssueCode::InternalFailure:
            return L"internal";
        }
        return L"unknown";
    }

    void PrintInspectionIssues(const InspectionReport& report) noexcept
    {
        if (!report.runtime_directory.empty())
        {
            WriteLine(L"runtime directory: ", report.runtime_directory.native());
        }

        for (const auto& issue : report.issues)
        {
            if (issue.subject.empty())
            {
                WriteLine(
                    L"[fail] ",
                    InspectionIssueName(issue.code),
                    L": ",
                    issue.message);
            }
            else
            {
                WriteLine(
                    L"[fail] ",
                    InspectionIssueName(issue.code),
                    L": ",
                    issue.subject.native(),
                    L" - ",
                    issue.message);
            }
        }
    }

    void PrintUsage() noexcept
    {
        WriteLine(L"Usage:");
        WriteLine(L"  UnrealDbgHostCli doctor");
        WriteLine(L"  UnrealDbgHostCli init");
        WriteLine(L"  UnrealDbgHostCli launch <absolute-exe-path>");
        WriteLine(L"  UnrealDbgHostCli status");
        WriteLine(L"  UnrealDbgHostCli              (interactive mode; quit exits)");
    }

    [[nodiscard]] ExitCode RunDoctor(Runtime& runtime)
    {
        const bool is_x64 = IsX64Process();
        const AdministratorStatus administrator = QueryAdministratorStatus();
        const InspectionReport inspection = runtime.Inspect();

        WriteLine(
            is_x64 ? L"[ok] architecture: x64" : L"[fail] architecture: not x64");

        if (!administrator.query_succeeded)
        {
            WriteLine(
                L"[fail] administrator identity: query failed (Win32 error ",
                administrator.win32_error,
                L")");
        }
        else
        {
            WriteLine(
                administrator.is_administrator
                    ? L"[ok] administrator identity: elevated"
                    : L"[fail] administrator identity: not elevated");
        }

        if (inspection.Succeeded())
        {
            if (!inspection.runtime_directory.empty())
            {
                WriteLine(
                    L"runtime directory: ",
                    inspection.runtime_directory.native());
            }
            WriteLine(L"[ok] runtime dependencies: complete");
        }
        else
        {
            WriteLine(L"[fail] runtime dependencies: incomplete");
            PrintInspectionIssues(inspection);
        }

        if (!is_x64)
        {
            return ExitCode::UnsupportedArchitecture;
        }
        if (!administrator.query_succeeded)
        {
            return ExitCode::AdministratorProbeFailed;
        }
        if (!administrator.is_administrator)
        {
            return ExitCode::AdministratorRequired;
        }
        if (!inspection.Succeeded())
        {
            return ExitCode::DependencyCheckFailed;
        }
        return ExitCode::Success;
    }

    [[nodiscard]] ExitCode EnsureActive(Runtime& runtime)
    {
        if (runtime.State() == RuntimeState::Active)
        {
            WriteLine(L"runtime state: Active");
            return ExitCode::Success;
        }
        if (runtime.State() == RuntimeState::Faulted)
        {
            WriteLine(L"[fail] runtime is Faulted; activation cannot be retried.");
            return ExitCode::ActivationFailed;
        }

        const InspectionReport inspection = runtime.Inspect();
        if (!inspection.Succeeded())
        {
            PrintInspectionIssues(inspection);
            return ExitCode::DependencyCheckFailed;
        }

        const OperationResult activation = runtime.Activate();
        if (!activation.Succeeded())
        {
            WriteLine(L"[fail] activation: ", activation.message);
            return ExitCode::ActivationFailed;
        }

        WriteLine(L"[ok] activation: ", activation.message);
        return ExitCode::Success;
    }

    [[nodiscard]] bool ValidateDebuggerPath(
        const std::filesystem::path& debugger_path) noexcept
    {
        try
        {
            if (debugger_path.empty() || !debugger_path.is_absolute())
            {
                WriteLine(L"[fail] launch path must be absolute.");
                return false;
            }

            std::error_code error;
            const std::filesystem::file_status status =
                std::filesystem::status(debugger_path, error);
            if (error || !std::filesystem::is_regular_file(status))
            {
                WriteLine(
                    L"[fail] launch path is not an accessible regular file: ",
                    debugger_path.native());
                return false;
            }
            return true;
        }
        catch (...)
        {
            WriteLine(L"[fail] launch path validation failed.");
            return false;
        }
    }

    [[nodiscard]] ExitCode RunLaunch(
        Runtime& runtime,
        const std::wstring_view debugger_path_text)
    {
        const std::filesystem::path debugger_path(debugger_path_text);
        if (!ValidateDebuggerPath(debugger_path))
        {
            return ExitCode::InvalidDebuggerPath;
        }

        const ExitCode activation = EnsureActive(runtime);
        if (activation != ExitCode::Success)
        {
            return activation;
        }

        const OperationResult launch = runtime.Launch(debugger_path);
        if (!launch.Succeeded())
        {
            WriteLine(L"[fail] launch: ", launch.message);
            return ExitCode::LaunchFailed;
        }

        WriteLine(L"[ok] launch: ", launch.message);
        return ExitCode::Success;
    }

    [[nodiscard]] ExitCode RunStatus(const Runtime& runtime) noexcept
    {
        const RuntimeState state = runtime.State();
        WriteLine(L"runtime state: ", RuntimeStateName(state));
        return state == RuntimeState::Faulted
            ? ExitCode::ActivationFailed
            : ExitCode::Success;
    }

    [[nodiscard]] std::wstring_view Trim(const std::wstring_view value) noexcept
    {
        std::size_t first = 0;
        while (first < value.size()
            && std::iswspace(static_cast<wint_t>(value[first])) != 0)
        {
            ++first;
        }

        std::size_t last = value.size();
        while (last > first
            && std::iswspace(static_cast<wint_t>(value[last - 1])) != 0)
        {
            --last;
        }
        return value.substr(first, last - first);
    }

    [[nodiscard]] std::wstring NormalizeCommand(const std::wstring_view command)
    {
        std::wstring normalized;
        normalized.reserve(command.size());
        for (const wchar_t character : command)
        {
            normalized.push_back(
                static_cast<wchar_t>(std::towlower(static_cast<wint_t>(character))));
        }
        return normalized;
    }

    [[nodiscard]] std::wstring_view RemoveOptionalQuotes(
        const std::wstring_view argument) noexcept
    {
        if (argument.size() >= 2
            && argument.front() == L'"'
            && argument.back() == L'"')
        {
            return argument.substr(1, argument.size() - 2);
        }
        return argument;
    }

    [[nodiscard]] ExitCode DispatchCommand(
        Runtime& runtime,
        const std::wstring_view command_text,
        const std::wstring_view argument_text)
    {
        const std::wstring command = NormalizeCommand(Trim(command_text));
        const std::wstring_view argument = RemoveOptionalQuotes(Trim(argument_text));

        if (command == L"doctor" && argument.empty())
        {
            return RunDoctor(runtime);
        }
        if (command == L"init" && argument.empty())
        {
            return EnsureActive(runtime);
        }
        if (command == L"status" && argument.empty())
        {
            return RunStatus(runtime);
        }
        if (command == L"launch" && !argument.empty())
        {
            return RunLaunch(runtime, argument);
        }

        PrintUsage();
        return ExitCode::Usage;
    }

    [[nodiscard]] ExitCode RunInteractive(Runtime& runtime)
    {
        PrintUsage();
        std::wstring line;

        for (;;)
        {
            WritePrompt();
            if (!std::getline(std::wcin, line))
            {
                return std::wcin.eof()
                    ? ExitCode::Success
                    : ExitCode::InputFailure;
            }

            const std::wstring_view trimmed = Trim(line);
            if (trimmed.empty())
            {
                continue;
            }

            const std::size_t separator = trimmed.find_first_of(L" \t");
            const std::wstring_view command = trimmed.substr(0, separator);
            const std::wstring_view argument =
                separator == std::wstring_view::npos
                ? std::wstring_view{}
                : Trim(trimmed.substr(separator + 1));

            if (NormalizeCommand(command) == L"quit" && argument.empty())
            {
                return ExitCode::Success;
            }

            const ExitCode result = DispatchCommand(runtime, command, argument);
            if (result != ExitCode::Success)
            {
                WriteLine(L"exit code: ", static_cast<int>(result));
            }
        }
    }
}

extern "C" void __stdcall PrintLog(wchar_t* message)
{
    try
    {
        std::wosyncstream output(std::wcout);
        if (message == nullptr)
        {
            output << L"(null)\n";
            return;
        }

        const std::wstring_view text(message);
        output << text;
        if (text.empty() || text.back() != L'\n')
        {
            output << L'\n';
        }
    }
    catch (...)
    {
    }
}

int wmain(const int argc, wchar_t* argv[])
{
    try
    {
        ConfigureUnicodeIo();
        Runtime runtime;
        if (argc == 1)
        {
            return static_cast<int>(RunInteractive(runtime));
        }

        if (argc == 2)
        {
            return static_cast<int>(DispatchCommand(runtime, argv[1], {}));
        }

        if (argc == 3)
        {
            return static_cast<int>(DispatchCommand(runtime, argv[1], argv[2]));
        }

        PrintUsage();
        return static_cast<int>(ExitCode::Usage);
    }
    catch (...)
    {
        WriteLine(L"[fail] unrecoverable host error.");
        return static_cast<int>(ExitCode::InternalFailure);
    }
}

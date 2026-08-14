#include "pch.h"

#include "LegacyUnrealDbgDllAdapter.h"

namespace unrealdbg::host::detail
{
    namespace
    {
        constexpr char kInitializeExport[] = "Initialize_F3";
        constexpr char kStartProcessExport[] = "StartProcess";
        constexpr char kGetFileVersionExport[] = "GetFileVersion";
        constexpr char kBlockGameResumeThreadExport[] = "TL_BlockGameResumeThread";

        using InitializeThunk = BOOL(__stdcall*)(std::uint64_t);
        using StartProcessThunk = BOOL(__stdcall*)(wchar_t*, wchar_t*);

        BOOL InvokeInitialize(
            const InitializeThunk function,
            const std::uint64_t key,
            DWORD* exception_code) noexcept
        {
            __try
            {
                return function(key);
            }
            __except (
                exception_code != nullptr ? (*exception_code = ::GetExceptionCode()) : 0,
                EXCEPTION_EXECUTE_HANDLER)
            {
                return FALSE;
            }
        }

        BOOL InvokeStartProcess(
            const StartProcessThunk function,
            wchar_t* debugger_path,
            wchar_t* runtime_directory,
            DWORD* exception_code) noexcept
        {
            __try
            {
                return function(debugger_path, runtime_directory);
            }
            __except (
                exception_code != nullptr ? (*exception_code = ::GetExceptionCode()) : 0,
                EXCEPTION_EXECUTE_HANDLER)
            {
                return FALSE;
            }
        }

        template <typename Function>
        bool ResolveExport(
            const HMODULE module,
            const char* export_name,
            Function& target) noexcept
        {
            static_assert(sizeof(Function) == sizeof(FARPROC));

            const FARPROC address = ::GetProcAddress(module, export_name);
            if (address == nullptr)
            {
                target = nullptr;
                return false;
            }

            std::memcpy(&target, &address, sizeof(target));
            return target != nullptr;
        }

        void AppendMissingExport(
            std::wstring& missing_exports,
            const std::wstring_view export_name)
        {
            if (!missing_exports.empty())
            {
                missing_exports.append(L", ");
            }
            missing_exports.append(export_name);
        }

        LegacyAdapterResult Success(std::wstring message)
        {
            return LegacyAdapterResult{
                LegacyAdapterError::None,
                std::move(message)
            };
        }

        LegacyAdapterResult Failure(
            const LegacyAdapterError error,
            std::wstring message)
        {
            return LegacyAdapterResult{
                error,
                std::move(message)
            };
        }

        LegacyAdapterResult UnexpectedFailure() noexcept
        {
            LegacyAdapterResult result;
            result.error = LegacyAdapterError::CallFailed;
            try
            {
                result.message =
                    L"The legacy UnrealDbgDll adapter failed unexpectedly.";
            }
            catch (...)
            {
            }
            return result;
        }
    }

    LegacyUnrealDbgDllAdapter::~LegacyUnrealDbgDllAdapter() noexcept
    {
        Reset();
    }

    LegacyAdapterResult LegacyUnrealDbgDllAdapter::Load(
        const std::filesystem::path& dll_path) noexcept
    {
        try
        {
            Reset();

            if (dll_path.empty() || !dll_path.is_absolute())
            {
                return Failure(
                    LegacyAdapterError::LoadFailed,
                    L"UnrealDbgDll.dll must be loaded from an absolute path.");
            }

            const std::wstring dll_path_text = dll_path.native();
            ::SetLastError(ERROR_SUCCESS);
            module_ = ::LoadLibraryExW(
                dll_path_text.c_str(),
                nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (module_ == nullptr)
            {
                const DWORD error = ::GetLastError();
                return Failure(
                    LegacyAdapterError::LoadFailed,
                    L"LoadLibraryExW failed for " + dll_path_text
                        + L" (Win32 error " + std::to_wstring(error) + L").");
            }

            std::wstring missing_exports;
            if (!ResolveExport(module_, kInitializeExport, initialize_))
            {
                AppendMissingExport(missing_exports, L"Initialize_F3");
            }
            if (!ResolveExport(module_, kStartProcessExport, start_process_))
            {
                AppendMissingExport(missing_exports, L"StartProcess");
            }
            if (!ResolveExport(module_, kGetFileVersionExport, get_file_version_))
            {
                AppendMissingExport(missing_exports, L"GetFileVersion");
            }
            if (!ResolveExport(
                module_,
                kBlockGameResumeThreadExport,
                block_game_resume_thread_))
            {
                AppendMissingExport(missing_exports, L"TL_BlockGameResumeThread");
            }

            if (!missing_exports.empty())
            {
                Reset();
                return Failure(
                    LegacyAdapterError::MissingExport,
                    L"UnrealDbgDll.dll is missing required export(s): "
                        + missing_exports + L".");
            }

            return Success(L"UnrealDbgDll.dll loaded and its legacy ABI was verified.");
        }
        catch (...)
        {
            Reset();
            return UnexpectedFailure();
        }
    }

    LegacyAdapterResult LegacyUnrealDbgDllAdapter::Initialize(
        const std::uint64_t key) noexcept
    {
        try
        {
            if (module_ == nullptr || initialize_ == nullptr)
            {
                return Failure(
                    LegacyAdapterError::CallFailed,
                    L"UnrealDbgDll.dll is not ready for initialization.");
            }

            ::SetLastError(ERROR_SUCCESS);
            DWORD exception_code = 0;
            if (InvokeInitialize(initialize_, key, &exception_code) == FALSE)
            {
                const DWORD error = ::GetLastError();
                return Failure(
                    LegacyAdapterError::CallFailed,
                    L"Initialize_F3 returned failure"
                        + (exception_code != 0
                            ? std::wstring(L" (SEH exception ")
                                + std::to_wstring(exception_code) + L")."
                            : error == ERROR_SUCCESS
                            ? std::wstring(L".")
                            : std::wstring(L" (Win32 error ")
                                + std::to_wstring(error) + L")."));
            }

            initialized_ = true;
            return Success(L"Runtime activation completed.");
        }
        catch (...)
        {
            return UnexpectedFailure();
        }
    }

    LegacyAdapterResult LegacyUnrealDbgDllAdapter::StartProcess(
        const std::filesystem::path& debugger_path,
        const std::filesystem::path& runtime_directory) noexcept
    {
        try
        {
            if (module_ == nullptr || start_process_ == nullptr)
            {
                return Failure(
                    LegacyAdapterError::CallFailed,
                    L"UnrealDbgDll.dll is not ready to launch a debugger.");
            }

            std::wstring debugger_path_buffer = debugger_path.native();
            std::wstring runtime_directory_buffer = runtime_directory.native();

            ::SetLastError(ERROR_SUCCESS);
            DWORD exception_code = 0;
            if (InvokeStartProcess(
                start_process_,
                debugger_path_buffer.data(),
                runtime_directory_buffer.data(),
                &exception_code) == FALSE)
            {
                const DWORD error = ::GetLastError();
                return Failure(
                    LegacyAdapterError::CallFailed,
                    L"StartProcess returned failure"
                        + (exception_code != 0
                            ? std::wstring(L" (SEH exception ")
                                + std::to_wstring(exception_code) + L")."
                            : error == ERROR_SUCCESS
                            ? std::wstring(L".")
                            : std::wstring(L" (Win32 error ")
                                + std::to_wstring(error) + L")."));
            }

            return Success(L"Debugger launch completed.");
        }
        catch (...)
        {
            return UnexpectedFailure();
        }
    }

    void LegacyUnrealDbgDllAdapter::Reset() noexcept
    {
        initialize_ = nullptr;
        start_process_ = nullptr;
        get_file_version_ = nullptr;
        block_game_resume_thread_ = nullptr;

        // Initialize_F3 starts a detached heartbeat thread inside the legacy DLL.
        // Once active, leave the module loaded until process teardown to avoid executing unmapped code.
        if (module_ != nullptr && !initialized_)
        {
            ::FreeLibrary(module_);
        }
        module_ = nullptr;
        initialized_ = false;
    }
}

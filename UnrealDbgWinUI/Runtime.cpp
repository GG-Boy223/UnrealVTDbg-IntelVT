#include "Bridge.h"
#include "SystemInspector.h"

#include <Psapi.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using unrealdbg_native::BackendState;
    using unrealdbg_native::DebuggerEntry;
    using unrealdbg_native::LogLevel;
    using unrealdbg_native::RuntimeFileStatus;
    using unrealdbg_native::TargetProcess;

    constexpr ULONG64 kInitializationKey = 0x9DD14D00F5DD0000ull;
    constexpr wchar_t kDebuggerListName[] = L"DebuggerList.ini";
    constexpr wchar_t kConfigName[] = L"Config.ini";
    constexpr wchar_t kDebuggerSection[] = L"配置";

    std::wstring PathInDirectory(const std::wstring& directory, const std::wstring& name)
    {
        return (std::filesystem::path(directory) / name).wstring();
    }

    bool EqualsInsensitive(const std::wstring& left, const std::wstring& right) noexcept
    {
        return _wcsicmp(left.c_str(), right.c_str()) == 0;
    }

    std::wstring ReadRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* valueName)
    {
        DWORD type{};
        DWORD size{};
        if (::RegGetValueW(root, subKey, valueName,
            RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, nullptr, &size) != ERROR_SUCCESS ||
            size < sizeof(wchar_t))
        {
            return {};
        }

        std::wstring buffer(size / sizeof(wchar_t), L'\0');
        if (::RegGetValueW(root, subKey, valueName,
            RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, buffer.data(), &size) != ERROR_SUCCESS)
        {
            return {};
        }
        while (!buffer.empty() && buffer.back() == L'\0')
        {
            buffer.pop_back();
        }
        return buffer;
    }

    bool QueryBuildAndUbr(DWORD& build, DWORD& ubr) noexcept
    {
        using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
        const auto function = ntdll == nullptr ? nullptr :
            reinterpret_cast<RtlGetVersionFn>(::GetProcAddress(ntdll, "RtlGetVersion"));
        if (function == nullptr)
        {
            return false;
        }

        RTL_OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
        if (function(&version) != 0)
        {
            return false;
        }

        build = version.dwBuildNumber;
        ubr = 0;
        DWORD size = sizeof(ubr);
        (void)::RegGetValueW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"UBR",
            RRF_RT_REG_DWORD, nullptr, &ubr, &size);
        return true;
    }

    const wchar_t* ArchitectureLabel(const WORD architecture) noexcept
    {
        switch (architecture)
        {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return L"x64";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return L"ARM64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return L"x86";
        case PROCESSOR_ARCHITECTURE_ARM:
            return L"ARM";
        default:
            return L"未知架构";
        }
    }

    std::wstring FormatBytes(std::uint64_t bytes)
    {
        if (bytes == 0)
        {
            return L"不可用";
        }
        wchar_t buffer[48]{};
        const double gigabytes = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
        swprintf_s(buffer, L"%.1f GB", gigabytes);
        return buffer;
    }

    std::wstring CurrentClockText()
    {
        SYSTEMTIME time{};
        ::GetLocalTime(&time);
        wchar_t buffer[48]{};
        swprintf_s(buffer, L"%04u-%02u-%02u  %02u:%02u",
            time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
        return buffer;
    }

    std::wstring ReadIniString(const std::wstring& path,
        const wchar_t* section, const wchar_t* key)
    {
        std::vector<wchar_t> buffer(512);
        for (;;)
        {
            const DWORD copied = ::GetPrivateProfileStringW(section, key, L"",
                buffer.data(), static_cast<DWORD>(buffer.size()), path.c_str());
            if (copied < buffer.size() - 1)
            {
                return std::wstring(buffer.data(), copied);
            }
            if (buffer.size() >= 32768)
            {
                return {};
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    int ReadIniInteger(const std::wstring& path,
        const wchar_t* section, const wchar_t* key, const int fallback)
    {
        const std::wstring value = ReadIniString(path, section, key);
        if (value.empty())
        {
            return fallback;
        }
        wchar_t* end = nullptr;
        const long parsed = wcstol(value.c_str(), &end, 10);
        return end != value.c_str() && parsed >= 0 && parsed <= 4096
            ? static_cast<int>(parsed) : fallback;
    }

    std::uint64_t ReadIniUInt64(const std::wstring& path,
        const wchar_t* section, const wchar_t* key,
        const std::uint64_t fallback)
    {
        const std::wstring value = ReadIniString(path, section, key);
        if (value.empty())
        {
            return fallback;
        }
        wchar_t* end = nullptr;
        const unsigned long long parsed = wcstoull(value.c_str(), &end, 0);
        return end != value.c_str()
            ? static_cast<std::uint64_t>(parsed) : fallback;
    }

    bool ReadIniBoolean(const std::wstring& path,
        const wchar_t* section, const wchar_t* key, const bool fallback)
    {
        const std::wstring value = ReadIniString(path, section, key);
        if (value.empty())
        {
            return fallback;
        }
        return value == L"1" ||
            EqualsInsensitive(value, L"true") ||
            EqualsInsensitive(value, L"yes") ||
            EqualsInsensitive(value, L"on");
    }

    bool ValidateDebuggerFile(const std::wstring& path)
    {
        if (!unrealdbg_native::FileExists(path))
        {
            return false;
        }
        const DWORD attributes = ::GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::wstring DecodeNameFromWindow(const HWND window, const DWORD processId)
    {
        wchar_t title[256]{};
        ::GetWindowTextW(window, title, static_cast<int>(std::size(title)));
        if (title[0] != L'\0')
        {
            return title;
        }
        return L"进程 " + std::to_wstring(processId);
    }
}

namespace unrealdbg_native
{
    std::wstring GetApplicationDirectory()
    {
        std::vector<wchar_t> buffer(MAX_PATH);
        for (;;)
        {
            const DWORD length = ::GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::filesystem::current_path().wstring();
            }
            if (length < buffer.size() - 1)
            {
                const std::filesystem::path path(
                    std::wstring(buffer.data(), length));
                return path.parent_path().wstring();
            }
            if (buffer.size() >= 32768)
            {
                return std::filesystem::current_path().wstring();
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    std::wstring JoinPath(const std::wstring& left, const std::wstring& right)
    {
        return PathInDirectory(left, right);
    }

    bool FileExists(const std::wstring& path)
    {
        if (path.empty())
        {
            return false;
        }
        std::error_code error;
        return std::filesystem::is_regular_file(path, error) && !error;
    }

    bool DirectoryExists(const std::wstring& path)
    {
        if (path.empty())
        {
            return false;
        }
        std::error_code error;
        return std::filesystem::is_directory(path, error) && !error;
    }

    std::wstring FileNameOf(const std::wstring& path)
    {
        const std::filesystem::path value(path);
        return value.filename().wstring();
    }

    std::wstring GetSystemErrorMessage(const DWORD error)
    {
        wchar_t* message = nullptr;
        const DWORD length = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<wchar_t*>(&message), 0, nullptr);
        if (length == 0 || message == nullptr)
        {
            return L"Win32 错误 " + std::to_wstring(error);
        }

        std::wstring result(message, length);
        ::LocalFree(message);
        while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' ||
            result.back() == L' ' || result.back() == L'.'))
        {
            result.pop_back();
        }
        return result + L"（" + std::to_wstring(error) + L"）";
    }

    bool IsProcessElevated() noexcept
    {
        HANDLE token = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
        {
            return false;
        }

        TOKEN_ELEVATION elevation{};
        DWORD returned{};
        const BOOL ok = ::GetTokenInformation(token, TokenElevation,
            &elevation, sizeof(elevation), &returned);
        ::CloseHandle(token);
        return ok != FALSE && elevation.TokenIsElevated != 0;
    }

    bool EnableDebugPrivilege() noexcept
    {
        HANDLE token = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(),
            TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        {
            return false;
        }

        LUID luid{};
        TOKEN_PRIVILEGES privileges{};
        privileges.PrivilegeCount = 1;
        const BOOL available = ::LookupPrivilegeValueW(
            nullptr, SE_DEBUG_NAME, &luid);
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        const BOOL adjusted = available &&
            ::AdjustTokenPrivileges(token, FALSE, &privileges,
                sizeof(privileges), nullptr, nullptr);
        const DWORD error = ::GetLastError();
        ::CloseHandle(token);
        return adjusted != FALSE && error == ERROR_SUCCESS;
    }

    Runtime::Runtime()
        : applicationDirectory_(GetApplicationDirectory()),
          symbolPreparer_(applicationDirectory_)
    {
    }

    Runtime::~Runtime()
    {
        Shutdown();
    }

    void Runtime::Start()
    {
        ConfigureLogStore(applicationDirectory_);
        WriteLog(LogLevel::Info, L"F3 VT 调试台启动");
        WriteLog(LogLevel::Info, L"运行目录：" + applicationDirectory_);
        WriteLog(LogLevel::Info, L"界面：原生 WinUI 3；后端：UnrealDbgDll");

        if (!EnableDebugPrivilege())
        {
            WriteLog(LogLevel::Error,
                L"SeDebugPrivilege 启用失败；目标进程操作可能受限，请以管理员身份运行");
        }

        LoadSystemInformation();
        LoadDetailedSystemInformation();
        ResolveRuntimeFiles();
        LoadDebuggerList();
        LoadTlSettings();

        if (LoadBackend())
        {
            WriteLog(LogLevel::Info,
                L"后端接口已就绪；进入 VT 调试模式前不会加载驱动");
        }
        else
        {
            WriteLog(LogLevel::Error,
                L"UnrealDbgDll.dll 或必要导出未就绪；请检查运行目录与 bin 目录");
        }

        // Loader 原本在旧 GUI 启动前完成这一步。新界面在首帧后
        // 立即异步校验，不阻塞窗口显示，也不启动独立 Loader 进程。
        symbolPreparer_.PrepareAsync();
    }

    void Runtime::Shutdown() noexcept
    {
        if (initializeWorker_.joinable() &&
            initializeWorker_.get_id() != std::this_thread::get_id())
        {
            initializeWorker_.join();
        }
        symbolPreparer_.Shutdown();
        UnloadBackend();
        WriteLog(LogLevel::Info, L"F3 VT 调试台已退出");
        ShutdownLogStore();
    }

    void Runtime::Refresh()
    {
        LoadSystemInformation();
        LoadDetailedSystemInformation();
        ResolveRuntimeFiles();
        LoadDebuggerList();
        LoadTlSettings();
        WriteLog(LogLevel::Info, L"本地环境与配置文件已刷新");
    }

    void Runtime::BeginInitialize()
    {
        if (!backendLoaded_)
        {
            WriteLog(LogLevel::Error, L"后端 DLL 未加载，无法进入 VT 调试模式");
            return;
        }
        if (!symbolPreparer_.IsReady())
        {
            WriteLog(LogLevel::Error,
                L"符号缓存尚未就绪，不能进入 VT 调试模式；请先完成 Loader 符号准备");
            return;
        }

        BackendState expected = State();
        while (expected == BackendState::Ready || expected == BackendState::Failed)
        {
            if (state_.compare_exchange_weak(expected, BackendState::Initializing,
                std::memory_order_acq_rel, std::memory_order_acquire))
            {
                if (initializeWorker_.joinable())
                {
                    initializeWorker_.join();
                }
                initializeWorker_ = std::thread([this] { InitializeWorker(); });
                return;
            }
        }
    }

    BackendState Runtime::State() const noexcept
    {
        return state_.load(std::memory_order_acquire);
    }

    bool Runtime::IsInitializing() const noexcept
    {
        return State() == BackendState::Initializing;
    }

    bool Runtime::IsInitialized() const noexcept
    {
        return State() == BackendState::Initialized;
    }

    std::wstring Runtime::LastError() const
    {
        std::lock_guard lock(errorMutex_);
        return lastError_;
    }

    bool Runtime::LoadBackend()
    {
        const std::array<std::wstring, 4> candidates = {
            PathInDirectory(applicationDirectory_, L"UnrealDbgDll.dll"),
            PathInDirectory(PathInDirectory(applicationDirectory_, L"bin"), L"UnrealDbgDll.dll"),
            PathInDirectory(std::filesystem::path(applicationDirectory_).parent_path().wstring(),
                L"UnrealDbgDll.dll"),
            PathInDirectory(std::filesystem::path(applicationDirectory_).parent_path().wstring(),
                L"bin\\UnrealDbgDll.dll"),
        };

        for (const std::wstring& candidate : candidates)
        {
            if (!FileExists(candidate))
            {
                continue;
            }

            backendModule_ = ::LoadLibraryExW(candidate.c_str(), nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                    LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                    LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (backendModule_ == nullptr)
            {
                const DWORD error = ::GetLastError();
                WriteLog(LogLevel::Error,
                    L"UnrealDbgDll.dll 加载失败：" + candidate + L"；" +
                        GetSystemErrorMessage(error));
                continue;
            }

            initialize_ = reinterpret_cast<InitializeFn>(
                ::GetProcAddress(backendModule_, "Initialize_F3"));
            startProcess_ = reinterpret_cast<StartProcessFn>(
                ::GetProcAddress(backendModule_, "StartProcess"));
            getFileVersion_ = reinterpret_cast<GetFileVersionFn>(
                ::GetProcAddress(backendModule_, "GetFileVersion"));
            blockResumeThread_ = reinterpret_cast<BlockResumeThreadFn>(
                ::GetProcAddress(backendModule_, "TL_BlockGameResumeThread"));
            configureHooks_ = reinterpret_cast<ConfigureHooksFn>(
                ::GetProcAddress(backendModule_, "ConfigureHooks_F3"));

            if (initialize_ == nullptr || startProcess_ == nullptr ||
                getFileVersion_ == nullptr || blockResumeThread_ == nullptr ||
                configureHooks_ == nullptr)
            {
                WriteLog(LogLevel::Error,
                    L"UnrealDbgDll.dll 导出表不完整：" + candidate);
                UnloadBackend();
                continue;
            }

            const std::wstring binDirectory =
                PathInDirectory(applicationDirectory_, L"bin");
            launchBaseDirectory_ =
                FileExists(PathInDirectory(binDirectory, L"Hook64.dll"))
                ? binDirectory : applicationDirectory_;
            backendPath_ = candidate;
            backendLoaded_ = true;
            if (!configureHooks_(hookSelectionMask_))
            {
                WriteLog(LogLevel::Error,
                    L"后端拒绝当前 Hook 选择掩码；进入 VT 前将再次校验");
            }
            SetState(BackendState::Ready);
            WriteLog(LogLevel::Info, L"已加载后端：" + candidate);
            return true;
        }

        {
            std::lock_guard lock(errorMutex_);
            lastError_ = L"UnrealDbgDll.dll 未找到或加载失败";
        }
        SetState(BackendState::Missing);
        return false;
    }

    void Runtime::UnloadBackend() noexcept
    {
        if (backendModule_ != nullptr)
        {
            if (State() == BackendState::Initialized)
            {
                WriteLog(LogLevel::Info,
                    L"后端已进入 VT 初始化，DLL 将保留到进程退出，避免卸载运行中的代码");
            }
            else
            {
                ::FreeLibrary(backendModule_);
            }
        }
        backendModule_ = nullptr;
        initialize_ = nullptr;
        startProcess_ = nullptr;
        getFileVersion_ = nullptr;
        blockResumeThread_ = nullptr;
        configureHooks_ = nullptr;
        backendLoaded_ = false;
    }

    void Runtime::InitializeWorker()
    {
        WriteLog(LogLevel::Info, L"开始进入 VT 调试模式；正在加载驱动并完成符号握手");
        const bool hooksConfigured = configureHooks_ != nullptr &&
            configureHooks_(hookSelectionMask_) != FALSE;
        if (!hooksConfigured)
        {
            {
                std::lock_guard lock(errorMutex_);
                lastError_ = L"Hook 选择掩码下发失败；未进入 VT";
            }
            SetState(BackendState::Failed);
            WriteLog(LogLevel::Error,
                L"Hook 选择掩码下发失败；未进入 VT");
            return;
        }

        const bool initialized = initialize_ != nullptr &&
            initialize_(kInitializationKey) != FALSE;
        if (initialized)
        {
            {
                std::lock_guard lock(errorMutex_);
                lastError_.clear();
            }
            SetState(BackendState::Initialized);
            WriteLog(LogLevel::Info, L"VT 调试模式初始化成功");
            return;
        }

        {
            std::lock_guard lock(errorMutex_);
            lastError_ = L"UnrealDbgDll.Initialize_F3 返回 FALSE；请查看上方日志尾部";
        }
        SetState(BackendState::Failed);
        WriteLog(LogLevel::Error,
            L"VT 调试模式初始化失败；后端已执行自身回滚，未继续启动调试器");
    }

    void Runtime::LoadSystemInformation()
    {
        constexpr wchar_t kVersionKey[] =
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
        std::wstring product = ReadRegistryString(
            HKEY_LOCAL_MACHINE, kVersionKey, L"ProductName");
        const std::wstring edition = ReadRegistryString(
            HKEY_LOCAL_MACHINE, kVersionKey, L"DisplayVersion");
        if (product.empty())
        {
            product = L"Windows（产品名不可用）";
        }
        system_.osName = edition.empty() ? product : product + L" " + edition;

        DWORD build{};
        DWORD ubr{};
        if (QueryBuildAndUbr(build, ubr))
        {
            system_.windowsBuild = build;
            system_.windows11 = build >= 22000;
            system_.kernelVersion = L"10.0." + std::to_wstring(build) +
                L"." + std::to_wstring(ubr);
        }
        else
        {
            system_.windowsBuild = 0;
            system_.windows11 = false;
            system_.kernelVersion = L"不可用";
        }

        std::wstring cpu = ReadRegistryString(HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            L"ProcessorNameString");
        if (cpu.empty())
        {
            cpu = L"处理器名称不可用";
        }

        SYSTEM_INFO native{};
        ::GetNativeSystemInfo(&native);
        system_.cpuName = cpu + L" · " +
            std::to_wstring(native.dwNumberOfProcessors) + L" 逻辑处理器";
        system_.architecture =
            std::wstring(ArchitectureLabel(native.wProcessorArchitecture)) +
            L" 系统 · x64 进程";

        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        system_.memoryText = ::GlobalMemoryStatusEx(&memory)
            ? FormatBytes(memory.ullTotalPhys) : L"不可用";
        system_.elevated = IsProcessElevated();
        system_.clockText = CurrentClockText();

        WriteLog(LogLevel::Info, L"系统环境已刷新：" + system_.osName +
            L"；内核 " + system_.kernelVersion + L"；" + system_.cpuName);
    }

    void Runtime::LoadDetailedSystemInformation()
    {
        CollectDetailedSystemInformation(
            system_, applicationDirectory_, systemDetails_);
    }

    std::wstring Runtime::ExistingPath(const wchar_t* name) const
    {
        const std::array<std::wstring, 3> candidates = {
            PathInDirectory(applicationDirectory_, name),
            PathInDirectory(PathInDirectory(applicationDirectory_, L"Config"), name),
            PathInDirectory(PathInDirectory(applicationDirectory_, L"bin"), name),
        };
        for (const std::wstring& candidate : candidates)
        {
            if (FileExists(candidate))
            {
                return candidate;
            }
        }
        return candidates.front();
    }

    std::wstring Runtime::PreferredConfigPath(const wchar_t* name) const
    {
        const std::wstring root = PathInDirectory(applicationDirectory_, name);
        if (FileExists(root))
        {
            return root;
        }
        const std::wstring config =
            PathInDirectory(PathInDirectory(applicationDirectory_, L"Config"), name);
        return FileExists(config) ? config : root;
    }

    void Runtime::LoadDebuggerList()
    {
        debuggers_.clear();
        debuggerListPath_ = PreferredConfigPath(kDebuggerListName);

        std::error_code sizeError;
        const std::uintmax_t fileSize =
            std::filesystem::file_size(debuggerListPath_, sizeError);
        const bool missing = !FileExists(debuggerListPath_);
        const bool empty = !sizeError && fileSize == 0;
        if (missing || empty)
        {
            WriteLog(LogLevel::Info, missing
                ? L"未发现 DebuggerList.ini，正在创建空列表"
                : L"DebuggerList.ini 为空，正在重建配置");
            if (SaveDebuggerList())
            {
                WriteLog(LogLevel::Info,
                    L"已创建新的空 DebuggerList.ini");
            }
            return;
        }

        int count = ReadIniInteger(debuggerListPath_, kDebuggerSection, L"Count", 0);
        if (count == 0)
        {
            count = ReadIniInteger(debuggerListPath_, L"DebuggerList", L"Count", 0);
        }

        const wchar_t* section = ReadIniString(
            debuggerListPath_, L"DebuggerList", L"Count").empty()
            ? kDebuggerSection : L"DebuggerList";

        for (int index = 0; index < count; ++index)
        {
            const std::wstring key = L"Debugger" + std::to_wstring(index);
            const std::wstring value = ReadIniString(
                debuggerListPath_, section, key.c_str());
            const size_t separator = value.find(L'&');
            if (separator == std::wstring::npos || separator == 0 ||
                separator + 1 >= value.size())
            {
                continue;
            }

            DebuggerEntry entry{
                value.substr(0, separator),
                value.substr(separator + 1),
            };
            const bool exists = std::any_of(debuggers_.begin(), debuggers_.end(),
                [&entry](const DebuggerEntry& item)
                {
                    return EqualsInsensitive(item.path, entry.path);
                });
            if (!exists)
            {
                debuggers_.push_back(std::move(entry));
            }
        }
        WriteLog(LogLevel::Info, L"已读取调试器列表：" +
            std::to_wstring(debuggers_.size()) + L" 项");
    }

    bool Runtime::SaveDebuggerList()
    {
        if (debuggerListPath_.empty())
        {
            debuggerListPath_ = PathInDirectory(applicationDirectory_, kDebuggerListName);
        }

        std::error_code directoryError;
        const std::filesystem::path target(debuggerListPath_);
        std::filesystem::create_directories(target.parent_path(), directoryError);
        if (directoryError)
        {
            WriteLog(LogLevel::Error,
                L"创建 DebuggerList.ini 目录失败：" +
                    GetSystemErrorMessage(directoryError.value()) +
                    L"；路径=" + debuggerListPath_);
            return false;
        }

        std::wstring text;
        text.reserve(96 + debuggers_.size() * 160);
        text.push_back(L'\uFEFF');
        text += L"[";
        text += kDebuggerSection;
        text += L"]\r\nCount=";
        text += std::to_wstring(debuggers_.size());
        text += L"\r\n";
        for (size_t index = 0; index < debuggers_.size(); ++index)
        {
            text += L"Debugger";
            text += std::to_wstring(index);
            text += L"=";
            text += debuggers_[index].name;
            text += L"&";
            text += debuggers_[index].path;
            text += L"\r\n";
        }

        const HANDLE file = ::CreateFileW(target.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            const DWORD createError = ::GetLastError();
            WriteLog(LogLevel::Error,
                L"创建 DebuggerList.ini 失败：" +
                    GetSystemErrorMessage(createError) +
                    L"；路径=" + debuggerListPath_);
            return false;
        }

        const BYTE* cursor = reinterpret_cast<const BYTE*>(text.data());
        size_t remaining = text.size() * sizeof(wchar_t);
        bool wrote = true;
        DWORD writeError = ERROR_SUCCESS;
        while (remaining != 0)
        {
            const DWORD chunk = static_cast<DWORD>((std::min)(
                remaining, static_cast<size_t>(MAXDWORD)));
            DWORD written{};
            if (!::WriteFile(file, cursor, chunk, &written, nullptr) ||
                written == 0)
            {
                wrote = false;
                writeError = ::GetLastError();
                break;
            }
            cursor += written;
            remaining -= written;
        }
        if (wrote && !::FlushFileBuffers(file))
        {
            wrote = false;
            writeError = ::GetLastError();
        }
        ::CloseHandle(file);

        if (!wrote)
        {
            WriteLog(LogLevel::Error,
                L"写入 DebuggerList.ini 失败：" +
                    GetSystemErrorMessage(writeError) +
                    L"；路径=" + debuggerListPath_);
            return false;
        }
        return true;
    }

    void Runtime::LoadTlSettings()
    {
        configPath_ = PreferredConfigPath(kConfigName);
        tlEnabled_ = ReadIniBoolean(configPath_, L"TL",
            L"enabled_tl_confrontation", false);
        tlGetTickCount_ = ReadIniBoolean(configPath_, L"TL",
            L"handler_gettickcount_check", true);
        tlBlockResumeThread_ = ReadIniBoolean(configPath_, L"TL",
            L"BlockResumeThread", false);
        hookSelectionMask_ = ReadIniUInt64(configPath_,
            L"HookSelection", L"Mask", HOOK_SELECTION_ALL);
        if ((hookSelectionMask_ & ~static_cast<std::uint64_t>(
            HOOK_SELECTION_ALL)) != 0)
        {
            hookSelectionMask_ = HOOK_SELECTION_ALL;
            WriteLog(LogLevel::Error,
                L"Config.ini 中的 Hook 掩码包含未知位，已回退为全部启用");
        }
        const std::wstring theme = ReadIniString(configPath_, L"UI", L"Theme");
        darkTheme_ = !theme.empty() && EqualsInsensitive(theme, L"dark");
    }

    bool Runtime::SaveTlSettings()
    {
        if (configPath_.empty())
        {
            configPath_ = PathInDirectory(applicationDirectory_, kConfigName);
        }

        const auto write = [this](const wchar_t* key, const bool enabled)
        {
            return ::WritePrivateProfileStringW(L"TL", key,
                enabled ? L"1" : L"0", configPath_.c_str()) != FALSE;
        };
        const bool ok = write(L"enabled_tl_confrontation", tlEnabled_) &&
            write(L"handler_gettickcount_check", tlGetTickCount_) &&
            write(L"BlockResumeThread", tlBlockResumeThread_);
        if (!ok)
        {
            WriteLog(LogLevel::Error,
                L"保存 Config.ini 失败：" +
                    GetSystemErrorMessage(::GetLastError()));
        }
        return ok;
    }

    bool Runtime::SaveHookSelection()
    {
        if (configPath_.empty())
        {
            configPath_ = PathInDirectory(applicationDirectory_, kConfigName);
        }
        wchar_t buffer[32]{};
        swprintf_s(buffer, L"0x%016llX",
            static_cast<unsigned long long>(hookSelectionMask_));
        if (!::WritePrivateProfileStringW(L"HookSelection", L"Mask",
            buffer, configPath_.c_str()))
        {
            WriteLog(LogLevel::Error,
                L"保存 Hook 选择失败：" +
                    GetSystemErrorMessage(::GetLastError()));
            return false;
        }
        return true;
    }

    bool Runtime::AddDebugger(const std::wstring& path)
    {
        if (!ValidateDebuggerFile(path))
        {
            WriteLog(LogLevel::Error, L"所选调试器不是有效文件：" + path);
            return false;
        }

        std::error_code error;
        const std::wstring normalized =
            std::filesystem::path(path).lexically_normal().wstring();
        const bool duplicate = std::any_of(debuggers_.begin(), debuggers_.end(),
            [&normalized](const DebuggerEntry& item)
            {
                return EqualsInsensitive(item.path, normalized);
            });
        if (duplicate)
        {
            WriteLog(LogLevel::Info, L"该调试器已在列表中：" + normalized);
            return true;
        }

        debuggers_.push_back(DebuggerEntry{ FileNameOf(normalized), normalized });
        if (!SaveDebuggerList())
        {
            debuggers_.pop_back();
            return false;
        }
        WriteLog(LogLevel::Info, L"已添加调试器：" + normalized);
        return true;
    }

    bool Runtime::DeleteDebugger(const size_t index)
    {
        if (index >= debuggers_.size())
        {
            return false;
        }
        const DebuggerEntry removed = debuggers_[index];
        debuggers_.erase(debuggers_.begin() + static_cast<std::ptrdiff_t>(index));
        if (!SaveDebuggerList())
        {
            debuggers_.insert(
                debuggers_.begin() + static_cast<std::ptrdiff_t>(index),
                removed);
            return false;
        }
        WriteLog(LogLevel::Info, L"已移除调试器：" + removed.path);
        return true;
    }

    bool Runtime::StartDebugger(const size_t index)
    {
        if (index >= debuggers_.size())
        {
            return false;
        }
        if (!IsInitialized())
        {
            WriteLog(LogLevel::Error, L"请先进入 VT 调试模式，再启动外部调试器");
            return false;
        }
        if (startProcess_ == nullptr)
        {
            WriteLog(LogLevel::Error, L"StartProcess 接口未就绪");
            return false;
        }

        std::wstring executable = debuggers_[index].path;
        std::wstring baseDirectory = launchBaseDirectory_;
        if (!startProcess_(executable.data(), baseDirectory.data()))
        {
            WriteLog(LogLevel::Error,
                L"启动调试器失败：" + debuggers_[index].name + L"；" +
                    GetSystemErrorMessage(::GetLastError()));
            return false;
        }
        WriteLog(LogLevel::Info,
            L"已提交调试器启动请求：" + debuggers_[index].path);
        return true;
    }

    bool Runtime::BlockTargetResumeThread()
    {
        if (target_.processId == 0)
        {
            WriteLog(LogLevel::Error, L"尚未选择目标进程");
            return false;
        }
        if (!tlEnabled_ || !tlBlockResumeThread_)
        {
            WriteLog(LogLevel::Info, L"请先启用防检测并打开“阻塞线程恢复”");
            return false;
        }
        if (blockResumeThread_ == nullptr)
        {
            WriteLog(LogLevel::Error, L"TL_BlockGameResumeThread 接口未就绪");
            return false;
        }

        if (!blockResumeThread_(target_.processId))
        {
            WriteLog(LogLevel::Error,
                L"阻塞目标线程恢复失败；PID=" +
                    std::to_wstring(target_.processId));
            return false;
        }
        WriteLog(LogLevel::Info,
            L"已向驱动提交阻塞线程恢复请求；PID=" +
                std::to_wstring(target_.processId));
        return true;
    }

    void Runtime::SetTlEnabled(const bool value)
    {
        tlEnabled_ = value;
        (void)SaveTlSettings();
    }

    void Runtime::SetTlGetTickCount(const bool value)
    {
        tlGetTickCount_ = value;
        (void)SaveTlSettings();
    }

    void Runtime::SetTlBlockResumeThread(const bool value)
    {
        tlBlockResumeThread_ = value;
        (void)SaveTlSettings();
    }

    void Runtime::SetDarkTheme(const bool value)
    {
        darkTheme_ = value;
        if (configPath_.empty())
        {
            configPath_ = PathInDirectory(applicationDirectory_, kConfigName);
        }
        if (!::WritePrivateProfileStringW(L"UI", L"Theme",
            value ? L"dark" : L"light", configPath_.c_str()))
        {
            WriteLog(LogLevel::Error,
                L"保存界面主题失败：" +
                    GetSystemErrorMessage(::GetLastError()));
        }
    }

    bool Runtime::HookConfigurationEditable() const noexcept
    {
        const BackendState state = State();
        return state != BackendState::Initializing &&
            state != BackendState::Initialized;
    }

    bool Runtime::SetHookSelectionMask(const std::uint64_t mask)
    {
        if (!HookConfigurationEditable())
        {
            WriteLog(LogLevel::Error,
                L"Hook 选择已锁定：初始化开始后不允许安装或取消 Hook");
            return false;
        }
        if ((mask & ~static_cast<std::uint64_t>(HOOK_SELECTION_ALL)) != 0)
        {
            WriteLog(LogLevel::Error, L"Hook 选择包含未知位");
            return false;
        }

        const std::uint64_t previous = hookSelectionMask_;
        hookSelectionMask_ = mask;
        if (backendLoaded_ && configureHooks_ != nullptr &&
            !configureHooks_(hookSelectionMask_))
        {
            hookSelectionMask_ = previous;
            WriteLog(LogLevel::Error, L"后端拒绝新的 Hook 选择");
            return false;
        }
        if (!SaveHookSelection())
        {
            hookSelectionMask_ = previous;
            if (backendLoaded_ && configureHooks_ != nullptr)
            {
                (void)configureHooks_(hookSelectionMask_);
            }
            return false;
        }
        return true;
    }

    std::vector<ExternalLogFile> Runtime::ExternalLogs() const
    {
        return CollectExternalLogs(system_, applicationDirectory_);
    }

    bool Runtime::ReadLogTail(const std::wstring& path,
        const size_t maximumBytes, std::wstring& text,
        std::wstring& error) const
    {
        return ReadLogTailText(path, maximumBytes, text, error);
    }

    bool Runtime::CaptureForegroundTarget(const HWND excludedWindow)
    {
        HWND window = ::GetForegroundWindow();
        if (window == nullptr || window == excludedWindow)
        {
            WriteLog(LogLevel::Error, L"没有捕获到前台窗口，请重试");
            return false;
        }

        DWORD processId = 0;
        ::GetWindowThreadProcessId(window, &processId);
        if (processId == 0 || processId == ::GetCurrentProcessId())
        {
            WriteLog(LogLevel::Error, L"不能把调试台自身作为目标进程");
            return false;
        }

        HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE, processId);
        std::wstring path;
        std::wstring name = DecodeNameFromWindow(window, processId);
        if (process != nullptr)
        {
            std::vector<wchar_t> buffer(1024);
            DWORD size = static_cast<DWORD>(buffer.size());
            if (::QueryFullProcessImageNameW(process, 0, buffer.data(), &size))
            {
                path.assign(buffer.data(), size);
                name = FileNameOf(path);
            }
            ::CloseHandle(process);
        }

        target_ = TargetProcess{ processId, name, path };
        WriteLog(LogLevel::Info,
            L"已选择目标进程：" + target_.name + L"（PID " +
                std::to_wstring(target_.processId) + L"）");
        return true;
    }

    void Runtime::ClearTarget()
    {
        target_ = {};
        WriteLog(LogLevel::Info, L"已清除目标进程");
    }

    void Runtime::ResolveRuntimeFiles()
    {
        runtimeFiles_.clear();
        const auto add = [this](const wchar_t* name, std::wstring path)
        {
            const bool present = FileExists(path);
            runtimeFiles_.push_back(RuntimeFileStatus{
                name, std::move(path), present
            });
        };

        add(L"UnrealDbgDll.dll",
            backendPath_.empty() ? ExistingPath(L"UnrealDbgDll.dll") : backendPath_);
        add(L"Hook64.dll", ExistingPath(L"Hook64.dll"));
        add(L"DbgkSysWin10.sys", ExistingPath(L"DbgkSysWin10.sys"));
        add(L"DbgkSysWin11.sys", ExistingPath(L"DbgkSysWin11.sys"));
        add(L"VT_Driver.sys", ExistingPath(L"VT_Driver.sys"));
        add(L"AIHelper.dll", ExistingPath(L"AIHelper.dll"));
        add(L"VMProtectSDK64.dll", ExistingPath(L"VMProtectSDK64.dll"));
    }

    void Runtime::SetState(const BackendState state)
    {
        state_.store(state, std::memory_order_release);
    }
}

#pragma once

#include "SymbolPreparer.h"
#include "../Common/Shared/HookSelection.h"

#include <Windows.h>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace unrealdbg_native
{
    enum class LogLevel
    {
        Debug,
        Info,
        Error,
    };

    struct LogRecord
    {
        LogLevel level{ LogLevel::Info };
        std::wstring text;
    };

    struct DebuggerEntry
    {
        std::wstring name;
        std::wstring path;
    };

    struct TargetProcess
    {
        DWORD processId{};
        std::wstring name;
        std::wstring path;
    };

    struct SystemInformation
    {
        std::wstring osName;
        std::wstring kernelVersion;
        std::wstring cpuName;
        std::wstring memoryText;
        std::wstring architecture;
        std::wstring clockText;
        DWORD windowsBuild{};
        bool windows11{};
        bool elevated{};
    };

    struct SystemDetailItem
    {
        std::wstring group;
        std::wstring label;
        std::wstring value;
    };

    struct ExternalLogFile
    {
        std::wstring name;
        std::wstring path;
        bool available{};
        bool currentSystem{};
    };

    struct RuntimeFileStatus
    {
        std::wstring name;
        std::wstring path;
        bool present{};
    };

    enum class BackendState
    {
        Missing,
        Ready,
        Initializing,
        Initialized,
        Failed,
    };

    void ConfigureLogStore(const std::wstring& applicationDirectory);
    void ShutdownLogStore() noexcept;
    void WriteLog(LogLevel level, std::wstring_view text);
    void DrainLogs(std::vector<LogRecord>& records);
    std::uint32_t LogCount() noexcept;

    [[nodiscard]] std::wstring GetApplicationDirectory();
    [[nodiscard]] std::wstring JoinPath(const std::wstring& left, const std::wstring& right);
    [[nodiscard]] bool FileExists(const std::wstring& path);
    [[nodiscard]] bool DirectoryExists(const std::wstring& path);
    [[nodiscard]] std::wstring FileNameOf(const std::wstring& path);
    [[nodiscard]] std::wstring GetSystemErrorMessage(DWORD error);
    [[nodiscard]] bool IsProcessElevated() noexcept;
    [[nodiscard]] bool EnableDebugPrivilege() noexcept;

    class Runtime final
    {
    public:
        Runtime();
        ~Runtime();

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;

        void Start();
        void Shutdown() noexcept;
        void Refresh();
        void BeginInitialize();

        [[nodiscard]] BackendState State() const noexcept;
        [[nodiscard]] bool BackendLoaded() const noexcept { return backendLoaded_; }
        [[nodiscard]] bool IsInitializing() const noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] std::wstring LastError() const;

        [[nodiscard]] const std::wstring& ApplicationDirectory() const noexcept { return applicationDirectory_; }
        [[nodiscard]] const std::wstring& BackendPath() const noexcept { return backendPath_; }
        [[nodiscard]] const std::wstring& LaunchBaseDirectory() const noexcept { return launchBaseDirectory_; }
        [[nodiscard]] const SystemInformation& System() const noexcept { return system_; }
        [[nodiscard]] const std::vector<SystemDetailItem>& SystemDetails() const noexcept { return systemDetails_; }
        [[nodiscard]] std::vector<ExternalLogFile> ExternalLogs() const;
        [[nodiscard]] bool ReadLogTail(const std::wstring& path, size_t maximumBytes,
            std::wstring& text, std::wstring& error) const;
        [[nodiscard]] SymbolPreparer& Symbols() noexcept { return symbolPreparer_; }
        [[nodiscard]] const SymbolPreparer& Symbols() const noexcept { return symbolPreparer_; }
        [[nodiscard]] const std::vector<DebuggerEntry>& Debuggers() const noexcept { return debuggers_; }
        [[nodiscard]] const std::vector<RuntimeFileStatus>& RuntimeFiles() const noexcept { return runtimeFiles_; }
        [[nodiscard]] const TargetProcess& Target() const noexcept { return target_; }

        bool AddDebugger(const std::wstring& path);
        bool DeleteDebugger(size_t index);
        bool StartDebugger(size_t index);
        bool BlockTargetResumeThread();

        [[nodiscard]] bool TlEnabled() const noexcept { return tlEnabled_; }
        [[nodiscard]] bool TlGetTickCount() const noexcept { return tlGetTickCount_; }
        [[nodiscard]] bool TlBlockResumeThread() const noexcept { return tlBlockResumeThread_; }
        [[nodiscard]] bool DarkTheme() const noexcept { return darkTheme_; }
        [[nodiscard]] bool HookConfigurationEditable() const noexcept;
        [[nodiscard]] std::uint64_t HookSelectionMask() const noexcept { return hookSelectionMask_; }
        void SetTlEnabled(bool value);
        void SetTlGetTickCount(bool value);
        void SetTlBlockResumeThread(bool value);
        void SetDarkTheme(bool value);
        bool SetHookSelectionMask(std::uint64_t mask);

        bool CaptureForegroundTarget(HWND excludedWindow);
        void ClearTarget();

    private:
        using InitializeFn = BOOL(*)(ULONG64);
        using StartProcessFn = BOOL(*)(wchar_t*, wchar_t*);
        using GetFileVersionFn = void(*)(wchar_t*, wchar_t*);
        using BlockResumeThreadFn = BOOL(*)(DWORD);
        using ConfigureHooksFn = BOOL(*)(ULONG64);

        bool LoadBackend();
        void UnloadBackend() noexcept;
        void InitializeWorker();
        void LoadSystemInformation();
        void LoadDetailedSystemInformation();
        void LoadDebuggerList();
        bool SaveDebuggerList();
        void LoadTlSettings();
        bool SaveTlSettings();
        bool SaveHookSelection();
        void ResolveRuntimeFiles();
        [[nodiscard]] std::wstring ExistingPath(const wchar_t* name) const;
        [[nodiscard]] std::wstring PreferredConfigPath(const wchar_t* name) const;
        void SetState(BackendState state);

        std::wstring applicationDirectory_;
        SymbolPreparer symbolPreparer_;
        std::wstring backendPath_;
        std::wstring launchBaseDirectory_;
        std::wstring debuggerListPath_;
        std::wstring configPath_;
        SystemInformation system_;
        std::vector<DebuggerEntry> debuggers_;
        std::vector<RuntimeFileStatus> runtimeFiles_;
        std::vector<SystemDetailItem> systemDetails_;
        TargetProcess target_;
        std::atomic<BackendState> state_{ BackendState::Missing };
        mutable std::mutex errorMutex_;
        std::wstring lastError_;
        std::thread initializeWorker_;
        HMODULE backendModule_{};
        InitializeFn initialize_{};
        StartProcessFn startProcess_{};
        GetFileVersionFn getFileVersion_{};
        BlockResumeThreadFn blockResumeThread_{};
        ConfigureHooksFn configureHooks_{};
        bool backendLoaded_{};
        bool tlEnabled_{};
        bool tlGetTickCount_{ true };
        bool tlBlockResumeThread_{};
        bool darkTheme_{ false };
        std::uint64_t hookSelectionMask_{ HOOK_SELECTION_ALL };
    };
}

extern "C" __declspec(dllexport) void __stdcall PrintLog(wchar_t* text);

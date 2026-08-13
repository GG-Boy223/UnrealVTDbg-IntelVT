#pragma once

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace unrealdbg::host::detail
{
    enum class LegacyAdapterError : std::uint8_t
    {
        None,
        LoadFailed,
        MissingExport,
        CallFailed
    };

    struct LegacyAdapterResult final
    {
        LegacyAdapterError error{ LegacyAdapterError::None };
        std::wstring message;

        [[nodiscard]] bool Succeeded() const noexcept
        {
            return error == LegacyAdapterError::None;
        }
    };

    class LegacyUnrealDbgDllAdapter final
    {
    public:
        LegacyUnrealDbgDllAdapter() noexcept = default;
        ~LegacyUnrealDbgDllAdapter() noexcept;

        LegacyUnrealDbgDllAdapter(const LegacyUnrealDbgDllAdapter&) = delete;
        LegacyUnrealDbgDllAdapter& operator=(const LegacyUnrealDbgDllAdapter&) = delete;
        LegacyUnrealDbgDllAdapter(LegacyUnrealDbgDllAdapter&&) = delete;
        LegacyUnrealDbgDllAdapter& operator=(LegacyUnrealDbgDllAdapter&&) = delete;

        [[nodiscard]] LegacyAdapterResult Load(
            const std::filesystem::path& dll_path) noexcept;
        [[nodiscard]] LegacyAdapterResult Initialize(std::uint64_t key) noexcept;
        [[nodiscard]] LegacyAdapterResult StartProcess(
            const std::filesystem::path& debugger_path,
            const std::filesystem::path& runtime_directory) noexcept;

    private:
        using InitializeFunction = BOOL(__stdcall*)(std::uint64_t);
        using StartProcessFunction = BOOL(__stdcall*)(wchar_t*, wchar_t*);
        using GetFileVersionFunction = void(__stdcall*)(wchar_t*, wchar_t*);
        using BlockGameResumeThreadFunction = BOOL(__stdcall*)(DWORD);

        void Reset() noexcept;

        HMODULE module_{ nullptr };
        InitializeFunction initialize_{ nullptr };
        StartProcessFunction start_process_{ nullptr };
        GetFileVersionFunction get_file_version_{ nullptr };
        BlockGameResumeThreadFunction block_game_resume_thread_{ nullptr };
        bool initialized_{ false };
    };
}

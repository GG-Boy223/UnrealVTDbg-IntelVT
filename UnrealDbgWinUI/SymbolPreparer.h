#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace unrealdbg_native
{
    enum class SymbolPreparationState
    {
        Idle,
        Preparing,
        Ready,
        Failed,
    };

    enum class SymbolModuleState
    {
        Pending,
        Checking,
        Downloading,
        Cached,
        Downloaded,
        Failed,
    };

    enum class SymbolAnalysisState
    {
        Idle,
        Analyzing,
        Ready,
        Failed,
    };

    struct SystemSymbolEntry
    {
        std::wstring moduleName;
        std::wstring symbolName;
        std::wstring kind;
        std::wstring description;
        std::uint64_t rva{};
        std::uint64_t address{};
        bool found{};
        bool addressValid{};
        bool usedByProject{};
        bool internalSymbol{};
        bool offset{};
    };

    using SystemSymbolSnapshot =
        std::shared_ptr<const std::vector<SystemSymbolEntry>>;

    struct SymbolModuleStatus
    {
        std::wstring moduleName;
        std::wstring modulePath;
        std::wstring pdbName;
        std::wstring pdbPath;
        std::wstring detail;
        SymbolModuleState state{ SymbolModuleState::Pending };
        int progress{};
        std::uint64_t downloadedBytes{};
        std::uint64_t totalBytes{};
    };

    class SymbolPreparer final
    {
    public:
        explicit SymbolPreparer(std::wstring applicationDirectory);
        ~SymbolPreparer();

        SymbolPreparer(const SymbolPreparer&) = delete;
        SymbolPreparer& operator=(const SymbolPreparer&) = delete;

        void PrepareAsync();
        void AnalyzeSystemSymbolsAsync(std::wstring pdbPath = {});
        void Shutdown() noexcept;

        [[nodiscard]] SymbolPreparationState State() const noexcept;
        [[nodiscard]] bool IsPreparing() const noexcept;
        [[nodiscard]] bool IsReady() const noexcept;
        [[nodiscard]] int Progress() const noexcept;
        [[nodiscard]] std::wstring Summary() const;
        [[nodiscard]] std::wstring LastError() const;
        [[nodiscard]] std::wstring CacheDirectory() const;
        [[nodiscard]] std::vector<SymbolModuleStatus> Modules() const;
        [[nodiscard]] SymbolAnalysisState AnalysisState() const noexcept;
        [[nodiscard]] SystemSymbolSnapshot SystemSymbols() const;
        [[nodiscard]] std::wstring AnalysisSummary() const;
        [[nodiscard]] std::wstring AnalysisError() const;
        [[nodiscard]] std::wstring AnalysisRuntimePdb() const;
        [[nodiscard]] std::wstring AnalysisModuleBase() const;

    private:
        struct PdbIdentity
        {
            GUID guid{};
            DWORD age{};
            std::wstring fileName;
        };

        void Worker();
        void SymbolAnalysisWorker(std::wstring requestedPdbPath);
        bool PrepareModule(size_t index, bool& usedCache);
        bool ParsePeIdentity(const std::wstring& modulePath, PdbIdentity& identity) const;
        bool VerifyPdb(const std::wstring& pdbPath, const PdbIdentity& identity) const;
        bool DownloadPdb(size_t index, const SymbolModuleStatus& module,
            const PdbIdentity& identity, const std::wstring& targetPath);
        [[nodiscard]] std::wstring BuildServerBaseUrl() const;
        void SetModule(size_t index, SymbolModuleStatus status);
        void SetProgress(int value) noexcept;
        void UpdateDownloadProgress(size_t index,
            std::uint64_t downloaded, std::uint64_t total);
        void SetLastError(std::wstring value);
        void SetState(SymbolPreparationState state) noexcept;
        void SetAnalysisState(SymbolAnalysisState state) noexcept;

        std::wstring applicationDirectory_;
        std::wstring configPath_;
        std::thread worker_;
        std::thread analysisWorker_;
        std::atomic<SymbolPreparationState> state_{ SymbolPreparationState::Idle };
        std::atomic<int> progress_{};
        std::atomic_bool stopRequested_{ false };
        std::atomic<SymbolAnalysisState> analysisState_{
            SymbolAnalysisState::Idle };
        mutable std::mutex dataMutex_;
        std::vector<SymbolModuleStatus> modules_;
        std::wstring lastError_;
        mutable std::mutex analysisMutex_;
        SystemSymbolSnapshot systemSymbols_;
        std::wstring analysisSummary_;
        std::wstring analysisError_;
        std::wstring analysisRuntimePdb_;
        std::wstring analysisModuleBase_;
    };
}

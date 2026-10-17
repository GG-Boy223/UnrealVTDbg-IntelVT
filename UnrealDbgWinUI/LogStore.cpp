#include "Bridge.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace
{
    using unrealdbg_native::LogLevel;
    using unrealdbg_native::LogRecord;

    constexpr size_t kMaximumPendingRecords = 5000;
    constexpr size_t kTrimBatch = 1000;
    constexpr std::uintmax_t kMaximumLogBytes = 8u * 1024u * 1024u;
    constexpr size_t kMaximumExternalCharacters = 32768;

    std::string ToUtf8(const std::wstring_view text)
    {
        if (text.empty())
        {
            return {};
        }

        const int required = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(required), '\0');
        if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), result.data(), required,
            nullptr, nullptr) != required)
        {
            return {};
        }
        return result;
    }

    std::wstring Timestamp()
    {
        SYSTEMTIME time{};
        ::GetLocalTime(&time);
        wchar_t buffer[40]{};
        swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
            time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
            time.wSecond, time.wMilliseconds);
        return buffer;
    }

    const wchar_t* LevelText(const LogLevel level) noexcept
    {
        switch (level)
        {
        case LogLevel::Debug:
            return L"调试";
        case LogLevel::Error:
            return L"错误";
        case LogLevel::Info:
        default:
            return L"信息";
        }
    }

    bool ContainsInsensitive(const std::wstring_view text, const std::wstring_view needle)
    {
        if (needle.empty() || needle.size() > text.size())
        {
            return false;
        }
        auto lower = [](wchar_t value)
        {
            if (value >= L'A' && value <= L'Z')
            {
                return static_cast<wchar_t>(value - L'A' + L'a');
            }
            return value;
        };

        for (size_t index = 0; index + needle.size() <= text.size(); ++index)
        {
            size_t inner = 0;
            for (; inner < needle.size(); ++inner)
            {
                if (lower(text[index + inner]) != lower(needle[inner]))
                {
                    break;
                }
            }
            if (inner == needle.size())
            {
                return true;
            }
        }
        return false;
    }

    LogLevel ClassifyExternalText(const std::wstring_view text) noexcept
    {
        if (ContainsInsensitive(text, L"[错误]") ||
            ContainsInsensitive(text, L"错误") ||
            ContainsInsensitive(text, L"失败") ||
            ContainsInsensitive(text, L"异常") ||
            ContainsInsensitive(text, L"error") ||
            ContainsInsensitive(text, L"fatal"))
        {
            return LogLevel::Error;
        }

        if (ContainsInsensitive(text, L"[调试]") ||
            ContainsInsensitive(text, L"debug") ||
            ContainsInsensitive(text, L"trace"))
        {
            return LogLevel::Debug;
        }

        return LogLevel::Info;
    }

    class LogStore final
    {
    public:
        void Configure(const std::wstring& applicationDirectory)
        {
            std::lock_guard lock(mutex_);
            if (applicationDirectory.empty())
            {
                pending_.clear();
                sequence_.store(0, std::memory_order_release);
                logPath_.clear();
                fileBytes_ = 0;
                return;
            }

            std::error_code error;
            const std::filesystem::path logDirectory =
                std::filesystem::path(applicationDirectory) / L"Log";
            std::filesystem::create_directories(logDirectory, error);
            if (error)
            {
                return;
            }

            const std::filesystem::path path = logDirectory / L"WinUI.log";
            const std::wstring pathText = path.wstring();
            if (logPath_ == pathText)
            {
                return;
            }

            pending_.clear();
            sequence_.store(0, std::memory_order_release);
            logPath_.clear();
            fileBytes_ = 0;

            const auto size = std::filesystem::file_size(path, error);
            if (!error && size >= kMaximumLogBytes)
            {
                const std::filesystem::path previous = logDirectory / L"WinUI.previous.log";
                std::filesystem::remove(previous, error);
                error.clear();
                std::filesystem::rename(path, previous, error);
            }

            logPath_ = pathText;
            const auto currentSize = std::filesystem::file_size(path, error);
            fileBytes_ = error ? 0 : currentSize;
        }

        void Reset() noexcept
        {
            std::lock_guard lock(mutex_);
            logPath_.clear();
            fileBytes_ = 0;
            pending_.clear();
        }

        void Push(const LogLevel level, const std::wstring_view text)
        {
            if (text.empty())
            {
                return;
            }

            std::wstring line = L"[";
            line += Timestamp();
            line += L"] [";
            line += LevelText(level);
            line += L"] ";
            line.append(text.data(), text.size());
            while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n'))
            {
                line.pop_back();
            }
            line += L"\r\n";

            std::lock_guard lock(mutex_);
            pending_.push_back(LogRecord{ level, std::move(line) });
            if (pending_.size() > kMaximumPendingRecords)
            {
                pending_.erase(pending_.begin(), pending_.begin() + kTrimBatch);
            }
            sequence_.fetch_add(1, std::memory_order_release);
            AppendToFile(pending_.back().text);
        }

        void Drain(std::vector<LogRecord>& records)
        {
            std::lock_guard lock(mutex_);
            if (pending_.empty())
            {
                return;
            }
            if (records.empty())
            {
                records = std::move(pending_);
            }
            else
            {
                records.insert(records.end(),
                    std::make_move_iterator(pending_.begin()),
                    std::make_move_iterator(pending_.end()));
            }
            pending_.clear();
        }

        std::uint32_t Count() const noexcept
        {
            return sequence_.load(std::memory_order_acquire);
        }

    private:
        void AppendToFile(const std::wstring_view line) noexcept
        {
            if (logPath_.empty())
            {
                return;
            }

            const std::string utf8 = ToUtf8(line);
            if (utf8.empty())
            {
                return;
            }

            std::ofstream file(logPath_, std::ios::binary | std::ios::app);
            if (!file.is_open())
            {
                return;
            }
            file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
            fileBytes_ += utf8.size();
        }

        mutable std::mutex mutex_;
        std::vector<LogRecord> pending_;
        std::wstring logPath_;
        std::uintmax_t fileBytes_{};
        std::atomic<std::uint32_t> sequence_{ 0 };
    };

    LogStore g_logStore;
}

namespace unrealdbg_native
{
    void ConfigureLogStore(const std::wstring& applicationDirectory)
    {
        g_logStore.Configure(applicationDirectory);
    }

    void ShutdownLogStore() noexcept
    {
        g_logStore.Reset();
    }

    void WriteLog(const LogLevel level, const std::wstring_view text)
    {
        g_logStore.Push(level, text);
    }

    void DrainLogs(std::vector<LogRecord>& records)
    {
        g_logStore.Drain(records);
    }

    std::uint32_t LogCount() noexcept
    {
        return g_logStore.Count();
    }
}

extern "C" __declspec(dllexport) void __stdcall PrintLog(wchar_t* text)
{
    if (text == nullptr)
    {
        unrealdbg_native::WriteLog(unrealdbg_native::LogLevel::Error,
            L"PrintLog 收到空指针");
        return;
    }

    size_t length = 0;
    while (length < kMaximumExternalCharacters && text[length] != L'\0')
    {
        ++length;
    }

    const std::wstring_view message(text, length);
    unrealdbg_native::WriteLog(ClassifyExternalText(message), message);
}

#include "../Help_Tou/HttpRequestTools.h"

#include <Windows.h>
#include <WinInet.h>

#include <array>
#include <limits>
#include <string_view>
#include <utility>

namespace Help
{
    namespace
    {
        constexpr DWORD kConnectTimeoutMs = 10'000;
        constexpr DWORD kSendTimeoutMs = 10'000;
        constexpr DWORD kMinimumReceiveTimeoutMs = 30'000;

        class InternetHandle final
        {
        public:
            InternetHandle() noexcept = default;
            explicit InternetHandle(HINTERNET handle) noexcept : handle_(handle)
            {
            }

            ~InternetHandle() noexcept
            {
                reset();
            }

            InternetHandle(const InternetHandle&) = delete;
            InternetHandle& operator=(const InternetHandle&) = delete;

            InternetHandle(InternetHandle&& other) noexcept
                : handle_(std::exchange(other.handle_, nullptr))
            {
            }

            InternetHandle& operator=(InternetHandle&& other) noexcept
            {
                if (this != &other)
                {
                    reset(std::exchange(other.handle_, nullptr));
                }
                return *this;
            }

            [[nodiscard]] HINTERNET get() const noexcept
            {
                return handle_;
            }

            explicit operator bool() const noexcept
            {
                return handle_ != nullptr;
            }

            void reset(HINTERNET handle = nullptr) noexcept
            {
                if (handle_ != nullptr)
                {
                    ::InternetCloseHandle(handle_);
                }
                handle_ = handle;
            }

        private:
            HINTERNET handle_{ nullptr };
        };

        std::wstring MultiByteToWide(
            const std::string_view text,
            const UINT code_page,
            const DWORD flags)
        {
            if (text.empty() ||
                text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            {
                return {};
            }

            const int input_size = static_cast<int>(text.size());
            const int output_size = ::MultiByteToWideChar(
                code_page,
                flags,
                text.data(),
                input_size,
                nullptr,
                0);
            if (output_size <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<std::size_t>(output_size), L'\0');
            if (::MultiByteToWideChar(
                code_page,
                flags,
                text.data(),
                input_size,
                result.data(),
                output_size) != output_size)
            {
                return {};
            }
            return result;
        }

        std::wstring ToWide(const std::string_view text)
        {
            std::wstring result = MultiByteToWide(text, CP_UTF8, MB_ERR_INVALID_CHARS);
            if (result.empty() && !text.empty())
            {
                result = MultiByteToWide(text, CP_ACP, 0);
            }
            return result;
        }

        std::string WideToMultiByte(const std::wstring_view text, const UINT code_page)
        {
            if (text.empty() ||
                text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            {
                return {};
            }

            const int input_size = static_cast<int>(text.size());
            const int output_size = ::WideCharToMultiByte(
                code_page,
                0,
                text.data(),
                input_size,
                nullptr,
                0,
                nullptr,
                nullptr);
            if (output_size <= 0)
            {
                return {};
            }

            std::string result(static_cast<std::size_t>(output_size), '\0');
            if (::WideCharToMultiByte(
                code_page,
                0,
                text.data(),
                input_size,
                result.data(),
                output_size,
                nullptr,
                nullptr) != output_size)
            {
                return {};
            }
            return result;
        }

        std::string Utf8ResponseToLocalCodePage(const std::string& response)
        {
            if (response.empty())
            {
                return {};
            }

            const std::wstring wide =
                MultiByteToWide(response, CP_UTF8, MB_ERR_INVALID_CHARS);
            if (wide.empty())
            {
                return response;
            }

            const std::string local = WideToMultiByte(wide, CP_ACP);
            return local.empty() ? response : local;
        }

        std::wstring NormalizeUserAgent(const std::string& agent)
        {
            constexpr std::string_view prefix = "User-Agent:";
            std::string_view value(agent);
            if (value.size() >= prefix.size() &&
                value.compare(0, prefix.size(), prefix) == 0)
            {
                value.remove_prefix(prefix.size());
                while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
                {
                    value.remove_prefix(1);
                }
            }
            return ToWide(value);
        }

        bool SetTimeout(HINTERNET handle, const DWORD option, DWORD value) noexcept
        {
            return ::InternetSetOptionW(
                handle,
                option,
                &value,
                sizeof(value)) != FALSE;
        }
    }

    CHttpRequestTool::CHttpRequestTool(std::string agent)
        : agent_(std::move(agent))
    {
    }

    std::string CHttpRequestTool::HttpGet(std::string url, const int timeOut)
    {
        return ExecuteRequest(L"GET", url, {}, {}, timeOut);
    }

    std::string CHttpRequestTool::HttpPost(
        std::string url,
        std::string postData,
        std::list<std::string> headers,
        const int timeOut)
    {
        return ExecuteRequest(L"POST", url, postData, headers, timeOut);
    }

    std::string CHttpRequestTool::ExecuteRequest(
        const wchar_t* method,
        const std::string& url,
        const std::string& postData,
        const std::list<std::string>& headers,
        const int timeOut)
    {
        try
        {
            const std::wstring wide_url = ToWide(url);
            const std::wstring wide_agent = NormalizeUserAgent(agent_);
            if (wide_url.empty() ||
                wide_agent.empty() ||
                wide_url.size() > static_cast<std::size_t>(MAXDWORD))
            {
                return {};
            }

            URL_COMPONENTSW components{};
            components.dwStructSize = sizeof(components);
            components.dwSchemeLength = static_cast<DWORD>(-1);
            components.dwHostNameLength = static_cast<DWORD>(-1);
            components.dwUrlPathLength = static_cast<DWORD>(-1);
            components.dwExtraInfoLength = static_cast<DWORD>(-1);
            if (::InternetCrackUrlW(
                wide_url.c_str(),
                static_cast<DWORD>(wide_url.size()),
                0,
                &components) == FALSE)
            {
                return {};
            }

            const bool secure = components.nScheme == INTERNET_SCHEME_HTTPS;
            if (!secure && components.nScheme != INTERNET_SCHEME_HTTP)
            {
                return {};
            }

            const std::wstring host(
                components.lpszHostName,
                static_cast<std::size_t>(components.dwHostNameLength));
            std::wstring object_path;
            if (components.lpszUrlPath != nullptr && components.dwUrlPathLength != 0)
            {
                object_path.assign(
                    components.lpszUrlPath,
                    static_cast<std::size_t>(components.dwUrlPathLength));
            }
            if (components.lpszExtraInfo != nullptr && components.dwExtraInfoLength != 0)
            {
                object_path.append(
                    components.lpszExtraInfo,
                    static_cast<std::size_t>(components.dwExtraInfoLength));
            }
            if (host.empty())
            {
                return {};
            }
            if (object_path.empty())
            {
                object_path = L"/";
            }

            InternetHandle session(::InternetOpenW(
                wide_agent.c_str(),
                INTERNET_OPEN_TYPE_PRECONFIG,
                nullptr,
                nullptr,
                0));
            if (!session)
            {
                return {};
            }

            const DWORD receive_timeout =
                timeOut > static_cast<int>(kMinimumReceiveTimeoutMs)
                ? static_cast<DWORD>(timeOut)
                : kMinimumReceiveTimeoutMs;
            if (!SetTimeout(session.get(), INTERNET_OPTION_CONNECT_TIMEOUT, kConnectTimeoutMs) ||
                !SetTimeout(session.get(), INTERNET_OPTION_SEND_TIMEOUT, kSendTimeoutMs) ||
                !SetTimeout(session.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, receive_timeout))
            {
                return {};
            }

            InternetHandle connection(::InternetConnectW(
                session.get(),
                host.c_str(),
                components.nPort,
                nullptr,
                nullptr,
                INTERNET_SERVICE_HTTP,
                0,
                0));
            if (!connection)
            {
                return {};
            }

            const wchar_t* accept_types[]{ L"*/*", nullptr };
            DWORD request_flags =
                INTERNET_FLAG_RELOAD |
                INTERNET_FLAG_NO_CACHE_WRITE |
                INTERNET_FLAG_KEEP_CONNECTION;
            if (secure)
            {
                request_flags |= INTERNET_FLAG_SECURE;
            }

            InternetHandle request(::HttpOpenRequestW(
                connection.get(),
                method,
                object_path.c_str(),
                nullptr,
                nullptr,
                accept_types,
                request_flags,
                0));
            if (!request)
            {
                return {};
            }

            for (const std::string& header : headers)
            {
                const std::wstring wide_header = ToWide(header);
                if (wide_header.empty() ||
                    ::HttpAddRequestHeadersW(
                        request.get(),
                        wide_header.c_str(),
                        static_cast<DWORD>(wide_header.size()),
                        HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE) == FALSE)
                {
                    return {};
                }
            }

            std::string request_body;
            if (!postData.empty())
            {
                const std::wstring wide_body = ToWide(postData);
                if (wide_body.empty())
                {
                    return {};
                }
                request_body = WideToMultiByte(wide_body, CP_UTF8);
                if (request_body.empty() ||
                    request_body.size() > static_cast<std::size_t>(MAXDWORD))
                {
                    return {};
                }
            }

            if (::HttpSendRequestW(
                request.get(),
                nullptr,
                0,
                request_body.empty() ? nullptr : request_body.data(),
                static_cast<DWORD>(request_body.size())) == FALSE)
            {
                return {};
            }

            DWORD status_code = 0;
            DWORD status_size = sizeof(status_code);
            if (::HttpQueryInfoW(
                request.get(),
                HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                &status_code,
                &status_size,
                nullptr) == FALSE ||
                status_code < 200 ||
                status_code >= 300)
            {
                return {};
            }

            std::string response;
            std::array<char, 4096> buffer{};
            for (;;)
            {
                DWORD bytes_read = 0;
                if (::InternetReadFile(
                    request.get(),
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()),
                    &bytes_read) == FALSE)
                {
                    return {};
                }
                if (bytes_read == 0)
                {
                    break;
                }
                response.append(buffer.data(), static_cast<std::size_t>(bytes_read));
            }

            return Utf8ResponseToLocalCodePage(response);
        }
        catch (...)
        {
            return {};
        }
    }
}

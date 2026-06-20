#include "HttpRequestToolWinInetInternal.h"

#include <array>

namespace Help
{
    namespace http_request_tool_wininet
    {
        namespace
        {
            constexpr DWORD BufferSize = 4 * 1024;
        }

        void ApplyTimeouts(HINTERNET internet, int timeOut)
        {
            DWORD connectRetries = 1000 * 10;
            DWORD connectTimeout = 1000 * 10;
            DWORD sendTimeout = 1000 * 10;
            DWORD receiveTimeout = static_cast<DWORD>(timeOut <= 1000 * 30 ? 1000 * 30 : timeOut);

            InternetSetOptionA(internet, INTERNET_OPTION_CONNECT_RETRIES, &connectRetries, sizeof(connectRetries));
            InternetSetOptionA(internet, INTERNET_OPTION_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));
            InternetSetOptionA(internet, INTERNET_OPTION_SEND_TIMEOUT, &sendTimeout, sizeof(sendTimeout));
            InternetSetOptionA(internet, INTERNET_OPTION_DATA_SEND_TIMEOUT, &sendTimeout, sizeof(sendTimeout));
            InternetSetOptionA(internet, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &receiveTimeout, sizeof(receiveTimeout));
            InternetSetOptionA(internet, INTERNET_OPTION_RECEIVE_TIMEOUT, &receiveTimeout, sizeof(receiveTimeout));
        }

        bool ParseHttpUrl(const string& url, ParsedHttpUrl& parsed)
        {
            std::array<char, 16> scheme = {};
            std::array<char, INTERNET_MAX_HOST_NAME_LENGTH + 1> host = {};
            std::array<char, INTERNET_MAX_PATH_LENGTH + 1> path = {};
            std::array<char, 2048> extraInfo = {};

            URL_COMPONENTSA components = {};
            components.dwStructSize = sizeof(components);
            components.lpszScheme = scheme.data();
            components.dwSchemeLength = static_cast<DWORD>(scheme.size());
            components.lpszHostName = host.data();
            components.dwHostNameLength = static_cast<DWORD>(host.size());
            components.lpszUrlPath = path.data();
            components.dwUrlPathLength = static_cast<DWORD>(path.size());
            components.lpszExtraInfo = extraInfo.data();
            components.dwExtraInfoLength = static_cast<DWORD>(extraInfo.size());

            if (!InternetCrackUrlA(url.c_str(), 0, 0, &components))
            {
                return false;
            }

            if (components.nScheme != INTERNET_SCHEME_HTTP && components.nScheme != INTERNET_SCHEME_HTTPS)
            {
                return false;
            }

            parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
            parsed.port = components.nPort;
            parsed.host.assign(host.data(), components.dwHostNameLength);
            parsed.object.assign(path.data(), components.dwUrlPathLength);
            parsed.object.append(extraInfo.data(), components.dwExtraInfoLength);

            if (parsed.object.empty())
            {
                parsed.object = "/";
            }

            return !parsed.host.empty();
        }

        string ReadInternetResponse(HINTERNET request)
        {
            string response;
            std::array<char, BufferSize + 1> buffer = {};
            DWORD bytesRead = 0;

            while (InternetReadFile(request, buffer.data(), BufferSize, &bytesRead) && bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                response += buffer.data();
                ZeroMemory(buffer.data(), buffer.size());
            }

            return response;
        }
    }
}

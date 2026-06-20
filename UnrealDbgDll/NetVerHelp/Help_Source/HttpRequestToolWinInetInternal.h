#pragma once

#include "../Help_Tou/HttpRequestToolWinInet.h"

namespace Help
{
    namespace http_request_tool_wininet
    {
        constexpr DWORD NormalConnectFlags = INTERNET_FLAG_KEEP_CONNECTION;
        constexpr DWORD SecureConnectFlags = NormalConnectFlags | INTERNET_FLAG_SECURE;
        constexpr DWORD GetRequestFlags = INTERNET_FLAG_TRANSFER_ASCII;
        constexpr DWORD NormalRequestFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE;
        constexpr DWORD SecureRequestFlags = NormalRequestFlags | INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

        struct ParsedHttpUrl
        {
            bool secure;
            INTERNET_PORT port;
            string host;
            string object;
        };

        void ApplyTimeouts(HINTERNET internet, int timeOut);
        bool ParseHttpUrl(const string& url, ParsedHttpUrl& parsed);
        string ReadInternetResponse(HINTERNET request);
    }
}

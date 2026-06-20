#include "../Help_Tou/HttpRequestToolWinInet.h"

#include <vector>

namespace Help
{
    string CHttpRequestTool::ConvertAcpToUtf8(const string& strText)
    {
        int unicodeLength = MultiByteToWideChar(CP_ACP, 0, strText.c_str(), -1, NULL, 0);
        if (unicodeLength <= 0)
        {
            return (const char*)_xor_("");
        }

        std::vector<WCHAR> unicode(static_cast<size_t>(unicodeLength) + 1);
        MultiByteToWideChar(CP_ACP, 0, strText.c_str(), -1, unicode.data(), unicodeLength);
        unicode[unicodeLength] = L'\0';

        int utf8Length = WideCharToMultiByte(CP_UTF8, 0, unicode.data(), -1, NULL, 0, NULL, NULL);
        if (utf8Length <= 0)
        {
            return (const char*)_xor_("");
        }

        std::vector<char> utf8(static_cast<size_t>(utf8Length) + 1);
        WideCharToMultiByte(CP_UTF8, 0, unicode.data(), -1, utf8.data(), utf8Length, NULL, NULL);
        utf8[utf8Length] = '\0';

        return string(utf8.data());
    }

    string CHttpRequestTool::ConvertUtf8ToAcp(const string& strText)
    {
        int unicodeLength = MultiByteToWideChar(CP_UTF8, 0, strText.c_str(), -1, NULL, 0);
        if (unicodeLength <= 0)
        {
            return (const char*)_xor_("");
        }

        std::vector<WCHAR> unicode(static_cast<size_t>(unicodeLength) + 1);
        MultiByteToWideChar(CP_UTF8, 0, strText.c_str(), -1, unicode.data(), unicodeLength);
        unicode[unicodeLength] = L'\0';

        int acpLength = WideCharToMultiByte(CP_ACP, 0, unicode.data(), -1, NULL, 0, NULL, NULL);
        if (acpLength <= 0)
        {
            return (const char*)_xor_("");
        }

        std::vector<char> acp(static_cast<size_t>(acpLength) + 1);
        WideCharToMultiByte(CP_ACP, 0, unicode.data(), -1, acp.data(), acpLength, NULL, NULL);
        acp[acpLength] = '\0';

        return string(acp.data());
    }
}

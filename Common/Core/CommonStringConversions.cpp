#include "CommonInternal.h"

namespace Common
{
    //string转wstring
    std::wstring stringToWideString(const std::string& narrowStr)
    {
        // 获取宽字符字符串的长度（包括空终止符）
        int wideStrLength = MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, nullptr, 0);

        // 分配内存来存储宽字符字符串
        wchar_t* wideStr = new wchar_t[wideStrLength];

        // 将窄字符转换为宽字符
        MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, wideStr, wideStrLength);

        // 创建 std::wstring 对象
        std::wstring result(wideStr);

        // 释放内存
        delete[] wideStr;

        return result;
    }

    //wstring转string
    //注意: 在Windows下将utf16转utf8的std::string是无法正常显示中文的
    std::string wideStringToString(const std::wstring& wideStr)
    {
        int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string str(bufferSize - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &str[0], bufferSize - 1, nullptr, nullptr);
        return str;
    }

    //wstring转本地string
    //注意: 本地ansi可以显示中文，但请不要再网络内容传输中使用它，因为不同计算机本地代码页不相同.
    std::string wideStringToString2(const std::wstring& wideStr)
    {
        int bufferSize = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string str(bufferSize - 1, 0);
        WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &str[0], bufferSize - 1, nullptr, nullptr);
        return str;
    }

    //wchar_t*转string
    std::string wcharToString(const wchar_t* str)
    {
        if (str == nullptr)
        {
            return {};
        }

        const int bufferSize = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
        if (bufferSize <= 0)
        {
            return {};
        }

        std::string result(bufferSize, '\0');
        WideCharToMultiByte(CP_UTF8, 0, str, -1, &result[0], bufferSize, nullptr, nullptr);
        if (!result.empty() && result.back() == '\0')
        {
            result.pop_back();
        }
        return result;
    }

    //wchar_t*转wstring
    std::wstring wcharToWideString(const wchar_t* wcharStr)
    {
        // 使用构造函数将 wchar_t* 转换为 std::wstring
        std::wstring wideStr(wcharStr);

        return wideStr;
    }

    //char*转wchar_t*
    std::wstring ConvertCharToWchar(const char* charStr)
    {
        if (charStr == nullptr)
        {
            return {};
        }

        const size_t charStrLength = strlen(charStr) + 1; // char 字符串的长度（包括 null 终止符）
        if (charStrLength > INT_MAX)
        {
            return {};
        }
        const int charStrLengthInt = static_cast<int>(charStrLength);

        // 计算 wchar_t 字符串所需的缓冲区大小
        const int wcharStrSize = MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLengthInt, nullptr, 0);
        if (wcharStrSize <= 0)
        {
            return {};
        }

        // 分配 wchar_t 缓冲区
        std::wstring result(wcharStrSize, L'\0');

        // 执行转换
        MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLengthInt, &result[0], wcharStrSize);
        if (!result.empty() && result.back() == L'\0')
        {
            result.pop_back();
        }

        return result;
    }

    // 将 utf8 编码的字符串转换为 Unicode 编码
    std::wstring utf8ToUnicode(const std::string& utf8String)
    {
        int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
        std::wstring unicodeString(bufferSize, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &unicodeString[0], bufferSize);
        return unicodeString;
    }

    //Unicode转Utf8
    std::string UnicodeToUtf8(const std::wstring& unicodeString)
    {
        int utf8Length = WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Length == 0)
        {
            // 转换失败
            // 转换失败，可以根据实际情况进行错误处理
            return "";
        }

        std::string utf8String(utf8Length, '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) == 0)
        {
            // 转换失败
            // 转换失败，可以根据实际情况进行错误处理
            return "";
        }

        return utf8String;
    }
}

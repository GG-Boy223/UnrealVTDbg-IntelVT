#include "CommonInternal.h"

namespace Common
{
    //gbk转utf8
    std::string GbkToUTF8(const std::string& gbkString)
    {
        int bufferSize = MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, nullptr, 0);
        std::wstring wideString(bufferSize - 1, L'\0');
        MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, &wideString[0], bufferSize - 1);

        bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8String(bufferSize - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], bufferSize - 1, nullptr, nullptr);

        return utf8String;
    }

    // 将 utf8 编码的字符串转换为 GBK 编码
    std::string utf8ToGbk(const std::string& utf8String)
    {
        int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
        if (bufferSize == 0)
        {
            // 转换失败，可以根据实际情况进行错误处理
            return "";
        }

        std::wstring wideString(bufferSize, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &wideString[0], bufferSize);

        bufferSize = WideCharToMultiByte(CP_ACP, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (bufferSize == 0)
        {
            // 转换失败，可以根据实际情况进行错误处理
            return "";
        }

        std::string gbkString(bufferSize, '\0');
        WideCharToMultiByte(CP_ACP, 0, wideString.c_str(), -1, &gbkString[0], bufferSize, nullptr, nullptr);

        return gbkString;
    }

    //本地代码页转std::wstring
    std::wstring ConvertLocalCodePageToWideString(const std::string& str)
    {
        int wideStrLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
        if (wideStrLen == 0)
        {
            // 转换失败，可以根据实际情况处理错误
            return L"";
        }

        std::wstring wideStr(wideStrLen, L'\0');
        if (MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wideStr[0], wideStrLen) == 0)
        {
            // 转换失败，可以根据实际情况处理错误
            return L"";
        }

        // 去掉末尾的空字符
        wideStr.resize(wideStrLen - 1);

        return wideStr;
    }

    //本地代码页转std::string
    std::string LocalCodePageToUtf8(const std::string& localString)
    {
        int wideCharLength = MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, nullptr, 0);
        if (wideCharLength == 0)
        {
            // 转换失败
            // 转换失败，可以根据实际情况进行错误处理
            return "";
        }

        std::wstring wideString(wideCharLength, L'\0');
        if (MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, &wideString[0], wideCharLength) == 0)
        {
            // 转换失败
            // 转换失败，可以根据实际情况进行错误处理
            return "";
        }

        int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Length == 0)
        {
            // 转换失败
            // 转换失败，可以根据实际情况进行错误处理
            return "";
        }

        std::string utf8String(utf8Length, '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) == 0)
        {
            // 转换失败
            // 转换失败，可以根据实际情况进行错误处理
            return "";
        }

        return utf8String;
    }
}

#include "SymbolsInternal.h"

namespace
{
    std::wstring Trim(const std::wstring& value)
    {
        const wchar_t* whitespace = L" \t\r\n";
        const size_t first = value.find_first_not_of(whitespace);
        if (first == std::wstring::npos)
        {
            // 转换失败，可以根据实际情况处理错误
            return L"";
        }

        const size_t last = value.find_last_not_of(whitespace);
        return value.substr(first, last - first + 1);
    }
}

std::string BuildSymbolServerBaseUrl(const std::wstring& configPath)
{
    std::wstring selectedServer = Trim(FileSystem::ReadIniValue(configPath, L"符号", L"服务器"));
    if (selectedServer.empty())
    {
        selectedServer = Trim(FileSystem::ReadIniValue(configPath, L"Symbol", L"Server"));
    }

    int selectSvr = 100; //默认选择镜像服务器
    if (!selectedServer.empty())
    {
        try
        {
            selectSvr = std::stoi(selectedServer);
        }
        catch (const std::exception&)
        {
            selectSvr = 100;
        }
    }

    std::wstring customServer = Trim(FileSystem::ReadIniValue(configPath, L"符号", L"自定义服务器"));
    if (customServer.empty())
    {
        customServer = Trim(FileSystem::ReadIniValue(configPath, L"符号", L"服务器地址"));
    }
    if (customServer.empty())
    {
        customServer = Trim(FileSystem::ReadIniValue(configPath, L"Symbol", L"CustomServer"));
    }
    if (customServer.empty())
    {
        customServer = Trim(FileSystem::ReadIniValue(configPath, L"Symbol", L"ServerAddress"));
    }

    std::wstring baseUrl;
    if (selectSvr == 300 && !customServer.empty())
    {
        baseUrl = customServer;
    }
    else if (selectSvr == 200)
    {
        baseUrl = L"https://msdl.microsoft.com/download/symbols/";
    }
    else
    {
        baseUrl = L"http://msdl.blackint3.com:88/download/symbols/";
    }

    std::replace(baseUrl.begin(), baseUrl.end(), L'\\', L'/');
    if (!baseUrl.empty() && baseUrl.back() != L'/')
    {
        baseUrl += L'/';
    }

    return Common::wideStringToString(baseUrl);
}

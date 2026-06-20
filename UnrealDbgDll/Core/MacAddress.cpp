#include "DllMainInternal.h"

#pragma comment(lib, "iphlpapi.lib")

#ifdef 开启验证
std::string FormatMacAddress(const BYTE* address, ULONG length)
{
    if (address == nullptr || length != 6)
    {
        return {};
    }

    char mac[13] = {};
    sprintf_s(mac, _countof(mac), "%02X%02X%02X%02X%02X%02X",
        address[0], address[1], address[2], address[3], address[4], address[5]);
    return mac;
}

std::wstring FormatMacAddressForLog(const std::string& mac)
{
    if (mac.length() != 12)
    {
        return Common::ConvertLocalCodePageToWideString(mac);
    }

    std::wstring formattedMac;
    formattedMac.reserve(17);
    for (size_t index = 0; index < mac.length(); ++index)
    {
        if (index > 0 && index % 2 == 0)
        {
            formattedMac.push_back(L'-');
        }
        formattedMac.push_back(static_cast<wchar_t>(mac[index]));
    }

    return formattedMac;
}

void LogMultilineLocalText(const std::string& text, const wchar_t* firstLinePrefix, const wchar_t* nextLinePrefix)
{
    std::wstring wideText = Common::ConvertLocalCodePageToWideString(text);
    if (wideText.empty())
    {
        if (firstLinePrefix != nullptr && firstLinePrefix[0] != L'\0')
        {
            logger.outDebug(_xor_(L"%s"), firstLinePrefix);
        }
        return;
    }

    size_t lineStart = 0;
    bool isFirstLine = true;
    while (lineStart < wideText.length())
    {
        const size_t lineEnd = wideText.find(L'\n', lineStart);
        std::wstring line = wideText.substr(lineStart, lineEnd == std::wstring::npos ? std::wstring::npos : lineEnd - lineStart);
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }

        std::wstring output;
        const wchar_t* prefix = isFirstLine ? firstLinePrefix : nextLinePrefix;
        if (prefix != nullptr)
        {
            output += prefix;
        }
        output += line;
        logger.outDebug(_xor_(L"%s"), output.c_str());

        if (lineEnd == std::wstring::npos)
        {
            break;
        }

        lineStart = lineEnd + 1;
        isFirstLine = false;
    }
}

static bool IsUsableMacAdapter(const IP_ADAPTER_ADDRESSES* adapter)
{
    if (adapter == nullptr)
    {
        return false;
    }

    if (adapter->OperStatus != IfOperStatusUp ||
        adapter->PhysicalAddressLength != 6 ||
        adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
        adapter->IfType == IF_TYPE_TUNNEL)
    {
        return false;
    }

    const BYTE zeroMac[6] = {};
    return memcmp(adapter->PhysicalAddress, zeroMac, sizeof(zeroMac)) != 0;
}

static int GetMacAdapterPriority(const IP_ADAPTER_ADDRESSES* adapter)
{
    if (adapter == nullptr)
    {
        return 3;
    }

    switch (adapter->IfType)
    {
    case IF_TYPE_IEEE80211:
        return 0;
    case IF_TYPE_ETHERNET_CSMACD:
        return 1;
    default:
        return 2;
    }
}

static std::wstring GetMacAdapterTypeName(const IP_ADAPTER_ADDRESSES* adapter)
{
    if (adapter == nullptr)
    {
        return L"Unknown";
    }

    switch (adapter->IfType)
    {
    case IF_TYPE_IEEE80211:
        return L"Wi-Fi";
    case IF_TYPE_ETHERNET_CSMACD:
        return L"Ethernet";
    default:
        return L"Other";
    }
}

std::string GetMac()
{
    ULONG bufferLength = 15 * 1024;
    constexpr ULONG flags =
        GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER |
        GAA_FLAG_SKIP_FRIENDLY_NAME;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        std::vector<BYTE> buffer(bufferLength);
        auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        const DWORD status = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &bufferLength);

        if (status == ERROR_BUFFER_OVERFLOW)
        {
            continue;
        }

        if (status != NO_ERROR)
        {
            logger.outDebug(_xor_(L"获取机器码失败，GetAdaptersAddresses error:%lu"), status);
            return {};
        }

        const IP_ADAPTER_ADDRESSES* selectedAdapter = nullptr;
        int selectedPriority = 3;

        for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next)
        {
            if (IsUsableMacAdapter(adapter))
            {
                const int priority = GetMacAdapterPriority(adapter);
                if (priority < selectedPriority)
                {
                    selectedAdapter = adapter;
                    selectedPriority = priority;
                    if (selectedPriority == 0)
                    {
                        break;
                    }
                }
            }
        }

        if (selectedAdapter != nullptr)
        {
            macAdapterType = GetMacAdapterTypeName(selectedAdapter);
            macAdapterName = selectedAdapter->FriendlyName != nullptr ? selectedAdapter->FriendlyName : L"";
            return FormatMacAddress(selectedAdapter->PhysicalAddress, selectedAdapter->PhysicalAddressLength);
        }

        logger.outDebug(_xor_(L"获取机器码失败，没有可用的物理网卡。"));
        return {};
    }

    logger.outDebug(_xor_(L"获取机器码失败，网卡列表缓冲区持续不足。"));
    return {};
}

bool EnsureMacCode()
{
    if (!maccode.empty())
    {
        return true;
    }

    maccode = GetMac();
    if (maccode.empty())
    {
        logger.outDebug(_xor_(L"初始化失败,MAC拿不到"));
        return false;
    }

    if (!macAdapterName.empty())
    {
        logger.outDebug(_xor_(L"MAC(%s, %s) : %s"), macAdapterType.c_str(), macAdapterName.c_str(), FormatMacAddressForLog(maccode).c_str());
    }
    else
    {
        logger.outDebug(_xor_(L"MAC(%s) : %s"), macAdapterType.c_str(), FormatMacAddressForLog(maccode).c_str());
    }
    return true;
}
#endif

#include "SymbolPreparer.h"
#include "Bridge.h"

#include <DbgHelp.h>
#include <Objbase.h>
#include <Psapi.h>
#include <WinHttp.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "Winhttp.lib")

namespace
{
    using unrealdbg_native::LogLevel;
    using unrealdbg_native::SymbolModuleState;
    using unrealdbg_native::SymbolModuleStatus;
    using unrealdbg_native::SymbolPreparationState;

    constexpr DWORD kRsdsSignature = 0x53445352; // RSDS
    constexpr size_t kMaximumPeBytes = 512u * 1024u * 1024u;
    constexpr size_t kMaximumPdbBytes = 1024u * 1024u * 1024u;
    constexpr wchar_t kSymbolCacheDirectory[] = L"C:\\Symbols";
    std::once_flag g_dbgHelpInitialization;
    std::mutex g_dbgHelpMutex;
    bool g_dbgHelpReady{};

    bool InitializeDbgHelp() noexcept
    {
        std::call_once(g_dbgHelpInitialization, []()
        {
            ::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS |
                SYMOPT_LOAD_ANYTHING);
            g_dbgHelpReady = ::SymInitializeW(::GetCurrentProcess(),
                kSymbolCacheDirectory, FALSE) != FALSE;
        });
        return g_dbgHelpReady;
    }

    bool LocateLocalPdb(const GUID& guid, const DWORD age,
        const std::wstring& fileName, std::wstring& foundPath)
    {
        if (!InitializeDbgHelp() || fileName.empty())
        {
            return false;
        }

        std::lock_guard lock(g_dbgHelpMutex);
        wchar_t found[MAX_PATH + 1]{};
        GUID searchGuid = guid;
        const BOOL located = ::SymFindFileInPathW(
            ::GetCurrentProcess(), kSymbolCacheDirectory,
            fileName.c_str(), &searchGuid, age, 0,
            SSRVOPT_GUIDPTR, found, nullptr, nullptr);
        if (!located || found[0] == L'\0')
        {
            return false;
        }
        foundPath = found;
        return true;
    }

    struct RsdsHeader
    {
        DWORD signature;
        GUID guid;
        DWORD age;
    };

    struct PDBHeader7
    {
        char signature[0x20];
        int pageSize;
        int allocationTablePointer;
        int filePageCount;
        int rootStreamSize;
        int reserved;
        int rootStreamPageNumberListNumber;
    };

    struct RootStream7
    {
        int streamCount;
        int streamSizes[1];
    };

    struct GuidStreamData
    {
        int version;
        int date;
        int age;
        GUID guid;
    };

    std::wstring PathInDirectory(const std::wstring& directory,
        const std::wstring& relative)
    {
        return (std::filesystem::path(directory) / relative).wstring();
    }

    std::wstring Trim(std::wstring value)
    {
        const size_t first = value.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos)
        {
            return {};
        }
        const size_t last = value.find_last_not_of(L" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    std::wstring ReadIniString(const std::wstring& path,
        const wchar_t* section, const wchar_t* key)
    {
        std::vector<wchar_t> buffer(512);
        for (;;)
        {
            const DWORD copied = ::GetPrivateProfileStringW(section, key, L"",
                buffer.data(), static_cast<DWORD>(buffer.size()), path.c_str());
            if (copied < buffer.size() - 1)
            {
                return std::wstring(buffer.data(), copied);
            }
            if (buffer.size() >= 32768)
            {
                return {};
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    std::wstring SystemDirectory()
    {
        std::vector<wchar_t> buffer(MAX_PATH);
        for (;;)
        {
            const UINT length = ::GetSystemDirectoryW(
                buffer.data(), static_cast<UINT>(buffer.size()));
            if (length == 0)
            {
                return L"C:\\Windows\\System32";
            }
            if (length < buffer.size() - 1)
            {
                return std::wstring(buffer.data(), length);
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    bool ReadFileBytes(const std::wstring& path, size_t maximumBytes,
        std::vector<std::uint8_t>& bytes)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return false;
        }

        const std::streamoff size = file.tellg();
        if (size <= 0 || static_cast<std::uint64_t>(size) > maximumBytes)
        {
            return false;
        }

        bytes.resize(static_cast<size_t>(size));
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return file.gcount() == static_cast<std::streamsize>(bytes.size());
    }

    bool RangeWithin(const size_t total, const size_t offset,
        const size_t length) noexcept
    {
        return offset <= total && length <= total - offset;
    }

    bool IsValidPdbFileName(const std::string& name)
    {
        if (name.empty() || name.size() >= MAX_PATH)
        {
            return false;
        }
        for (const unsigned char value : name)
        {
            if (value < 0x21 || value > 0x7E ||
                value == '\\' || value == '/' || value == ':' ||
                value == '*' || value == '?' || value == '"' ||
                value == '<' || value == '>' || value == '|')
            {
                return false;
            }
        }
        return true;
    }

    bool RvaToFileOffset(const std::vector<std::uint8_t>& bytes,
        const IMAGE_SECTION_HEADER* sections, const WORD sectionCount,
        const DWORD rva, size_t& offset)
    {
        if (rva == 0 || sections == nullptr)
        {
            return false;
        }
        for (WORD index = 0; index < sectionCount; ++index)
        {
            const IMAGE_SECTION_HEADER& section = sections[index];
            if (rva < section.VirtualAddress ||
                rva - section.VirtualAddress >= section.SizeOfRawData)
            {
                continue;
            }
            const size_t delta = rva - section.VirtualAddress;
            const size_t raw = section.PointerToRawData;
            if (!RangeWithin(bytes.size(), raw, section.SizeOfRawData) ||
                delta >= section.SizeOfRawData)
            {
                return false;
            }
            offset = raw + delta;
            return true;
        }
        return false;
    }

    std::wstring GuidKey(const GUID& guid, const DWORD age)
    {
        wchar_t text[64]{};
        if (::StringFromGUID2(guid, text, static_cast<int>(std::size(text))) == 0)
        {
            return {};
        }
        std::wstring compact;
        for (const wchar_t value : text)
        {
            if ((value >= L'0' && value <= L'9') ||
                (value >= L'a' && value <= L'f') ||
                (value >= L'A' && value <= L'F'))
            {
                compact.push_back(value);
            }
        }
        return compact + std::to_wstring(age);
    }

    std::wstring ServerUrlFromConfig(const std::wstring& configPath)
    {
        std::wstring selected = Trim(ReadIniString(
            configPath, L"符号", L"服务器"));
        if (selected.empty())
        {
            selected = Trim(ReadIniString(
                configPath, L"Symbol", L"Server"));
        }

        int selectedServer = 100;
        if (!selected.empty())
        {
            try
            {
                selectedServer = std::stoi(selected);
            }
            catch (...)
            {
                selectedServer = 100;
            }
        }

        std::wstring custom;
        for (const auto& [section, key] : {
            std::pair{ L"符号", L"自定义服务器" },
            std::pair{ L"符号", L"服务器地址" },
            std::pair{ L"Symbol", L"CustomServer" },
            std::pair{ L"Symbol", L"ServerAddress" } })
        {
            custom = Trim(ReadIniString(configPath, section, key));
            if (!custom.empty())
            {
                break;
            }
        }

        std::wstring baseUrl;
        if (selectedServer == 300 && !custom.empty())
        {
            baseUrl = custom;
        }
        else if (selectedServer == 200)
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
            baseUrl.push_back(L'/');
        }
        return baseUrl;
    }

    bool PagePointer(const std::vector<std::uint8_t>& data,
        const size_t pageSize, const int pageNumber, const char*& pointer)
    {
        if (pageNumber < 0 || pageSize == 0)
        {
            return false;
        }
        const size_t offset = static_cast<size_t>(pageNumber) * pageSize;
        if (!RangeWithin(data.size(), offset, pageSize))
        {
            return false;
        }
        pointer = reinterpret_cast<const char*>(data.data() + offset);
        return true;
    }

    size_t PageCountForStream(const int streamSize, const size_t pageSize)
    {
        if (streamSize <= 0 || streamSize == 0xFFFFFFFF)
        {
            return 0;
        }
        return (static_cast<size_t>(streamSize) + pageSize - 1) / pageSize;
    }

    bool VerifyPdbFile(const std::wstring& pdbPath,
        const GUID& expectedGuid, const DWORD expectedAge)
    {
        std::vector<std::uint8_t> data;
        if (!ReadFileBytes(pdbPath, kMaximumPdbBytes, data) ||
            data.size() < sizeof(PDBHeader7))
        {
            return false;
        }

        const auto* header = reinterpret_cast<const PDBHeader7*>(data.data());
        if (memcmp(header->signature,
            "Microsoft C/C++ MSF 7.00\r\n\x1A" "DS\0\0\0",
            sizeof(header->signature)) != 0)
        {
            return false;
        }
        if (header->pageSize <= 0 || header->filePageCount < 0 ||
            header->rootStreamPageNumberListNumber < 0)
        {
            return false;
        }

        const size_t pageSize = static_cast<size_t>(header->pageSize);
        if (pageSize > data.size() ||
            static_cast<size_t>(header->filePageCount) > data.size() / pageSize)
        {
            return false;
        }

        const char* rootPageList = nullptr;
        if (!PagePointer(data, pageSize,
            header->rootStreamPageNumberListNumber, rootPageList))
        {
            return false;
        }
        const size_t rootPageListBytes =
            data.size() - static_cast<size_t>(
                reinterpret_cast<const std::uint8_t*>(rootPageList) - data.data());
        if (rootPageListBytes < sizeof(int) ||
            header->rootStreamSize <= 0 ||
            static_cast<size_t>(header->rootStreamSize) > data.size())
        {
            return false;
        }

        const size_t rootStreamSize =
            static_cast<size_t>(header->rootStreamSize);
        const size_t rootPageCount =
            (rootStreamSize + pageSize - 1) / pageSize;
        if (rootPageCount > rootPageListBytes / sizeof(int))
        {
            return false;
        }

        std::vector<std::uint8_t> rootData(rootStreamSize);
        for (size_t pageIndex = 0; pageIndex < rootPageCount; ++pageIndex)
        {
            int rootPageNumber{};
            memcpy(&rootPageNumber,
                rootPageList + pageIndex * sizeof(int), sizeof(int));
            const char* rootPage = nullptr;
            if (!PagePointer(data, pageSize, rootPageNumber, rootPage))
            {
                return false;
            }
            const size_t copied = (std::min)(
                pageSize, rootStreamSize - pageIndex * pageSize);
            memcpy(rootData.data() + pageIndex * pageSize, rootPage, copied);
        }

        if (rootStreamSize < sizeof(RootStream7))
        {
            return false;
        }

        const auto* root =
            reinterpret_cast<const RootStream7*>(rootData.data());
        if (root->streamCount <= 0)
        {
            return false;
        }
        const size_t streamCount = static_cast<size_t>(root->streamCount);
        if (streamCount > rootStreamSize / sizeof(int) ||
            streamCount < 4)
        {
            return false;
        }

        std::vector<int> streamSizes(streamCount);
        for (size_t index = 0; index < streamCount; ++index)
        {
            memcpy(&streamSizes[index], root->streamSizes + index, sizeof(int));
        }

        const size_t pageTableStart = streamCount;
        size_t pageTableEntries{};
        for (const int streamSize : streamSizes)
        {
            pageTableEntries += PageCountForStream(streamSize, pageSize);
        }
        if (!RangeWithin(rootStreamSize / sizeof(int),
            pageTableStart, pageTableEntries))
        {
            return false;
        }

        const auto streamPage = [&](const size_t streamIndex,
            const size_t pageIndex) -> int
        {
            size_t tableIndex = pageTableStart;
            for (size_t index = 0; index < streamIndex; ++index)
            {
                tableIndex += PageCountForStream(streamSizes[index], pageSize);
            }
            tableIndex += pageIndex;
            int pageNumber{};
            memcpy(&pageNumber,
                rootData.data() + tableIndex * sizeof(int), sizeof(int));
            return pageNumber;
        };

        const int guidPage = streamPage(1, 0);
        const char* guidData = nullptr;
        if (!PagePointer(data, pageSize, guidPage, guidData) ||
            streamSizes[1] < static_cast<int>(sizeof(GuidStreamData)) ||
            !RangeWithin(data.size(),
                static_cast<size_t>(guidData - reinterpret_cast<const char*>(data.data())),
                static_cast<size_t>(streamSizes[1])))
        {
            return false;
        }
        const auto* guidStream = reinterpret_cast<const GuidStreamData*>(guidData);
        if (memcmp(&guidStream->guid, &expectedGuid, sizeof(GUID)) != 0)
        {
            return false;
        }

        const int dbiPage = streamPage(3, 0);
        const char* dbiData = nullptr;
        constexpr size_t kDbiAgeOffset = sizeof(int) * 2;
        if (!PagePointer(data, pageSize, dbiPage, dbiData) ||
            streamSizes[3] < static_cast<int>(kDbiAgeOffset + sizeof(int)))
        {
            return false;
        }
        int dbiAge{};
        memcpy(&dbiAge, dbiData + kDbiAgeOffset, sizeof(dbiAge));
        return dbiAge >= 0 && static_cast<DWORD>(dbiAge) == expectedAge;
    }

    struct InternetHandle
    {
        HINTERNET value{};

        ~InternetHandle()
        {
            if (value != nullptr)
            {
                ::WinHttpCloseHandle(value);
            }
        }

        InternetHandle(const InternetHandle&) = delete;
        InternetHandle& operator=(const InternetHandle&) = delete;

        InternetHandle() = default;
        explicit InternetHandle(HINTERNET handle) : value(handle)
        {
        }

        HINTERNET get() const noexcept
        {
            return value;
        }

        HINTERNET* put() noexcept
        {
            return &value;
        }
    };

    std::wstring HttpFailure(const wchar_t* operation)
    {
        return std::wstring(operation) + L"失败：" +
            unrealdbg_native::GetSystemErrorMessage(::GetLastError());
    }

    bool DownloadUrlToFile(const std::wstring& url,
        const std::wstring& targetPath,
        const std::function<void(std::uint64_t, std::uint64_t)>& progress,
        const std::function<bool()>& canceled,
        std::wstring& error)
    {
        URL_COMPONENTS components{};
        components.dwStructSize = sizeof(components);
        components.dwHostNameLength = static_cast<DWORD>(-1);
        components.dwUrlPathLength = static_cast<DWORD>(-1);
        components.dwExtraInfoLength = static_cast<DWORD>(-1);
        if (!::WinHttpCrackUrl(url.c_str(), 0, 0, &components))
        {
            error = HttpFailure(L"解析符号服务器 URL ");
            return false;
        }

        const std::wstring host(components.lpszHostName,
            components.dwHostNameLength);
        std::wstring resource(components.lpszUrlPath,
            components.dwUrlPathLength);
        if (components.dwExtraInfoLength != 0)
        {
            resource.append(components.lpszExtraInfo,
                components.dwExtraInfoLength);
        }

        const bool secure =
            components.nScheme == INTERNET_SCHEME_HTTPS;
        const INTERNET_PORT port = components.nPort != 0
            ? components.nPort : (secure ? 443 : 80);

        InternetHandle session(::WinHttpOpen(
            L"F3-Symbol-Downloader/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (session.get() == nullptr)
        {
            error = HttpFailure(L"打开 WinHTTP 会话");
            return false;
        }
        ::WinHttpSetTimeouts(session.get(), 15000, 15000, 30000, 60000);

        InternetHandle connection(::WinHttpConnect(
            session.get(), host.c_str(), port, 0));
        if (connection.get() == nullptr)
        {
            error = HttpFailure(L"连接符号服务器");
            return false;
        }

        InternetHandle request(::WinHttpOpenRequest(
            connection.get(), L"GET", resource.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            secure ? WINHTTP_FLAG_SECURE : 0));
        if (request.get() == nullptr)
        {
            error = HttpFailure(L"创建符号下载请求");
            return false;
        }

        DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        (void)::WinHttpSetOption(request.get(),
            WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy,
            sizeof(redirectPolicy));
        const wchar_t headers[] =
            L"Accept: application/octet-stream\r\n"
            L"User-Agent: F3/1.0\r\n";
        (void)::WinHttpAddRequestHeaders(request.get(), headers,
            static_cast<DWORD>(-1),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

        if (!::WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS,
            0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        {
            error = HttpFailure(L"发送符号下载请求");
            return false;
        }
        if (!::WinHttpReceiveResponse(request.get(), nullptr))
        {
            error = HttpFailure(L"接收符号服务器响应");
            return false;
        }

        DWORD statusCode{};
        DWORD statusSize = sizeof(statusCode);
        if (!::WinHttpQueryHeaders(request.get(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX))
        {
            error = HttpFailure(L"读取 HTTP 状态码");
            return false;
        }
        if (statusCode != 200)
        {
            error = L"符号服务器返回 HTTP " + std::to_wstring(statusCode);
            return false;
        }

        std::uint64_t totalBytes{};
        DWORD lengthSize = sizeof(totalBytes);
        (void)::WinHttpQueryHeaders(request.get(),
            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &totalBytes, &lengthSize,
            WINHTTP_NO_HEADER_INDEX);
        if (progress)
        {
            progress(0, totalBytes);
        }

        const HANDLE file = ::CreateFileW(targetPath.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            error = HttpFailure(L"创建符号文件");
            return false;
        }

        std::array<BYTE, 64 * 1024> buffer{};
        std::uint64_t downloaded{};
        bool ok = true;
        for (;;)
        {
            if (canceled && canceled())
            {
                error = L"符号下载已取消";
                ok = false;
                break;
            }
            DWORD available{};
            if (!::WinHttpQueryDataAvailable(request.get(), &available))
            {
                error = HttpFailure(L"查询符号下载数据");
                ok = false;
                break;
            }
            if (available == 0)
            {
                break;
            }

            DWORD bytesRead{};
            const DWORD toRead = (std::min)(
                available, static_cast<DWORD>(buffer.size()));
            if (!::WinHttpReadData(request.get(), buffer.data(),
                toRead, &bytesRead) || bytesRead == 0)
            {
                error = HttpFailure(L"读取符号下载数据");
                ok = false;
                break;
            }

            DWORD written{};
            if (!::WriteFile(file, buffer.data(), bytesRead,
                &written, nullptr) || written != bytesRead)
            {
                error = HttpFailure(L"写入符号文件");
                ok = false;
                break;
            }

            downloaded += bytesRead;
            if (progress)
            {
                progress(downloaded, totalBytes);
            }
        }

        if (ok && totalBytes != 0 && downloaded != totalBytes)
        {
            error = L"符号文件下载不完整";
            ok = false;
        }
        if (ok && !::FlushFileBuffers(file))
        {
            error = HttpFailure(L"刷新符号文件");
            ok = false;
        }
        ::CloseHandle(file);
        if (!ok)
        {
            (void)::DeleteFileW(targetPath.c_str());
        }
        return ok;
    }
}

namespace
{
    std::wstring HexOffset(const std::uint64_t value)
    {
        wchar_t buffer[24]{};
        swprintf_s(buffer, L"0x%llX",
            static_cast<unsigned long long>(value));
        return buffer;
    }

    struct SymbolUsageDescriptor
    {
        const wchar_t* name;
        const wchar_t* usage;
        bool function;
    };

    constexpr SymbolUsageDescriptor kUsedSymbols[] = {
        { L"PspLoaderInitRoutine", L"系统 DLL 加载入口", false },
        { L"DbgkDebugObjectType", L"调试对象类型", false },
        { L"ObTypeIndexTable", L"对象类型索引表", false },
        { L"PspNotifyEnableMask", L"进程通知掩码", false },
        { L"PerfGlobalGroupMask", L"全局性能分组掩码", false },
        { L"PspActiveProcessLock", L"活动进程锁", false },
        { L"PspProcessSequenceNumber", L"进程序列号", false },
        { L"PsActiveProcessHead", L"活动进程链表头", false },
        { L"PsFreezeProcess", L"冻结进程", true },
        { L"PsThawProcess", L"解冻进程", true },
        { L"NtCreateDebugObject", L"创建虚拟调试对象", true },
        { L"DbgkpSuspendProcess", L"挂起调试目标进程", true },
        { L"DbgkCreateThread", L"线程创建调试事件", true },
        { L"DbgkpQueueMessage", L"投递调试消息", true },
        { L"DbgkpSectionToFileHandle", L"节对象转文件句柄", true },
        { L"DbgkpSendApiMessage", L"发送调试 API 消息", true },
        { L"DbgkMapViewOfSection", L"DLL 加载调试事件", true },
        { L"DbgkUnMapViewOfSection", L"DLL 卸载调试事件", true },
        { L"DbgkpSuppressDbgMsg", L"抑制调试消息", true },
        { L"DbgkExitThread", L"线程退出调试事件", true },
        { L"PsSetProcessFaultInformation", L"设置进程异常信息", true },
        { L"PsCaptureExceptionPort", L"捕获异常端口", true },
        { L"DbgkpSendApiMessageLpc", L"LPC 调试消息", true },
        { L"DbgkpSendErrorMessage", L"发送错误消息", true },
        { L"DbgkForwardException", L"转发调试异常", true },
        { L"DbgkpPostFakeProcessCreateMessages", L"伪造进程创建消息", true },
        { L"NtDebugActiveProcess", L"附加调试目标", true },
        { L"DbgkExitProcess", L"进程退出调试事件", true },
        { L"PspExitThread", L"线程退出清理", true },
        { L"DbgkpWakeTarget", L"唤醒调试目标", true },
        { L"NtDebugContinue", L"继续调试事件", true },
        { L"NtWaitForDebugEvent", L"等待调试事件", true },
        { L"ObDuplicateObject", L"复制对象句柄", true },
        { L"DbgkClearProcessDebugObject", L"清除进程调试对象", true },
        { L"NtRemoveProcessDebug", L"移除进程调试", true },
        { L"PsGetNextProcess", L"枚举下一个进程", true },
        { L"DbgkpMarkProcessPeb", L"标记进程 PEB", true },
        { L"PsTerminateProcess", L"终止进程", true },
        { L"ObCreateObjectType", L"创建对象类型", true },
        { L"PsGetNextProcessThread", L"枚举进程线程", true },
        { L"DbgkpPostFakeThreadMessages", L"伪造线程消息", true },
        { L"KiStackAttachProcess", L"附加进程地址空间", true },
        { L"KiUnstackDetachProcess", L"分离进程地址空间", true },
        { L"NtReadVirtualMemory", L"读取目标虚拟内存", true },
        { L"NtWriteVirtualMemory", L"写入目标虚拟内存", true },
        { L"ZwProtectVirtualMemory", L"修改虚拟内存保护", true },
        { L"NtProtectVirtualMemory", L"修改虚拟内存保护", true },
        { L"PspCreateThread", L"创建进程线程", true },
        { L"NtCreateThreadEx", L"创建远程线程", true },
        { L"NtOpenProcess", L"打开目标进程", true },
        { L"DbgkpConvertKernelToUserStateChange", L"转换用户态调试事件", true },
        { L"DbgkpOpenHandles", L"复制调试句柄", true },
        { L"KeCopyExceptionRecord", L"复制异常记录", true },
        { L"ObReferenceObjectByHandleWithTag", L"带 Tag 引用对象", true },
        { L"ObReferenceObjectByHandle", L"引用对象句柄", true },
        { L"ObfDereferenceObjectWithTag", L"带 Tag 解引用对象", true },
        { L"ObfDereferenceObject", L"解引用对象", true },
        { L"KiCheckForKernelApcDelivery", L"检查内核 APC 投递", true },
        { L"KeEnterCriticalRegionThread", L"进入临界区", true },
        { L"KeLeaveCriticalRegionThread", L"离开临界区", true },
        { L"MmCopyVirtualMemory", L"跨进程复制内存", true },
        { L"PspCreateUserContext", L"创建用户上下文", true },
        { L"PspCallThreadNotifyRoutines", L"线程通知回调", true },
        { L"PspAllocateThread", L"分配线程对象", true },
        { L"ObpReferenceObjectByHandleWithTag", L"句柄 Tag 引用内部路径", true },
        { L"MiObtainReferencedVadEx", L"获取 VAD", true },
        { L"MmProtectVirtualMemory", L"内存保护内部路径", true },
        { L"NtGetContextThread", L"读取线程上下文", true },
        { L"NtSetContextThread", L"设置线程上下文", true },
        { L"NtQueryInformationProcess", L"查询进程信息", true },
        { L"NtSetInformationProcess", L"设置进程信息", true },
        { L"NtQueryObject", L"查询对象信息", true },
        { L"NtQuerySystemInformation", L"查询系统信息", true },
        { L"NtSystemDebugControl", L"系统调试控制", true },
        { L"NtClose", L"关闭对象句柄", true },
        { L"NtDuplicateObject", L"复制对象句柄", true },
        { L"NtSetInformationThread", L"设置线程信息", true },
        { L"ObReferenceObjectByPointer", L"按指针引用对象", true },
        { L"ZwGetContextThread", L"读取线程上下文", true },
        { L"PspGetContextThreadInternal", L"线程上下文内部路径", true },
        { L"KiDispatchException", L"分发内核异常", true },
        { L"KeStackAttachProcess", L"附加进程地址空间", true },
        { L"NtSetInformationDebugObject", L"设置调试对象信息", true },
        { L"DbgkOpenProcessDebugPort", L"打开调试端口", true },
        { L"NtTerminateProcess", L"终止进程", true },
        { L"NtSuspendThread", L"挂起线程", true },
        { L"NtResumeThread", L"恢复线程", true },
        { L"NtQueryInformationThread", L"查询线程信息", true },
        { L"PsGetCurrentProcessByThread", L"由线程取进程", true },
        { L"PsQuerySystemDllInfo", L"查询系统 DLL 信息", true },
        { L"PsWow64GetProcessNtdllType", L"查询 WoW64 NTDLL 类型", true },
        { L"PspReferenceSystemDll", L"引用系统 DLL", true },
        { L"MiSectionControlArea", L"节控制区", true },
        { L"MiReferenceControlAreaFile", L"引用控制区文件", true },
        { L"ObFastDereferenceObject", L"快速解引用对象", true },
        { L"DbgkpPostModuleMessages", L"投递模块调试消息", true },
        { L"PsCallImageNotifyRoutines", L"调用映像通知", true },
        { L"PsReferenceProcessFilePointer", L"引用进程文件对象", true },
        { L"SeAuditingWithTokenForSubcategory", L"审计子类别", true },
        { L"SeAuditProcessCreation", L"审计进程创建", true },
        { L"PspImplicitAssignProcessToJob", L"隐式加入作业", true },
        { L"PspUnlockProcessListExclusive", L"解锁进程链表", true },
        { L"DbgkCopyProcessDebugPort", L"复制调试端口", true },
        { L"SeCreateAccessStateEx", L"创建访问状态", true },
        { L"ObInsertObjectEx", L"插入对象", true },
        { L"ObCheckRefTraceProcess", L"检查引用跟踪", true },
        { L"PspValidateJobAffinityState", L"验证作业亲和性", true },
        { L"SepDeleteAccessState", L"删除访问状态", true },
        { L"PspInsertProcess", L"插入活动进程", true },
        { L"DbgkSendSystemDllMessages", L"发送系统 DLL 消息", true },
    };

    struct StructOffsetDescriptor
    {
        const wchar_t* typeName;
        const wchar_t* description;
    };

    constexpr StructOffsetDescriptor kUsedStructs[] = {
        { L"_EPROCESS", L"进程对象结构" },
        { L"_KPROCESS", L"进程内核结构" },
        { L"_ETHREAD", L"线程对象结构" },
        { L"_KTHREAD", L"线程内核结构" },
        { L"_EWOW64PROCESS", L"WoW64 进程结构" },
        { L"_KAPC_STATE", L"APC 状态结构" },
        { L"_IMAGE_NT_HEADERS64", L"PE64 头结构" },
        { L"_IMAGE_FILE_HEADER", L"PE 文件头结构" },
        { L"_MMVAD_SHORT", L"VAD 短结构" },
        { L"_HANDLE_TABLE", L"句柄表结构" },
        { L"_OBJECT_TYPE", L"对象类型结构" },
        { L"_OBJECT_TYPE_INITIALIZER", L"对象类型初始化结构" },
        { L"_PEB", L"进程环境块" },
        { L"_TEB", L"线程环境块" },
        { L"_MMVAD", L"VAD 结构" },
        { L"_MMVAD_FLAGS", L"VAD 标志结构" },
        { L"_MMVAD_FLAGS1", L"VAD 标志结构 1" },
    };

    struct UsedFieldDescriptor
    {
        const wchar_t* typeName;
        const wchar_t* fieldName;
    };

    constexpr UsedFieldDescriptor kUsedFields[] = {
        { L"_EPROCESS", L"Pcb" },
        { L"_EPROCESS", L"DebugPort" },
        { L"_EPROCESS", L"Peb" },
        { L"_EPROCESS", L"ImageFileName" },
        { L"_EPROCESS", L"WoW64Process" },
        { L"_EPROCESS", L"RundownProtect" },
        { L"_EPROCESS", L"ExitTime" },
        { L"_EPROCESS", L"Flags" },
        { L"_EPROCESS", L"SectionBaseAddress" },
        { L"_EPROCESS", L"SectionObject" },
        { L"_EPROCESS", L"Machine" },
        { L"_EPROCESS", L"ObjectTable" },
        { L"_EPROCESS", L"UniqueProcessId" },
        { L"_EPROCESS", L"Job" },
        { L"_EPROCESS", L"ActiveProcessLinks" },
        { L"_EPROCESS", L"SequenceNumber" },
        { L"_EPROCESS", L"ProcessLock" },
        { L"_KPROCESS", L"DirectoryTableBase" },
        { L"_ETHREAD", L"Tcb" },
        { L"_ETHREAD", L"CrossThreadFlags" },
        { L"_ETHREAD", L"Cid" },
        { L"_ETHREAD", L"RundownProtect" },
        { L"_ETHREAD", L"Win32StartAddress" },
        { L"_ETHREAD", L"SameThreadPassiveFlags" },
        { L"_KTHREAD", L"ApcState" },
        { L"_KTHREAD", L"PreviousMode" },
        { L"_KTHREAD", L"Teb" },
        { L"_KTHREAD", L"Process" },
        { L"_KTHREAD", L"KernelApcDisable" },
        { L"_KTHREAD", L"MiscFlags" },
        { L"_KTHREAD", L"TrapFrame" },
        { L"_KTHREAD", L"SuspendCount" },
        { L"_KTHREAD", L"SpecialApcDisable" },
        { L"_EWOW64PROCESS", L"Machine" },
        { L"_KAPC_STATE", L"Process" },
        { L"_IMAGE_NT_HEADERS64", L"FileHeader" },
        { L"_IMAGE_NT_HEADERS64", L"OptionalHeader" },
        { L"_IMAGE_NT_HEADERS64", L"Signature" },
        { L"_IMAGE_FILE_HEADER", L"PointerToSymbolTable" },
        { L"_IMAGE_FILE_HEADER", L"NumberOfSymbols" },
        { L"_MMVAD", L"Core" },
        { L"_MMVAD_SHORT", L"u" },
        { L"_HANDLE_TABLE", L"UniqueProcessId" },
        { L"_OBJECT_TYPE", L"TypeInfo" },
        { L"_OBJECT_TYPE_INITIALIZER", L"GenericMapping" },
    };

    bool IsCompilerInternalSymbol(const std::wstring& name)
    {
        return name.rfind(L"??", 0) == 0 ||
            name.find(L"<lambda_") != std::wstring::npos ||
            name.find(L"'string'") != std::wstring::npos ||
            (!name.empty() && name.front() == L'$');
    }

    std::uint64_t QueryKernelModuleBase(std::wstring& error)
    {
        std::array<LPVOID, 1024> drivers{};
        DWORD needed = 0;
        if (!::EnumDeviceDrivers(drivers.data(),
            static_cast<DWORD>(drivers.size() * sizeof(LPVOID)), &needed) ||
            needed < sizeof(LPVOID))
        {
            error = L"无法枚举内核模块基址";
            return 0;
        }
        const size_t count = (std::min)(
            static_cast<size_t>(needed / sizeof(LPVOID)),
            drivers.size());
        for (size_t index = 0; index < count; ++index)
        {
            wchar_t name[MAX_PATH]{};
            if (!::GetDeviceDriverBaseNameW(drivers[index],
                name, static_cast<DWORD>(std::size(name))))
            {
                continue;
            }
            if (_wcsicmp(name, L"ntoskrnl.exe") == 0 ||
                _wcsicmp(name, L"ntkrnlmp.exe") == 0)
            {
                return reinterpret_cast<std::uintptr_t>(drivers[index]);
            }
        }
        error = L"EnumDeviceDrivers 未找到 ntoskrnl";
        return 0;
    }

    std::wstring Lowercase(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](const wchar_t character)
            {
                return static_cast<wchar_t>(std::towlower(character));
            });
        return value;
    }

    std::unordered_map<std::wstring, std::uint64_t>
        QueryLoadedModuleBases(std::wstring& error)
    {
        std::unordered_map<std::wstring, std::uint64_t> modules;
        std::array<LPVOID, 1024> drivers{};
        DWORD needed = 0;
        if (!::EnumDeviceDrivers(drivers.data(),
            static_cast<DWORD>(drivers.size() * sizeof(LPVOID)), &needed) ||
            needed < sizeof(LPVOID))
        {
            error = L"无法枚举已加载内核模块";
            return modules;
        }
        const size_t count = (std::min)(
            static_cast<size_t>(needed / sizeof(LPVOID)),
            drivers.size());
        for (size_t index = 0; index < count; ++index)
        {
            wchar_t name[MAX_PATH]{};
            if (!::GetDeviceDriverBaseNameW(drivers[index],
                name, static_cast<DWORD>(std::size(name))))
            {
                continue;
            }
            std::wstring stem =
                std::filesystem::path(name).stem().wstring();
            modules[Lowercase(std::move(stem))] =
                reinterpret_cast<std::uintptr_t>(drivers[index]);
        }
        const auto ntos = modules.find(L"ntoskrnl");
        if (ntos != modules.end())
        {
            modules[L"ntkrnlmp"] = ntos->second;
        }
        return modules;
    }

    std::vector<std::wstring> CollectSymbolPdbFiles(std::wstring& error)
    {
        std::vector<std::wstring> files;
        std::unordered_set<std::wstring> seen;
        std::error_code iteratorError;
        std::filesystem::recursive_directory_iterator iterator(
            kSymbolCacheDirectory,
            std::filesystem::directory_options::skip_permission_denied,
            iteratorError);
        const std::filesystem::recursive_directory_iterator end;
        if (iteratorError)
        {
            error = L"无法遍历 C:\\Symbols：" +
                std::to_wstring(iteratorError.value());
            return files;
        }
        for (; iterator != end; iterator.increment(iteratorError))
        {
            if (iteratorError)
            {
                iteratorError.clear();
                continue;
            }
            std::error_code fileError;
            if (!iterator->is_regular_file(fileError) || fileError)
            {
                continue;
            }
            if (Lowercase(iterator->path().extension().wstring()) != L".pdb")
            {
                continue;
            }
            const std::wstring path =
                std::filesystem::absolute(iterator->path(), fileError).wstring();
            if (fileError)
            {
                continue;
            }
            const std::wstring key = Lowercase(path);
            if (seen.insert(key).second)
            {
                files.push_back(path);
            }
        }
        std::sort(files.begin(), files.end(),
            [](const std::wstring& left, const std::wstring& right)
            {
                return Lowercase(left) < Lowercase(right);
            });
        return files;
    }

    std::wstring ResolveImageForPdb(const std::wstring& pdbPath)
    {
        std::wstring stem = Lowercase(
            std::filesystem::path(pdbPath).stem().wstring());
        if (stem == L"ntkrnlmp")
        {
            stem = L"ntoskrnl";
        }
        const std::filesystem::path pdbDirectory =
            std::filesystem::path(pdbPath).parent_path();
        wchar_t systemDirectory[MAX_PATH]{};
        const UINT systemLength = ::GetSystemDirectoryW(
            systemDirectory, static_cast<UINT>(std::size(systemDirectory)));
        const std::array<std::wstring, 8> candidates = {
            (pdbDirectory / (stem + L".exe")).wstring(),
            (pdbDirectory / (stem + L".sys")).wstring(),
            (pdbDirectory / (stem + L".dll")).wstring(),
            systemLength == 0 ? std::wstring{} :
                (std::filesystem::path(systemDirectory) /
                    (stem + L".exe")).wstring(),
            systemLength == 0 ? std::wstring{} :
                (std::filesystem::path(systemDirectory) /
                    (stem + L".sys")).wstring(),
            systemLength == 0 ? std::wstring{} :
                (std::filesystem::path(systemDirectory) /
                    (stem + L".dll")).wstring(),
            systemLength == 0 ? std::wstring{} :
                (std::filesystem::path(systemDirectory) / L"drivers" /
                    (stem + L".sys")).wstring(),
            systemLength == 0 ? std::wstring{} :
                (std::filesystem::path(systemDirectory) / L"drivers" /
                    (stem + L".dll")).wstring(),
        };
        for (const auto& candidate : candidates)
        {
            if (!candidate.empty() && unrealdbg_native::FileExists(candidate))
            {
                return candidate;
            }
        }
        return {};
    }

    struct SymbolEnumerationContext
    {
        std::wstring moduleName;
        DWORD64 moduleBase{};
        std::uint64_t kernelBase{};
        const std::unordered_map<std::wstring,
            const SymbolUsageDescriptor*>* usage{};
        std::vector<unrealdbg_native::SystemSymbolEntry>* symbols{};
        std::unordered_set<std::wstring>* seen{};
    };

    constexpr DWORD kDbgHelpSymTagFunction = 5;
    constexpr DWORD kDbgHelpSymTagData = 7;

    BOOL CALLBACK EnumerateSystemSymbol(
        PSYMBOL_INFOW info, ULONG, PVOID userContext)
    {
        auto* context = reinterpret_cast<SymbolEnumerationContext*>(userContext);
        if (context == nullptr || info == nullptr || info->Name[0] == L'\0' ||
            info->Address < context->moduleBase)
        {
            return TRUE;
        }

        const std::uint64_t rva =
            static_cast<std::uint64_t>(info->Address - context->moduleBase);
        const std::wstring name = info->Name;
        const std::wstring key =
            name + L":" + std::to_wstring(rva);
        if (!context->seen->insert(key).second)
        {
            return TRUE;
        }

        DWORD tag = 0;
        const bool haveTag = ::SymGetTypeInfo(::GetCurrentProcess(),
            context->moduleBase, info->Index, TI_GET_SYMTAG, &tag) != FALSE;

        unrealdbg_native::SystemSymbolEntry entry;
        entry.moduleName = context->moduleName;
        entry.symbolName = name;
        entry.internalSymbol = IsCompilerInternalSymbol(name);
        entry.kind = haveTag && tag == kDbgHelpSymTagFunction
            ? L"函数"
            : haveTag && tag == kDbgHelpSymTagData
                ? L"变量" : L"符号";
        entry.description = L"系统符号";
        const auto used = context->usage->find(name);
        if (used != context->usage->end())
        {
            entry.description = used->second->usage;
            entry.usedByProject = true;
        }
        entry.rva = rva;
        entry.found = true;
        if (context->kernelBase != 0)
        {
            entry.address = context->kernelBase + rva;
            entry.addressValid = true;
        }
        context->symbols->push_back(std::move(entry));
        return TRUE;
    }

    struct StructEnumerationContext
    {
        DWORD64 loaded{};
        std::wstring moduleName;
        const std::unordered_map<std::wstring,
            const StructOffsetDescriptor*>* usedTypes{};
        const std::unordered_set<std::wstring>* usedFields{};
        std::vector<unrealdbg_native::SystemSymbolEntry>* symbols{};
        std::unordered_set<std::wstring>* seen{};
    };

    constexpr ULONG kDbgHelpSymTagUdt = 11;

    BOOL CALLBACK EnumerateStructType(
        PSYMBOL_INFOW info, ULONG, PVOID userContext)
    {
        auto* context = reinterpret_cast<StructEnumerationContext*>(userContext);
        if (context == nullptr || info == nullptr ||
            info->Tag != kDbgHelpSymTagUdt ||
            info->Name[0] == L'\0')
        {
            return TRUE;
        }

        DWORD childCount = 0;
        if (!::SymGetTypeInfo(::GetCurrentProcess(), context->loaded,
            info->Index, TI_GET_CHILDRENCOUNT, &childCount) ||
            childCount == 0)
        {
            return TRUE;
        }
        const size_t childBytes = sizeof(TI_FINDCHILDREN_PARAMS) +
            static_cast<size_t>(childCount) * sizeof(ULONG);
        std::vector<BYTE> childBuffer(childBytes);
        auto* children = reinterpret_cast<TI_FINDCHILDREN_PARAMS*>(
            childBuffer.data());
        children->Count = childCount;
        children->Start = 0;
        if (!::SymGetTypeInfo(::GetCurrentProcess(), context->loaded,
            info->Index, TI_FINDCHILDREN, children))
        {
            return TRUE;
        }

        const std::wstring typeName = info->Name;
        const auto usedType = context->usedTypes->find(Lowercase(typeName));
        for (DWORD index = 0; index < childCount; ++index)
        {
            PWSTR childName = nullptr;
            DWORD offset = 0;
            if (!::SymGetTypeInfo(::GetCurrentProcess(), context->loaded,
                children->ChildId[index], TI_GET_SYMNAME, &childName) ||
                childName == nullptr)
            {
                continue;
            }
            const bool haveOffset = ::SymGetTypeInfo(
                ::GetCurrentProcess(), context->loaded,
                children->ChildId[index], TI_GET_OFFSET, &offset) != FALSE;
            if (!haveOffset)
            {
                ::LocalFree(childName);
                continue;
            }

            const std::wstring symbolName =
                typeName + L"." + childName;
            const bool usedField = context->usedFields->find(
                Lowercase(symbolName)) != context->usedFields->end();
            ::LocalFree(childName);
            const std::wstring key = context->moduleName + L":" +
                symbolName + L":" + std::to_wstring(offset);
            if (!context->seen->insert(key).second)
            {
                continue;
            }

            unrealdbg_native::SystemSymbolEntry entry;
            entry.moduleName = context->moduleName;
            entry.symbolName = symbolName;
            entry.kind = L"结构字段";
            entry.description = usedField
                ? (usedType == context->usedTypes->end()
                    ? L"项目使用的结构字段偏移"
                    : std::wstring(usedType->second->description) +
                        L" · 项目使用")
                : L"结构字段偏移";
            entry.rva = offset;
            entry.found = true;
            entry.usedByProject = usedField;
            entry.offset = true;
            context->symbols->push_back(std::move(entry));
        }
        return TRUE;
    }

    void AppendAllStructFields(const DWORD64 loaded,
        const std::wstring& moduleName,
        std::vector<unrealdbg_native::SystemSymbolEntry>& symbols,
        std::unordered_set<std::wstring>& seen)
    {
        std::unordered_map<std::wstring,
            const StructOffsetDescriptor*> usedTypes;
        for (const auto& descriptor : kUsedStructs)
        {
            usedTypes.emplace(Lowercase(descriptor.typeName), &descriptor);
        }
        std::unordered_set<std::wstring> usedFields;
        usedFields.reserve(std::size(kUsedFields));
        for (const auto& descriptor : kUsedFields)
        {
            usedFields.emplace(Lowercase(std::wstring(descriptor.typeName) +
                L"." + descriptor.fieldName));
        }
        StructEnumerationContext context{
            loaded, moduleName, &usedTypes, &usedFields, &symbols, &seen
        };
        (void)::SymEnumTypesW(::GetCurrentProcess(), loaded,
            EnumerateStructType, &context);
    }

    bool QueryPdbSymbols(const std::wstring& pdbPath,
        const std::wstring& imagePath,
        const std::wstring& moduleName, const DWORD64 moduleBase,
        const std::uint64_t kernelBase,
        std::vector<unrealdbg_native::SystemSymbolEntry>& symbols,
        std::wstring& error)
    {
        symbols.clear();
        error.clear();
        if (!InitializeDbgHelp())
        {
            error = L"DbgHelp 初始化失败";
            return false;
        }
        if (!unrealdbg_native::FileExists(pdbPath))
        {
            error = L"PDB 文件不存在：" + pdbPath;
            return false;
        }
        const std::wstring searchPath =
            std::filesystem::path(pdbPath).parent_path().wstring() +
            L";" + kSymbolCacheDirectory;
        (void)::SymSetSearchPathW(::GetCurrentProcess(), searchPath.c_str());
        DWORD64 loaded = 0;
        if (!imagePath.empty() && unrealdbg_native::FileExists(imagePath))
        {
            loaded = ::SymLoadModuleExW(::GetCurrentProcess(), nullptr,
                imagePath.c_str(), nullptr, moduleBase, 0, nullptr, 0);
        }
        if (loaded == 0)
        {
            loaded = ::SymLoadModuleExW(::GetCurrentProcess(), nullptr,
                pdbPath.c_str(), nullptr, moduleBase, 0, nullptr,
                SLMFLAG_VIRTUAL);
        }
        if (loaded == 0)
        {
            error = L"无法加载 PDB：" + pdbPath;
            return false;
        }

        std::unordered_map<std::wstring,
            const SymbolUsageDescriptor*> usage;
        usage.reserve(std::size(kUsedSymbols));
        for (const auto& descriptor : kUsedSymbols)
        {
            usage.emplace(descriptor.name, &descriptor);
        }
        std::unordered_set<std::wstring> seen;
        seen.reserve(131072);
        symbols.reserve(131072);
        SymbolEnumerationContext context{
            moduleName, moduleBase, kernelBase, &usage, &symbols, &seen
        };
        const BOOL enumerated = ::SymEnumSymbolsW(
            ::GetCurrentProcess(), loaded, L"*",
            EnumerateSystemSymbol, &context);
        if (enumerated)
        {
            AppendAllStructFields(loaded, moduleName, symbols, seen);
        }
        ::SymUnloadModule64(::GetCurrentProcess(), loaded);
        if (!enumerated)
        {
            error = L"PDB 符号枚举失败：" + pdbPath;
            return false;
        }
        if (symbols.empty())
        {
            error = imagePath.empty()
                ? L"PDB 未枚举到符号，且找不到匹配映像：" + pdbPath
                : L"PDB 未枚举到符号，可能映像与 PDB 不匹配：" +
                    imagePath;
            return false;
        }

        std::sort(symbols.begin(), symbols.end(),
            [](const auto& left, const auto& right)
            {
                if (left.usedByProject != right.usedByProject)
                {
                    return left.usedByProject;
                }
                if (left.symbolName != right.symbolName)
                {
                    return left.symbolName < right.symbolName;
                }
                return left.rva < right.rva;
            });
        return true;
    }

}

namespace unrealdbg_native
{
    SymbolPreparer::SymbolPreparer(std::wstring applicationDirectory)
        : applicationDirectory_(std::move(applicationDirectory))
    {
        const std::wstring symbolToolConfig =
            PathInDirectory(applicationDirectory_, L"SymbolTool\\Config.ini");
        const std::wstring fallbackConfig =
            PathInDirectory(applicationDirectory_, L"Config\\SymbolTool\\Config.ini");
        if (FileExists(symbolToolConfig))
        {
            configPath_ = symbolToolConfig;
        }
        else if (FileExists(fallbackConfig))
        {
            configPath_ = fallbackConfig;
        }
        else
        {
            configPath_ = symbolToolConfig;
            std::error_code error;
            std::filesystem::create_directories(
                std::filesystem::path(configPath_).parent_path(), error);
            if (!error)
            {
                // Microsoft Symbol Server is the default for new deployments.
                ::WritePrivateProfileStringW(L"Symbol", L"Server", L"200",
                    configPath_.c_str());
            }
        }

        const std::wstring systemDirectory = SystemDirectory();
        modules_ = {
            SymbolModuleStatus{ L"ntoskrnl.exe",
                PathInDirectory(systemDirectory, L"ntoskrnl.exe") },
            SymbolModuleStatus{ L"win32kbase.sys",
                PathInDirectory(systemDirectory, L"win32kbase.sys") },
            SymbolModuleStatus{ L"win32kfull.sys",
                PathInDirectory(systemDirectory, L"win32kfull.sys") },
        };
        for (auto& module : modules_)
        {
            module.detail = L"等待校验";
        }
    }

    SymbolPreparer::~SymbolPreparer()
    {
        Shutdown();
    }

    void SymbolPreparer::PrepareAsync()
    {
        if (State() == SymbolPreparationState::Preparing)
        {
            return;
        }
        if (worker_.joinable())
        {
            worker_.join();
        }

        {
            std::lock_guard lock(dataMutex_);
            for (auto& module : modules_)
            {
                module.state = SymbolModuleState::Pending;
                module.progress = 0;
                module.pdbName.clear();
                module.pdbPath.clear();
                module.detail = L"等待校验";
            }
            lastError_.clear();
        }
        stopRequested_.store(false, std::memory_order_release);
        progress_.store(0, std::memory_order_release);
        SetState(SymbolPreparationState::Preparing);

        try
        {
            worker_ = std::thread([this] { Worker(); });
        }
        catch (const std::exception&)
        {
            SetLastError(L"无法启动符号准备线程");
            SetState(SymbolPreparationState::Failed);
        }
    }

    void SymbolPreparer::AnalyzeSystemSymbolsAsync(std::wstring pdbPath)
    {
        const SymbolAnalysisState current =
            analysisState_.load(std::memory_order_acquire);
        if (current == SymbolAnalysisState::Analyzing)
        {
            return;
        }
        if (analysisWorker_.joinable() &&
            analysisWorker_.get_id() != std::this_thread::get_id())
        {
            analysisWorker_.join();
        }

        {
            std::lock_guard lock(analysisMutex_);
            systemSymbols_.reset();
            analysisSummary_.clear();
            analysisError_.clear();
            analysisRuntimePdb_.clear();
            analysisModuleBase_.clear();
        }
        SetAnalysisState(SymbolAnalysisState::Analyzing);
        try
        {
            analysisWorker_ = std::thread([this,
                requestedPdbPath = std::move(pdbPath)]() mutable
            {
                SymbolAnalysisWorker(std::move(requestedPdbPath));
            });
        }
        catch (const std::exception&)
        {
            std::lock_guard lock(analysisMutex_);
            analysisError_ = L"无法启动系统符号分析线程";
            SetAnalysisState(SymbolAnalysisState::Failed);
        }
    }

    void SymbolPreparer::Shutdown() noexcept
    {
        stopRequested_.store(true, std::memory_order_release);
        if (worker_.joinable() &&
            worker_.get_id() != std::this_thread::get_id())
        {
            worker_.join();
        }
        if (analysisWorker_.joinable() &&
            analysisWorker_.get_id() != std::this_thread::get_id())
        {
            analysisWorker_.join();
        }
    }

    SymbolPreparationState SymbolPreparer::State() const noexcept
    {
        return state_.load(std::memory_order_acquire);
    }

    bool SymbolPreparer::IsPreparing() const noexcept
    {
        return State() == SymbolPreparationState::Preparing;
    }

    bool SymbolPreparer::IsReady() const noexcept
    {
        return State() == SymbolPreparationState::Ready;
    }

    int SymbolPreparer::Progress() const noexcept
    {
        return progress_.load(std::memory_order_acquire);
    }

    std::wstring SymbolPreparer::Summary() const
    {
        std::lock_guard lock(dataMutex_);
        size_t ready{};
        for (const auto& module : modules_)
        {
            if (module.state == SymbolModuleState::Cached ||
                module.state == SymbolModuleState::Downloaded)
            {
                ++ready;
            }
        }

        switch (State())
        {
        case SymbolPreparationState::Preparing:
        {
            size_t active{};
            for (const auto& module : modules_)
            {
                if (module.state == SymbolModuleState::Checking ||
                    module.state == SymbolModuleState::Downloading)
                {
                    ++active;
                }
            }
            return L"正在并行准备 " + std::to_wstring(active) +
                L" 个符号 · " + std::to_wstring(Progress()) + L"%";
        }
        case SymbolPreparationState::Ready:
            return std::to_wstring(ready) + L"/" +
                std::to_wstring(modules_.size()) + L" 缓存可用";
        case SymbolPreparationState::Failed:
            return L"符号准备失败 · " + std::to_wstring(ready) + L"/" +
                std::to_wstring(modules_.size()) + L" 可用";
        case SymbolPreparationState::Idle:
        default:
            return L"等待校验";
        }
    }

    std::wstring SymbolPreparer::LastError() const
    {
        std::lock_guard lock(dataMutex_);
        return lastError_;
    }

    std::wstring SymbolPreparer::CacheDirectory() const
    {
        return kSymbolCacheDirectory;
    }

    std::vector<SymbolModuleStatus> SymbolPreparer::Modules() const
    {
        std::lock_guard lock(dataMutex_);
        return modules_;
    }

    SymbolAnalysisState SymbolPreparer::AnalysisState() const noexcept
    {
        return analysisState_.load(std::memory_order_acquire);
    }

    SystemSymbolSnapshot SymbolPreparer::SystemSymbols() const
    {
        std::lock_guard lock(analysisMutex_);
        return systemSymbols_;
    }

    std::wstring SymbolPreparer::AnalysisSummary() const
    {
        std::lock_guard lock(analysisMutex_);
        return analysisSummary_;
    }

    std::wstring SymbolPreparer::AnalysisError() const
    {
        std::lock_guard lock(analysisMutex_);
        return analysisError_;
    }

    std::wstring SymbolPreparer::AnalysisRuntimePdb() const
    {
        std::lock_guard lock(analysisMutex_);
        return analysisRuntimePdb_;
    }

    std::wstring SymbolPreparer::AnalysisModuleBase() const
    {
        std::lock_guard lock(analysisMutex_);
        return analysisModuleBase_;
    }

    void SymbolPreparer::SymbolAnalysisWorker(std::wstring requestedPdbPath)
    {
        const auto fail = [this](std::wstring message)
        {
            {
                std::lock_guard lock(analysisMutex_);
                analysisError_ = std::move(message);
            }
            SetAnalysisState(SymbolAnalysisState::Failed);
        };

        const bool customPdb = !requestedPdbPath.empty();
        std::wstring discoveryError;
        std::vector<std::wstring> pdbFiles;
        if (customPdb)
        {
            if (!FileExists(requestedPdbPath))
            {
                fail(L"所选 PDB 不存在：" + requestedPdbPath);
                return;
            }
            pdbFiles.push_back(requestedPdbPath);
        }
        else
        {
            pdbFiles = CollectSymbolPdbFiles(discoveryError);
            if (pdbFiles.empty())
            {
                fail(discoveryError.empty()
                    ? L"C:\\Symbols 中没有可解析的 PDB"
                    : discoveryError);
                return;
            }
        }

        std::wstring baseError;
        const auto moduleBases = QueryLoadedModuleBases(baseError);
        const auto ntos = moduleBases.find(L"ntoskrnl");
        const std::uint64_t kernelBase = ntos == moduleBases.end()
            ? 0 : ntos->second;
        std::vector<SystemSymbolEntry> symbols;
        std::wstring skippedDetails;
        size_t failedPdbCount = 0;
        for (const std::wstring& pdbPath : pdbFiles)
        {
            std::error_code relativeError;
            const std::wstring relativePath =
                std::filesystem::relative(pdbPath, kSymbolCacheDirectory,
                    relativeError).wstring();
            const std::wstring moduleName =
                relativeError ? std::filesystem::path(pdbPath)
                    .filename().wstring() : relativePath;
            const std::wstring moduleStem =
                Lowercase(std::filesystem::path(pdbPath).stem().wstring());
            const auto base = moduleBases.find(moduleStem);
            const std::uint64_t moduleBase =
                base == moduleBases.end() ? 0 : base->second;
            const std::wstring imagePath = ResolveImageForPdb(pdbPath);
            std::vector<SystemSymbolEntry> pdbSymbols;
            std::wstring queryError;
            bool queryOk = false;
            {
                std::lock_guard lock(g_dbgHelpMutex);
                queryOk = QueryPdbSymbols(pdbPath, imagePath, moduleName,
                    0x0000000180000000ULL, moduleBase,
                    pdbSymbols, queryError);
            }
            if (!queryOk)
            {
                ++failedPdbCount;
                if (skippedDetails.size() < 2048)
                {
                    skippedDetails += L"\r\n" + moduleName + L"：" +
                        queryError;
                }
                continue;
            }
            symbols.insert(symbols.end(),
                std::make_move_iterator(pdbSymbols.begin()),
                std::make_move_iterator(pdbSymbols.end()));
        }
        if (symbols.empty())
        {
            fail(L"所有 PDB 解析失败" + skippedDetails);
            return;
        }

        std::sort(symbols.begin(), symbols.end(),
            [](const auto& left, const auto& right)
            {
                if (left.internalSymbol != right.internalSymbol)
                {
                    return !left.internalSymbol;
                }
                if (left.usedByProject != right.usedByProject)
                {
                    return left.usedByProject;
                }
                if (left.offset != right.offset)
                {
                    return left.offset;
                }
                if (left.moduleName != right.moduleName)
                {
                    return left.moduleName < right.moduleName;
                }
                if (left.symbolName != right.symbolName)
                {
                    return left.symbolName < right.symbolName;
                }
                return left.rva < right.rva;
            });
        size_t used = 0;
        size_t addressReady = 0;
        size_t structFields = 0;
        for (const auto& symbol : symbols)
        {
            if (symbol.usedByProject)
            {
                ++used;
            }
            if (symbol.addressValid)
            {
                ++addressReady;
            }
            if (symbol.offset)
            {
                ++structFields;
            }
        }
        const size_t symbolCount = symbols.size();
        auto snapshot = std::make_shared<const std::vector<SystemSymbolEntry>>(
            std::move(symbols));

        {
            std::lock_guard lock(analysisMutex_);
            systemSymbols_ = std::move(snapshot);
            analysisError_ = skippedDetails;
            analysisModuleBase_ = kernelBase == 0
                ? baseError : HexOffset(kernelBase);
            analysisSummary_ = std::wstring(
                customPdb ? L"自定义 PDB" : L"系统 PDB") +
                L" · " + std::to_wstring(pdbFiles.size()) + L" 个文件" +
                L" · 已解析 " + std::to_wstring(symbolCount) +
                L" 个（结构字段 " + std::to_wstring(structFields) +
                L"） · 本项目使用 " + std::to_wstring(used) +
                L" 个 · 可定位地址 " + std::to_wstring(addressReady) +
                L" 个" +
                (failedPdbCount == 0
                    ? L"" : L" · 失败 " + std::to_wstring(failedPdbCount) +
                        L" 个");
            analysisRuntimePdb_ = customPdb
                ? requestedPdbPath
                : L"C:\\Symbols（" + std::to_wstring(pdbFiles.size()) +
                    L" 个 PDB）";
        }
        SetAnalysisState(SymbolAnalysisState::Ready);
    }

    void SymbolPreparer::Worker()
    {
        WriteLog(LogLevel::Info,
            L"Loader 符号准备已并入前台：并行校验 ntoskrnl / win32kbase / win32kfull");

        std::atomic<size_t> successCount{};
        std::atomic<size_t> cachedCount{};
        std::atomic<size_t> downloadCount{};
        std::vector<std::thread> downloadWorkers;
        downloadWorkers.reserve(modules_.size());
        bool workerStartFailed{};

        try
        {
            for (size_t index = 0; index < modules_.size(); ++index)
            {
                downloadWorkers.emplace_back([this, index,
                    &successCount, &cachedCount, &downloadCount]()
                {
                    if (stopRequested_.load(std::memory_order_acquire))
                    {
                        return;
                    }
                    try
                    {
                        bool usedCache = false;
                        if (PrepareModule(index, usedCache))
                        {
                            successCount.fetch_add(1, std::memory_order_acq_rel);
                            if (usedCache)
                            {
                                cachedCount.fetch_add(1, std::memory_order_acq_rel);
                            }
                            else
                            {
                                downloadCount.fetch_add(1, std::memory_order_acq_rel);
                            }
                        }
                    }
                    catch (const std::exception& error)
                    {
                        SetLastError(L"符号准备线程异常：" +
                            std::wstring(error.what(), error.what() +
                                std::strlen(error.what())));
                    }
                    catch (...)
                    {
                        SetLastError(L"符号准备线程发生未知异常");
                    }
                });
            }
        }
        catch (const std::exception& error)
        {
            workerStartFailed = true;
            stopRequested_.store(true, std::memory_order_release);
            SetLastError(L"创建符号下载线程失败：" +
                std::wstring(error.what(), error.what() +
                    std::strlen(error.what())));
        }

        for (std::thread& worker : downloadWorkers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        if (workerStartFailed)
        {
            SetState(SymbolPreparationState::Failed);
            return;
        }
        if (stopRequested_.load(std::memory_order_acquire))
        {
            return;
        }

        if (successCount.load(std::memory_order_acquire) == modules_.size())
        {
            WriteLog(LogLevel::Info,
                L"符号缓存准备完成：缓存 " +
                std::to_wstring(cachedCount.load(std::memory_order_acquire)) +
                L" 个，下载 " +
                std::to_wstring(downloadCount.load(std::memory_order_acquire)) +
                L" 个");
            SetProgress(100);
            SetState(SymbolPreparationState::Ready);
            return;
        }

        WriteLog(LogLevel::Error, L"符号缓存准备未全部完成：" +
            LastError());
        SetState(SymbolPreparationState::Failed);
    }

    bool SymbolPreparer::PrepareModule(const size_t index, bool& usedCache)
    {
        usedCache = false;
        SymbolModuleStatus module;
        {
            std::lock_guard lock(dataMutex_);
            if (index >= modules_.size())
            {
                return false;
            }
            module = modules_[index];
        }

        module.state = SymbolModuleState::Checking;
        module.detail = L"读取 PE 调试签名";
        module.progress = 0;
        module.downloadedBytes = 0;
        module.totalBytes = 0;
        SetModule(index, module);

        if (!FileExists(module.modulePath))
        {
            module.state = SymbolModuleState::Failed;
            module.detail = L"系统模块不存在";
            SetModule(index, module);
            SetLastError(module.moduleName + L"：系统模块不存在");
            return false;
        }

        PdbIdentity identity;
        if (!ParsePeIdentity(module.modulePath, identity))
        {
            module.state = SymbolModuleState::Failed;
            module.detail = L"无法解析 PDB 签名";
            SetModule(index, module);
            SetLastError(module.moduleName + L"：无法解析 PDB 签名");
            return false;
        }

        const std::wstring key = GuidKey(identity.guid, identity.age);
        if (key.empty())
        {
            module.state = SymbolModuleState::Failed;
            module.detail = L"GUID 生成失败";
            SetModule(index, module);
            SetLastError(module.moduleName + L"：GUID 生成失败");
            return false;
        }

        module.pdbName = identity.fileName;
        const std::wstring expectedPath = PathInDirectory(
            PathInDirectory(
                PathInDirectory(kSymbolCacheDirectory, identity.fileName),
                key),
            identity.fileName);

        std::wstring locatedPath;
        WriteLog(LogLevel::Debug,
            module.moduleName + L" PDB=" + identity.fileName +
            L"，ID=" + key + L"，Age=" + std::to_wstring(identity.age));

        if (LocateLocalPdb(identity.guid, identity.age,
            identity.fileName, locatedPath))
        {
            usedCache = true;
            module.pdbPath = locatedPath;
            module.state = SymbolModuleState::Cached;
            module.progress = 100;
            module.detail = L"本地缓存校验通过";
            std::error_code sizeError;
            const std::uintmax_t fileSize =
                std::filesystem::file_size(locatedPath, sizeError);
            if (!sizeError)
            {
                module.downloadedBytes = fileSize;
                module.totalBytes = fileSize;
            }
            SetModule(index, module);
            WriteLog(LogLevel::Info,
                module.moduleName + L" 使用本地 PDB：" + module.pdbPath);
            return true;
        }

        module.pdbPath = expectedPath;
        module.state = SymbolModuleState::Downloading;
        module.detail = L"正在下载符号";
        module.progress = 0;
        module.downloadedBytes = 0;
        module.totalBytes = 0;
        SetModule(index, module);
        WriteLog(LogLevel::Info,
            module.moduleName + L" 缺少匹配 PDB，开始下载");

        if (!DownloadPdb(index, module, identity, module.pdbPath))
        {
            {
                std::lock_guard lock(dataMutex_);
                if (index < modules_.size())
                {
                    module = modules_[index];
                }
            }
            module.state = SymbolModuleState::Failed;
            if (module.detail.empty() ||
                module.detail == L"正在下载符号")
            {
                module.detail = L"下载或校验失败";
            }
            SetModule(index, module);
            SetLastError(module.moduleName + L"：" + module.detail);
            return false;
        }

        {
            std::lock_guard lock(dataMutex_);
            if (index < modules_.size())
            {
                module = modules_[index];
            }
        }
        module.state = SymbolModuleState::Downloaded;
        module.progress = 100;
        module.detail = L"下载并校验通过";
        SetModule(index, module);
        WriteLog(LogLevel::Info,
            module.moduleName + L" 符号已保存：" + module.pdbPath);
        return true;
    }

    bool SymbolPreparer::ParsePeIdentity(const std::wstring& modulePath,
        PdbIdentity& identity) const
    {
        std::vector<std::uint8_t> data;
        if (!ReadFileBytes(modulePath, kMaximumPeBytes, data) ||
            data.size() < sizeof(IMAGE_DOS_HEADER))
        {
            return false;
        }

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(data.data());
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        {
            return false;
        }

        const size_t ntOffset = static_cast<size_t>(dos->e_lfanew);
        if (!RangeWithin(data.size(), ntOffset,
            sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)))
        {
            return false;
        }
        DWORD signature{};
        memcpy(&signature, data.data() + ntOffset, sizeof(signature));
        if (signature != IMAGE_NT_SIGNATURE)
        {
            return false;
        }

        const auto* fileHeader = reinterpret_cast<const IMAGE_FILE_HEADER*>(
            data.data() + ntOffset + sizeof(DWORD));
        const size_t optionalOffset = ntOffset + sizeof(DWORD) +
            sizeof(IMAGE_FILE_HEADER);
        if (!RangeWithin(data.size(), optionalOffset,
            fileHeader->SizeOfOptionalHeader) ||
            fileHeader->SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32) ||
            fileHeader->NumberOfSections == 0)
        {
            return false;
        }

        WORD magic{};
        memcpy(&magic,
            data.data() + optionalOffset + offsetof(IMAGE_OPTIONAL_HEADER32, Magic),
            sizeof(magic));

        const IMAGE_DATA_DIRECTORY* debugDirectory{};
        if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        {
            if (fileHeader->SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
            {
                return false;
            }
            const auto* optional = reinterpret_cast<const IMAGE_OPTIONAL_HEADER64*>(
                data.data() + optionalOffset);
            debugDirectory =
                &optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        }
        else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        {
            const auto* optional = reinterpret_cast<const IMAGE_OPTIONAL_HEADER32*>(
                data.data() + optionalOffset);
            debugDirectory =
                &optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        }
        else
        {
            return false;
        }

        if (debugDirectory->VirtualAddress == 0 ||
            debugDirectory->Size < sizeof(IMAGE_DEBUG_DIRECTORY))
        {
            return false;
        }

        const size_t sectionOffset =
            optionalOffset + fileHeader->SizeOfOptionalHeader;
        const size_t sectionBytes = static_cast<size_t>(
            fileHeader->NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        if (!RangeWithin(data.size(), sectionOffset, sectionBytes))
        {
            return false;
        }
        const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
            data.data() + sectionOffset);

        size_t debugOffset{};
        if (!RvaToFileOffset(data, sections, fileHeader->NumberOfSections,
            debugDirectory->VirtualAddress, debugOffset) ||
            !RangeWithin(data.size(), debugOffset, debugDirectory->Size))
        {
            return false;
        }

        const size_t debugCount =
            debugDirectory->Size / sizeof(IMAGE_DEBUG_DIRECTORY);
        for (size_t index = 0; index < debugCount; ++index)
        {
            const auto* entry = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(
                data.data() + debugOffset +
                index * sizeof(IMAGE_DEBUG_DIRECTORY));
            if (entry->Type != IMAGE_DEBUG_TYPE_CODEVIEW ||
                entry->SizeOfData < sizeof(RsdsHeader))
            {
                continue;
            }

            size_t codeViewOffset{};
            if (entry->PointerToRawData != 0)
            {
                codeViewOffset = entry->PointerToRawData;
            }
            else if (entry->AddressOfRawData != 0)
            {
                if (!RvaToFileOffset(data, sections,
                    fileHeader->NumberOfSections,
                    entry->AddressOfRawData, codeViewOffset))
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            if (!RangeWithin(data.size(), codeViewOffset, entry->SizeOfData))
            {
                continue;
            }
            const auto* header = reinterpret_cast<const RsdsHeader*>(
                data.data() + codeViewOffset);
            if (header->signature != kRsdsSignature)
            {
                continue;
            }

            const size_t nameOffset = codeViewOffset + sizeof(RsdsHeader);
            const size_t nameMaximum =
                codeViewOffset + entry->SizeOfData - nameOffset;
            const char* name = reinterpret_cast<const char*>(
                data.data() + nameOffset);
            size_t nameLength{};
            while (nameLength < nameMaximum && name[nameLength] != '\0')
            {
                ++nameLength;
            }
            if (nameLength == 0 || nameLength >= nameMaximum)
            {
                continue;
            }

            std::string asciiName(name, nameLength);
            if (!IsValidPdbFileName(asciiName))
            {
                continue;
            }

            const int wideLength = ::MultiByteToWideChar(CP_ACP, 0,
                asciiName.c_str(), static_cast<int>(asciiName.size()),
                nullptr, 0);
            if (wideLength <= 0)
            {
                continue;
            }
            identity.fileName.resize(static_cast<size_t>(wideLength));
            if (::MultiByteToWideChar(CP_ACP, 0,
                asciiName.c_str(), static_cast<int>(asciiName.size()),
                identity.fileName.data(), wideLength) != wideLength)
            {
                continue;
            }
            identity.guid = header->guid;
            identity.age = header->age;
            return true;
        }
        return false;
    }

    bool SymbolPreparer::VerifyPdb(const std::wstring& pdbPath,
        const PdbIdentity& identity) const
    {
        return VerifyPdbFile(pdbPath, identity.guid, identity.age);
    }

    bool SymbolPreparer::DownloadPdb(const size_t index,
        const SymbolModuleStatus& module,
        const PdbIdentity& identity, const std::wstring& targetPath)
    {
        std::error_code error;
        const std::filesystem::path parent =
            std::filesystem::path(targetPath).parent_path();
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            return false;
        }

        const std::wstring key = GuidKey(identity.guid, identity.age);
        const std::wstring url = BuildServerBaseUrl() +
            identity.fileName + L"/" + key + L"/" + identity.fileName;
        const std::wstring temporary = targetPath + L"." +
            std::to_wstring(::GetCurrentProcessId()) + L".download";
        ::DeleteFileW(temporary.c_str());

        std::wstring downloadError;
        const bool downloaded = DownloadUrlToFile(url, temporary,
            [this, index](const std::uint64_t downloadedBytes,
                const std::uint64_t totalBytes)
            {
                UpdateDownloadProgress(index,
                    downloadedBytes, totalBytes);
            },
            [this]()
            {
                return stopRequested_.load(std::memory_order_acquire);
            },
            downloadError);
        if (!downloaded)
        {
            SymbolModuleStatus status = module;
            {
                std::lock_guard lock(dataMutex_);
                if (index < modules_.size())
                {
                    status = modules_[index];
                }
            }
            status.detail = downloadError;
            if (index < modules_.size())
            {
                SetModule(index, status);
            }
            ::DeleteFileW(temporary.c_str());
            return false;
        }

        if (!::MoveFileExW(temporary.c_str(), targetPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            ::DeleteFileW(temporary.c_str());
            SymbolModuleStatus status = module;
            {
                std::lock_guard lock(dataMutex_);
                if (index < modules_.size())
                {
                    status = modules_[index];
                }
            }
            status.detail = L"符号文件写入失败";
            if (index < modules_.size())
            {
                SetModule(index, status);
            }
            return false;
        }

        std::wstring locatedPath;
        if (!LocateLocalPdb(identity.guid, identity.age,
            identity.fileName, locatedPath))
        {
            ::DeleteFileW(targetPath.c_str());
            SymbolModuleStatus status = module;
            {
                std::lock_guard lock(dataMutex_);
                if (index < modules_.size())
                {
                    status = modules_[index];
                }
            }
            status.detail = L"下载文件签名校验失败";
            if (index < modules_.size())
            {
                SetModule(index, status);
            }
            return false;
        }

        std::error_code sizeError;
        const std::uintmax_t fileSize =
            std::filesystem::file_size(locatedPath, sizeError);
        if (!sizeError)
        {
            UpdateDownloadProgress(index, fileSize, fileSize);
        }
        return true;
    }

    std::wstring SymbolPreparer::BuildServerBaseUrl() const
    {
        return ServerUrlFromConfig(configPath_);
    }

    void SymbolPreparer::SetModule(const size_t index, SymbolModuleStatus status)
    {
        std::lock_guard lock(dataMutex_);
        if (index < modules_.size())
        {
            modules_[index] = std::move(status);
        }
        int totalProgress{};
        for (const auto& module : modules_)
        {
            totalProgress += module.progress;
        }
        if (!modules_.empty())
        {
            SetProgress(totalProgress /
                static_cast<int>(modules_.size()));
        }
    }

    void SymbolPreparer::SetProgress(const int value) noexcept
    {
        progress_.store((std::clamp)(value, 0, 100), std::memory_order_release);
    }

    void SymbolPreparer::UpdateDownloadProgress(const size_t index,
        const std::uint64_t downloaded, const std::uint64_t total)
    {
        std::lock_guard lock(dataMutex_);
        if (index >= modules_.size())
        {
            return;
        }

        SymbolModuleStatus& module = modules_[index];
        module.state = SymbolModuleState::Downloading;
        module.downloadedBytes = downloaded;
        module.totalBytes = total;
        module.progress = total == 0 ? 0 : static_cast<int>(
            (std::min)(downloaded * 100 / total,
                static_cast<std::uint64_t>(100)));
        module.detail = total == 0
            ? L"下载中，等待服务器总大小"
            : L"下载中 " + std::to_wstring(module.progress) + L"%";

        int totalProgress{};
        for (const auto& item : modules_)
        {
            totalProgress += item.progress;
        }
        SetProgress(totalProgress /
            static_cast<int>(modules_.size()));
    }

    void SymbolPreparer::SetLastError(std::wstring value)
    {
        std::lock_guard lock(dataMutex_);
        lastError_ = std::move(value);
    }

    void SymbolPreparer::SetState(const SymbolPreparationState state) noexcept
    {
        state_.store(state, std::memory_order_release);
    }

    void SymbolPreparer::SetAnalysisState(
        const SymbolAnalysisState state) noexcept
    {
        analysisState_.store(state, std::memory_order_release);
    }
}

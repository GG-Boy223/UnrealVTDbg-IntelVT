#include "SystemInspector.h"

#include <Windows.h>
#include <intrin.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    using unrealdbg_native::ExternalLogFile;
    using unrealdbg_native::SystemDetailItem;
    using unrealdbg_native::SystemInformation;

    std::wstring ReadRegistryString(HKEY root, const wchar_t* subKey,
        const wchar_t* valueName)
    {
        DWORD type{};
        DWORD size{};
        if (::RegGetValueW(root, subKey, valueName,
            RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type, nullptr, &size) !=
                ERROR_SUCCESS ||
            size < sizeof(wchar_t))
        {
            return {};
        }

        std::wstring buffer(size / sizeof(wchar_t), L'\0');
        if (::RegGetValueW(root, subKey, valueName,
            RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, &type,
            buffer.data(), &size) != ERROR_SUCCESS)
        {
            return {};
        }
        while (!buffer.empty() && buffer.back() == L'\0')
        {
            buffer.pop_back();
        }
        return buffer;
    }

    DWORD ReadRegistryDword(HKEY root, const wchar_t* subKey,
        const wchar_t* valueName, const DWORD fallback)
    {
        DWORD value{};
        DWORD size = sizeof(value);
        return ::RegGetValueW(root, subKey, valueName, RRF_RT_REG_DWORD,
            nullptr, &value, &size) == ERROR_SUCCESS ? value : fallback;
    }

    std::wstring FormatBytes(std::uint64_t bytes)
    {
        constexpr double kKiB = 1024.0;
        constexpr double kMiB = kKiB * 1024.0;
        constexpr double kGiB = kMiB * 1024.0;
        constexpr double kTiB = kGiB * 1024.0;
        wchar_t buffer[64]{};
        if (bytes >= static_cast<std::uint64_t>(kTiB))
        {
            swprintf_s(buffer, L"%.2f TB", static_cast<double>(bytes) / kTiB);
        }
        else if (bytes >= static_cast<std::uint64_t>(kGiB))
        {
            swprintf_s(buffer, L"%.2f GB", static_cast<double>(bytes) / kGiB);
        }
        else if (bytes >= static_cast<std::uint64_t>(kMiB))
        {
            swprintf_s(buffer, L"%.1f MB", static_cast<double>(bytes) / kMiB);
        }
        else
        {
            swprintf_s(buffer, L"%.1f KB", static_cast<double>(bytes) / kKiB);
        }
        return buffer;
    }

    std::wstring FormatUnixTime(const DWORD seconds)
    {
        if (seconds == 0)
        {
            return L"不可用";
        }
        const std::time_t value = static_cast<std::time_t>(seconds);
        tm local{};
        if (localtime_s(&local, &value) != 0)
        {
            return L"不可用";
        }
        wchar_t buffer[48]{};
        swprintf_s(buffer, L"%04d-%02d-%02d %02d:%02d",
            local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
            local.tm_hour, local.tm_min);
        return buffer;
    }

    std::wstring FormatUptime()
    {
        const ULONGLONG milliseconds = ::GetTickCount64();
        const ULONGLONG totalMinutes = milliseconds / 60000ULL;
        const ULONGLONG days = totalMinutes / (24ULL * 60ULL);
        const ULONGLONG hours = (totalMinutes / 60ULL) % 24ULL;
        const ULONGLONG minutes = totalMinutes % 60ULL;
        wchar_t buffer[64]{};
        swprintf_s(buffer, L"%llu 天 %02llu:%02llu",
            days, hours, minutes);
        return buffer;
    }

    std::wstring FormatHResult(const LONG value)
    {
        wchar_t buffer[16]{};
        swprintf_s(buffer, L"0x%08X", static_cast<unsigned int>(value));
        return buffer;
    }

    DWORD CountPhysicalCores()
    {
        DWORD length = 0;
        ::GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
        if (length == 0)
        {
            return 0;
        }

        std::vector<BYTE> buffer(length);
        if (!::GetLogicalProcessorInformationEx(RelationProcessorCore,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
                buffer.data()), &length))
        {
            return 0;
        }

        DWORD count = 0;
        DWORD offset = 0;
        while (offset < length)
        {
            const auto* entry =
                reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                    buffer.data() + offset);
            if (entry->Relationship == RelationProcessorCore)
            {
                ++count;
            }
            if (entry->Size == 0)
            {
                break;
            }
            offset += entry->Size;
        }
        return count;
    }

    bool HasHypervisorPresent()
    {
        int registers[4]{};
        __cpuid(registers, 1);
        return (registers[2] & (1 << 31)) != 0;
    }

    std::wstring FirmwareTypeText()
    {
        FIRMWARE_TYPE type{};
        if (!::GetFirmwareType(&type))
        {
            return L"不可用";
        }
        switch (type)
        {
        case FirmwareTypeBios:
            return L"Legacy BIOS";
        case FirmwareTypeUefi:
            return L"UEFI";
        default:
            return L"Unknown";
        }
    }

    std::wstring SecureBootText()
    {
        const DWORD value = ReadRegistryDword(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State",
            L"UEFISecureBootEnabled", MAXDWORD);
        if (value == MAXDWORD)
        {
            return L"不适用或不可用";
        }
        return value != 0 ? L"已启用" : L"未启用";
    }

    std::vector<std::wstring> CollectGpuNames()
    {
        std::vector<std::wstring> names;
        constexpr wchar_t kDisplayClass[] =
            L"SYSTEM\\CurrentControlSet\\Control\\Class\\"
            L"{4d36e968-e325-11ce-bfc1-08002be10318}";
        HKEY key = nullptr;
        if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, kDisplayClass, 0,
            KEY_READ, &key) != ERROR_SUCCESS)
        {
            return names;
        }

        for (DWORD index = 0;; ++index)
        {
            wchar_t subKey[128]{};
            DWORD length = static_cast<DWORD>(std::size(subKey));
            const LSTATUS status = ::RegEnumKeyExW(key, index, subKey,
                &length, nullptr, nullptr, nullptr, nullptr);
            if (status == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            if (status != ERROR_SUCCESS)
            {
                continue;
            }

            const std::wstring path =
                std::wstring(kDisplayClass) + L"\\" + subKey;
            std::wstring name = ReadRegistryString(
                HKEY_LOCAL_MACHINE, path.c_str(), L"DriverDesc");
            if (name.empty())
            {
                continue;
            }
            if (std::find(names.begin(), names.end(), name) == names.end())
            {
                const std::wstring version = ReadRegistryString(
                    HKEY_LOCAL_MACHINE, path.c_str(), L"DriverVersion");
                if (!version.empty())
                {
                    name += L"  ·  " + version;
                }
                names.push_back(std::move(name));
            }
        }
        ::RegCloseKey(key);
        return names;
    }

    std::wstring CollectVolumes()
    {
        wchar_t drives[512]{};
        const DWORD length = ::GetLogicalDriveStringsW(
            static_cast<DWORD>(std::size(drives)), drives);
        if (length == 0 || length >= std::size(drives))
        {
            return L"不可用";
        }

        std::wstring result;
        for (const wchar_t* drive = drives; *drive != L'\0';
            drive += std::wcslen(drive) + 1)
        {
            if (::GetDriveTypeW(drive) != DRIVE_FIXED)
            {
                continue;
            }
            ULARGE_INTEGER freeBytes{};
            ULARGE_INTEGER totalBytes{};
            if (!::GetDiskFreeSpaceExW(drive, &freeBytes,
                &totalBytes, nullptr))
            {
                continue;
            }
            if (!result.empty())
            {
                result += L"\n";
            }
            result += drive;
            result += L"  可用 ";
            result += FormatBytes(freeBytes.QuadPart);
            result += L" / ";
            result += FormatBytes(totalBytes.QuadPart);
        }
        return result.empty() ? L"不可用" : result;
    }

    std::wstring DecodeCodePage(const BYTE* data, const int byteCount,
        const UINT codePage, const bool strict)
    {
        const DWORD flags = codePage == CP_UTF8 && strict
            ? MB_ERR_INVALID_CHARS : 0;
        const int count = ::MultiByteToWideChar(codePage, flags,
            reinterpret_cast<const char*>(data), byteCount, nullptr, 0);
        if (count <= 0)
        {
            return {};
        }
        std::wstring text(static_cast<size_t>(count), L'\0');
        if (::MultiByteToWideChar(codePage, flags,
            reinterpret_cast<const char*>(data), byteCount,
            text.data(), count) <= 0)
        {
            return {};
        }
        return text;
    }

    int DecodeQuality(const std::wstring& text)
    {
        int score = 0;
        for (const wchar_t value : text)
        {
            const unsigned int code = static_cast<unsigned int>(value);
            if (value == 0xFFFD)
            {
                score -= 120;
            }
            else if (code >= 0x4E00 && code <= 0x9FFF)
            {
                score += 3;
            }
            else if (code >= 0x3000 && code <= 0x303F)
            {
                score += 2;
            }
            else if (value < 0x20 && value != L'\r' &&
                value != L'\n' && value != L'\t')
            {
                score -= 20;
            }
            else if (code >= 0xE000 && code <= 0xF8FF)
            {
                score -= 6;
            }
        }
        return score;
    }

    std::wstring DecodeLogText(const BYTE* data, const size_t size)
    {
        if (data == nullptr || size == 0)
        {
            return {};
        }
        if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE)
        {
            return std::wstring(
                reinterpret_cast<const wchar_t*>(data + 2),
                (size - 2) / sizeof(wchar_t));
        }
        if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB &&
            data[2] == 0xBF)
        {
            return DecodeCodePage(data + 3,
                static_cast<int>(size - 3), CP_UTF8, true);
        }

        const int byteCount = static_cast<int>(size);
        std::wstring best = DecodeCodePage(data, byteCount, CP_UTF8, true);
        if (!best.empty())
        {
            const bool ascii = std::all_of(best.begin(), best.end(),
                [](const wchar_t value)
                {
                    return value < 0x80;
                });
            const int utf8Score = DecodeQuality(best);
            if (ascii || utf8Score >= 0)
            {
                return best;
            }
        }

        int bestScore = DecodeQuality(best);
        const std::wstring gbk =
            DecodeCodePage(data, byteCount, 936, false);
        const int gbkScore = DecodeQuality(gbk);
        if (!gbk.empty() && (best.empty() || gbkScore > bestScore))
        {
            return gbk;
        }
        const std::array<UINT, 4> candidates = {
            936U, ::GetOEMCP(), ::GetACP(), 54936U
        };
        for (const UINT codePage : candidates)
        {
            if (codePage == 0 || codePage == CP_UTF8 || codePage == 936)
            {
                continue;
            }
            const std::wstring candidate =
                DecodeCodePage(data, byteCount, codePage, false);
            const int score = DecodeQuality(candidate);
            if (!candidate.empty() && score > bestScore)
            {
                best = candidate;
                bestScore = score;
            }
        }

        return best;
    }

    void Add(std::vector<SystemDetailItem>& items,
        std::wstring group, std::wstring label, std::wstring value)
    {
        items.push_back(SystemDetailItem{
            std::move(group), std::move(label),
            value.empty() ? L"不可用" : std::move(value)
        });
    }
}

namespace unrealdbg_native
{
    void CollectDetailedSystemInformation(
        const SystemInformation& system,
        const std::wstring& applicationDirectory,
        std::vector<SystemDetailItem>& items)
    {
        items.clear();
        constexpr wchar_t kVersionKey[] =
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

        Add(items, L"操作系统", L"产品名称",
            ReadRegistryString(HKEY_LOCAL_MACHINE, kVersionKey, L"ProductName"));
        Add(items, L"操作系统", L"版本与 UBR",
            system.kernelVersion.empty() ? L"不可用" : system.kernelVersion);
        Add(items, L"操作系统", L"显示版本",
            ReadRegistryString(HKEY_LOCAL_MACHINE, kVersionKey,
                L"DisplayVersion"));
        Add(items, L"操作系统", L"内部版本",
            ReadRegistryString(HKEY_LOCAL_MACHINE, kVersionKey,
                L"BuildLabEx"));
        Add(items, L"操作系统", L"安装时间",
            FormatUnixTime(ReadRegistryDword(HKEY_LOCAL_MACHINE,
                kVersionKey, L"InstallDate", 0)));
        Add(items, L"操作系统", L"系统启动时间", FormatUptime());
        Add(items, L"操作系统", L"系统架构", system.architecture);

        SYSTEM_INFO native{};
        ::GetNativeSystemInfo(&native);
        const DWORD cores = CountPhysicalCores();
        Add(items, L"处理器", L"处理器",
            ReadRegistryString(HKEY_LOCAL_MACHINE,
                L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                L"ProcessorNameString"));
        Add(items, L"处理器", L"厂商",
            ReadRegistryString(HKEY_LOCAL_MACHINE,
                L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                L"VendorIdentifier"));
        Add(items, L"处理器", L"核心与线程",
            std::to_wstring(cores) + L" 物理核心 / " +
            std::to_wstring(native.dwNumberOfProcessors) + L" 逻辑处理器");
        const DWORD mhz = ReadRegistryDword(HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            L"~MHz", 0);
        Add(items, L"处理器", L"标称频率",
            mhz == 0 ? L"不可用" : std::to_wstring(mhz) + L" MHz");
        Add(items, L"处理器", L"页大小",
            std::to_wstring(native.dwPageSize) + L" 字节");
        Add(items, L"处理器", L"地址分配粒度",
            std::to_wstring(native.dwAllocationGranularity) + L" 字节");

        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        if (::GlobalMemoryStatusEx(&memory))
        {
            Add(items, L"内存", L"物理内存",
                FormatBytes(memory.ullTotalPhys));
            Add(items, L"内存", L"当前可用",
                FormatBytes(memory.ullAvailPhys));
            Add(items, L"内存", L"提交上限",
                FormatBytes(memory.ullTotalPageFile));
            Add(items, L"内存", L"提交可用",
                FormatBytes(memory.ullAvailPageFile));
            Add(items, L"内存", L"用户虚拟地址上限",
                FormatBytes(memory.ullTotalVirtual));
            Add(items, L"内存", L"内存占用",
                std::to_wstring(memory.dwMemoryLoad) + L"%");
        }

        constexpr wchar_t kBiosKey[] =
            L"HARDWARE\\DESCRIPTION\\System\\BIOS";
        Add(items, L"固件与主板", L"固件模式", FirmwareTypeText());
        Add(items, L"固件与主板", L"安全启动", SecureBootText());
        Add(items, L"固件与主板", L"Hypervisor 标志",
            HasHypervisorPresent() ? L"CPUID 报告已存在" : L"未报告");
        Add(items, L"固件与主板", L"主板厂商",
            ReadRegistryString(HKEY_LOCAL_MACHINE, kBiosKey,
                L"BaseBoardManufacturer"));
        Add(items, L"固件与主板", L"主板型号",
            ReadRegistryString(HKEY_LOCAL_MACHINE, kBiosKey,
                L"BaseBoardProduct"));
        Add(items, L"固件与主板", L"BIOS 厂商",
            ReadRegistryString(HKEY_LOCAL_MACHINE, kBiosKey,
                L"BIOSVendor"));
        Add(items, L"固件与主板", L"BIOS 版本",
            ReadRegistryString(HKEY_LOCAL_MACHINE, kBiosKey,
                L"BIOSVersion"));
        Add(items, L"固件与主板", L"BIOS 日期",
            ReadRegistryString(HKEY_LOCAL_MACHINE, kBiosKey,
                L"BIOSReleaseDate"));

        const std::vector<std::wstring> gpuNames = CollectGpuNames();
        if (gpuNames.empty())
        {
            Add(items, L"显示设备", L"GPU", L"不可用");
        }
        else
        {
            for (size_t index = 0; index < gpuNames.size(); ++index)
            {
                Add(items, L"显示设备",
                    L"GPU " + std::to_wstring(index + 1), gpuNames[index]);
            }
        }

        Add(items, L"存储", L"固定磁盘", CollectVolumes());

        wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
        if (::GetUserDefaultLocaleName(locale,
            static_cast<int>(std::size(locale))) == 0)
        {
            Add(items, L"区域与运行环境", L"用户区域", L"不可用");
        }
        else
        {
            Add(items, L"区域与运行环境", L"用户区域", locale);
        }
        TIME_ZONE_INFORMATION zone{};
        const DWORD zoneId = ::GetTimeZoneInformation(&zone);
        Add(items, L"区域与运行环境", L"时区", zone.StandardName);
        if (zoneId == TIME_ZONE_ID_DAYLIGHT)
        {
            Add(items, L"区域与运行环境", L"夏令时", zone.DaylightName);
        }
        Add(items, L"区域与运行环境", L"Windows 目录",
            ReadRegistryString(HKEY_LOCAL_MACHINE, kVersionKey,
                L"SystemRoot"));
        Add(items, L"区域与运行环境", L"运行目录",
            applicationDirectory);
        Add(items, L"区域与运行环境", L"调试台 PID",
            std::to_wstring(::GetCurrentProcessId()));
        Add(items, L"区域与运行环境", L"权限",
            system.elevated ? L"管理员 / 高完整性" : L"标准用户");
    }

    std::vector<ExternalLogFile> CollectExternalLogs(
        const SystemInformation& system,
        const std::wstring& applicationDirectory)
    {
        const std::array<std::pair<std::wstring, std::wstring>, 5> paths = {
            std::pair{ L"DbgkSys10.log", L"C:\\DbgkSys10.log" },
            std::pair{ L"DbgkSys11.log", L"C:\\DbgkSys11.log" },
            std::pair{ L"VTLoad.log", L"C:\\VTLoad.log" },
            std::pair{ L"Hook64.log",
                (std::filesystem::path(applicationDirectory) /
                    L"Hook64.log").wstring() },
            std::pair{ L"Hook.log",
                (std::filesystem::path(applicationDirectory) /
                    L"Hook.log").wstring() },
        };

        std::vector<ExternalLogFile> logs;
        logs.reserve(paths.size());
        for (const auto& [name, path] : paths)
        {
            std::error_code error;
            const bool available =
                std::filesystem::is_regular_file(path, error) && !error;
            const bool currentSystem =
                (name == L"DbgkSys10.log" && !system.windows11) ||
                (name == L"DbgkSys11.log" && system.windows11);
            logs.push_back(ExternalLogFile{
                name, path, available, currentSystem
            });
        }
        return logs;
    }

    bool ReadLogTailText(const std::wstring& path, const size_t maximumBytes,
        std::wstring& text, std::wstring& error)
    {
        text.clear();
        error.clear();
        const HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            error = L"无法打开日志（" +
                std::to_wstring(::GetLastError()) + L"）";
            return false;
        }

        LARGE_INTEGER size{};
        if (!::GetFileSizeEx(file, &size))
        {
            error = L"无法读取日志大小（" +
                std::to_wstring(::GetLastError()) + L"）";
            ::CloseHandle(file);
            return false;
        }
        if (size.QuadPart == 0)
        {
            text = L"（文件为空）";
            ::CloseHandle(file);
            return true;
        }

        const std::uint64_t fileBytes =
            static_cast<std::uint64_t>(size.QuadPart);
        const std::uint64_t readBytes =
            (std::min)(fileBytes, static_cast<std::uint64_t>(maximumBytes));
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(fileBytes - readBytes);
        if (!::SetFilePointerEx(file, position, nullptr, FILE_BEGIN))
        {
            error = L"无法定位日志尾部（" +
                std::to_wstring(::GetLastError()) + L"）";
            ::CloseHandle(file);
            return false;
        }

        std::vector<BYTE> bytes(static_cast<size_t>(readBytes));
        DWORD read = 0;
        if (!::ReadFile(file, bytes.data(),
            static_cast<DWORD>(bytes.size()), &read, nullptr))
        {
            error = L"读取日志失败（" +
                std::to_wstring(::GetLastError()) + L"）";
            ::CloseHandle(file);
            return false;
        }
        ::CloseHandle(file);
        bytes.resize(read);

        size_t textOffset = 0;
        if (fileBytes > readBytes)
        {
            while (textOffset < bytes.size() &&
                bytes[textOffset] != '\r' && bytes[textOffset] != '\n')
            {
                ++textOffset;
            }
            while (textOffset < bytes.size() &&
                (bytes[textOffset] == '\r' || bytes[textOffset] == '\n'))
            {
                ++textOffset;
            }
        }
        text = DecodeLogText(bytes.data() + textOffset,
            bytes.size() - textOffset);
        if (text.empty() && bytes.size() > textOffset)
        {
            error = L"无法识别日志文本编码";
            return false;
        }

        if (fileBytes > readBytes)
        {
            text = L"（仅显示文件末尾 " + FormatBytes(readBytes) +
                L"）\r\n" + text;
        }
        return true;
    }
}

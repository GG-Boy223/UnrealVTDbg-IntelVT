#include "SymbolsInternal.h"

#include <string_view>

namespace
{
    constexpr DWORD kRsdsSignature = 0x53445352; // "RSDS"

    struct RsdsHeader
    {
        DWORD Signature;
        GUID Guid;
        DWORD Age;
    };

    bool IsValidPdbFileName(const std::string& name)
    {
        if (name.empty() || name.size() >= MAX_PATH)
        {
            return false;
        }

        for (unsigned char c : name)
        {
            if (c < 0x21 || c > 0x7E ||
                c == '\\' || c == '/' || c == ':' ||
                c == '*' || c == '?' || c == '"' ||
                c == '<' || c == '>' || c == '|')
            {
                return false;
            }
        }

        return true;
    }

    bool RvaToFileOffset(const BYTE* fileData, size_t fileSize,
        const IMAGE_SECTION_HEADER* sections, WORD sectionCount,
        DWORD rva, size_t* offset)
    {
        if (!offset || !rva)
        {
            return false;
        }

        for (WORD i = 0; i < sectionCount; ++i)
        {
            const IMAGE_SECTION_HEADER& section = sections[i];
            const DWORD delta = section.VirtualAddress <= rva
                ? rva - section.VirtualAddress
                : UINT32_MAX;

            if (delta >= section.SizeOfRawData)
            {
                continue;
            }

            const size_t rawBase = section.PointerToRawData;
            if (rawBase > fileSize ||
                section.SizeOfRawData > fileSize - rawBase)
            {
                return false;
            }

            const size_t rawOffset = rawBase + delta;
            if (rawOffset >= fileSize)
            {
                return false;
            }

            *offset = rawOffset;
            return true;
        }

        return false;
    }
}

// 下载PDB文件
bool DownloadSymbol_internal(const std::wstring& szModulePath, const std::wstring& path, std::wstring* pdb_path_out, bool Redownload)
{
    if (szModulePath.empty() || path.empty())
    {
        Common::ReportSeriousError("%s[%d] 参数错误! (error: %d)", __func__, __LINE__, ERROR_INVALID_PARAMETER);
        return false;
    }

    std::ifstream File(szModulePath, std::ios::binary | std::ios::ate);
    if (!File.good())
    {
        Common::ReportSeriousError("%s[%d] 打开文件失败! (error: %d)", __func__, __LINE__, GetLastError());
        return false;
    }

    const std::streamoff FileSize = File.tellg();
    if (FileSize <= 0)
    {
        Common::ReportSeriousError("%s[%d] 获取文件大小失败! (error: %d)", __func__, __LINE__, GetLastError());
        return false;
    }

    const size_t fileSize = static_cast<size_t>(FileSize);
    std::unique_ptr<BYTE[]> pRawData(new(std::nothrow) BYTE[fileSize]);
    if (!pRawData)
    {
        Common::ReportSeriousError("%s[%d] 分配内存失败! (error: %d)", __func__, __LINE__, GetLastError());
        return false;
    }

    File.seekg(0, std::ios::beg);
    File.read(reinterpret_cast<char*>(pRawData.get()), FileSize);
    const bool readOk = File.gcount() == FileSize;
    File.close();
    if (!readOk)
    {
        Common::ReportSeriousError("%s[%d] 读取文件失败! (error: %d)", __func__, __LINE__, GetLastError());
        return false;
    }

    const BYTE* fileBytes = pRawData.get();
    if (fileSize < sizeof(IMAGE_DOS_HEADER))
    {
        Common::ReportSeriousError("%s[%d] PE文件不完整! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    const auto* pDos = reinterpret_cast<const IMAGE_DOS_HEADER*>(fileBytes);
    if (pDos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        Common::ReportSeriousError("%s[%d] PE签名错误! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    const LONG peOffset = pDos->e_lfanew;
    if (peOffset <= 0 || static_cast<size_t>(peOffset) > fileSize - sizeof(IMAGE_NT_HEADERS32))
    {
        Common::ReportSeriousError("%s[%d] PE头偏移错误! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    const auto* pNt32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(fileBytes + peOffset);
    if (pNt32->Signature != IMAGE_NT_SIGNATURE)
    {
        Common::ReportSeriousError("%s[%d] PE签名错误! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    const IMAGE_DATA_DIRECTORY* pDebugDir = nullptr;
    const IMAGE_SECTION_HEADER* sections = nullptr;
    WORD sectionCount = pNt32->FileHeader.NumberOfSections;
    WORD optionalHeaderSize = pNt32->FileHeader.SizeOfOptionalHeader;
    const size_t optionalOffset = peOffset + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader);

    if (pNt32->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
    {
        if (fileSize < optionalOffset + sizeof(IMAGE_OPTIONAL_HEADER64))
        {
            Common::ReportSeriousError("%s[%d] PE可选头不完整! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
            return false;
        }

        const auto* pNt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(fileBytes + peOffset);
        if (pNt64->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            pNt64->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
        {
            Common::ReportSeriousError("%s[%d] PE64可选头错误! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
            return false;
        }

        pDebugDir = &pNt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        optionalHeaderSize = pNt64->FileHeader.SizeOfOptionalHeader;
    }
    else if (pNt32->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
    {
        if (fileSize < optionalOffset + sizeof(IMAGE_OPTIONAL_HEADER32))
        {
            Common::ReportSeriousError("%s[%d] PE可选头不完整! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
            return false;
        }

        if (pNt32->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
            pNt32->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32))
        {
            Common::ReportSeriousError("%s[%d] PE32可选头错误! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
            return false;
        }

        pDebugDir = &pNt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        optionalHeaderSize = pNt32->FileHeader.SizeOfOptionalHeader;
    }
    else
    {
        Common::ReportSeriousError("%s[%d] 不支持的PE架构! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    if (!sectionCount)
    {
        Common::ReportSeriousError("%s[%d] PE节表为空! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    const size_t sectionsOffset = optionalOffset + optionalHeaderSize;
    const size_t sectionBytes = static_cast<size_t>(sectionCount) * sizeof(IMAGE_SECTION_HEADER);
    if (sectionsOffset > fileSize || sectionBytes > fileSize - sectionsOffset)
    {
        Common::ReportSeriousError("%s[%d] PE节表不完整! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(fileBytes + sectionsOffset);
    if (!pDebugDir || !pDebugDir->VirtualAddress || pDebugDir->Size < sizeof(IMAGE_DEBUG_DIRECTORY))
    {
        Common::ReportSeriousError("%s[%d] 未找到调试目录! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    size_t debugOffset = 0;
    if (!RvaToFileOffset(fileBytes, fileSize, sections, sectionCount, pDebugDir->VirtualAddress, &debugOffset) ||
        debugOffset > fileSize || pDebugDir->Size > fileSize - debugOffset)
    {
        Common::ReportSeriousError("%s[%d] 调试目录越界! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    const DWORD debugEntryCount = pDebugDir->Size / sizeof(IMAGE_DEBUG_DIRECTORY);
    std::string pdbFileName;
    GUID guid = {};
    DWORD age = 0;
    DWORD timeDateStamp = 0;
    bool found = false;

    for (DWORD i = 0; i < debugEntryCount; ++i)
    {
        const auto* debugEntry = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(
            fileBytes + debugOffset + static_cast<size_t>(i) * sizeof(IMAGE_DEBUG_DIRECTORY));

        if (debugEntry->Type != IMAGE_DEBUG_TYPE_CODEVIEW)
        {
            continue;
        }

        size_t rsdsOffset = 0;
        if (debugEntry->AddressOfRawData &&
            !RvaToFileOffset(fileBytes, fileSize, sections, sectionCount,
                debugEntry->AddressOfRawData, &rsdsOffset))
        {
            continue;
        }

        if (!rsdsOffset)
        {
            const size_t rawPointer = debugEntry->PointerToRawData;
            if (rawPointer >= fileSize)
            {
                continue;
            }
            rsdsOffset = rawPointer;
        }

        size_t dataSize = debugEntry->SizeOfData;
        if (dataSize < sizeof(RsdsHeader))
        {
            continue;
        }
        if (rsdsOffset >= fileSize || dataSize > fileSize - rsdsOffset)
        {
            continue;
        }

        RsdsHeader header = {};
        memcpy(&header, fileBytes + rsdsOffset, sizeof(header));
        if (header.Signature != kRsdsSignature)
        {
            continue;
        }

        const size_t nameOffset = rsdsOffset + sizeof(RsdsHeader);
        const size_t remaining = fileSize - nameOffset;
        size_t nameLength = 0;
        while (nameLength < remaining &&
            fileBytes[nameOffset + nameLength] != '\0')
        {
            ++nameLength;
        }

        if (nameLength == 0 || nameLength == remaining)
        {
            continue;
        }

        std::string currentName(
            reinterpret_cast<const char*>(fileBytes + nameOffset), nameLength);
        if (!IsValidPdbFileName(currentName))
        {
            continue;
        }

        pdbFileName = std::move(currentName);
        guid = header.Guid;
        age = header.Age;
        timeDateStamp = debugEntry->TimeDateStamp;
        found = true;
        break;
    }

    if (!found)
    {
        Common::ReportSeriousError("%s[%d] 未找到RSDS信息! (error: %d)", __func__, __LINE__, ERROR_INVALID_DATA);
        return false;
    }

    std::wstring pdbFileNameW;
    const int wideLength = MultiByteToWideChar(CP_ACP, 0,
        pdbFileName.c_str(), static_cast<int>(pdbFileName.size()), nullptr, 0);
    if (wideLength <= 0)
    {
        Common::ReportSeriousError("%s[%d] 转换Unicode字符串失败! (error: %d)", __func__, __LINE__, GetLastError());
        return false;
    }

    pdbFileNameW.resize(static_cast<size_t>(wideLength));
    if (MultiByteToWideChar(CP_ACP, 0,
        pdbFileName.c_str(), static_cast<int>(pdbFileName.size()),
        &pdbFileNameW[0], wideLength) != wideLength)
    {
        Common::ReportSeriousError("%s[%d] 转换Unicode字符串失败! (error: %d)", __func__, __LINE__, GetLastError());
        return false;
    }

    g_szPdbPath = path;
    if (g_szPdbPath.empty() || g_szPdbPath.back() != L'\\')
    {
        g_szPdbPath += L'\\';
    }

    if (!CreateDirectoryIfNotExists(g_szPdbPath.c_str()))
    {
        Common::ReportSeriousError("%s[%d] 创建目录失败! (error: %d)", __func__, __LINE__, GetLastError());
        return false;
    }

    g_szPdbPath += pdbFileNameW;
    if (!CreateDirectoryIfNotExists(g_szPdbPath.c_str()))
    {
        Common::ReportSeriousError("%s[%d] 创建目录失败! (error: %d)", __func__, __LINE__, GetLastError());
        return false;
    }

    if (Redownload)
    {
        wchar_t guidWide[100] = {};
        if (!StringFromGUID2(guid, guidWide, _countof(guidWide)))
        {
            Common::ReportSeriousError("%s[%d] 获取Guid失败! (error: %d)", __func__, __LINE__, GetLastError());
            return false;
        }

        std::wstring guidFiltered;
        for (const wchar_t& ch : guidWide)
        {
            if ((ch >= L'0' && ch <= L'9') ||
                (ch >= L'A' && ch <= L'F') ||
                (ch >= L'a' && ch <= L'f'))
            {
                guidFiltered += ch;
            }
        }

        const std::wstring ageWide = std::to_wstring(age);
        const std::wstring filename = FileSystem::GetModuleDirectory(NULL) + L"SymbolTool\\Config.ini";
        const std::string baseUrl = BuildSymbolServerBaseUrl(filename);
        std::wstring url = Common::stringToWideString(baseUrl);
        url += pdbFileNameW;
        url += L'/';
        url += guidFiltered;
        url += ageWide;
        url += L'/';
        url += pdbFileNameW;

        if (g_szPdbPath.empty() || g_szPdbPath.back() != L'\\')
        {
            g_szPdbPath += L'\\';
        }
        g_szPdbPath += guidFiltered;
        g_szPdbPath += ageWide;

        if (!CreateDirectoryIfNotExists(g_szPdbPath.c_str()))
        {
            Common::ReportSeriousError("%s[%d] 创建目录失败! (error: %d)", __func__, __LINE__, GetLastError());
            return false;
        }

        if (g_szPdbPath.empty() || g_szPdbPath.back() != L'\\')
        {
            g_szPdbPath += L'\\';
        }
        g_szPdbPath += pdbFileNameW;

        const HRESULT hr = URLDownloadToFileW(nullptr, url.c_str(),
            g_szPdbPath.c_str(), NULL, &callback);
        if (FAILED(hr))
        {
            Common::ReportSeriousError("下载符号表失败,系统无法找到指定的资源! "
                "请按照说明书操作使用第三方工具进行下载。(error: 0x%08X)", static_cast<unsigned>(hr));
            return false;
        }
    }

    if (pdb_path_out)
    {
        *pdb_path_out = g_szPdbPath;
    }

    return true;
}

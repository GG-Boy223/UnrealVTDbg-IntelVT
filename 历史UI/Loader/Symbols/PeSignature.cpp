#include "SymbolsInternal.h"

namespace
{
    struct RsdsHeader
    {
        DWORD Sig;
        GUID Guid;
        DWORD Age;
    };

    bool ByteRangeWithin(const BYTE* base, size_t mappedSize,
        const void* pointer, size_t size)
    {
        if (!base || !pointer)
        {
            return false;
        }

        const BYTE* p = static_cast<const BYTE*>(pointer);
        if (p < base)
        {
            return false;
        }

        const size_t offset = static_cast<size_t>(p - base);
        return offset <= mappedSize && size <= mappedSize - offset;
    }
}

//---------- 工具:从 PE 文件里读 PDB 签名 ----------
bool GetPePdbSig(const std::wstring& pePath, GUID* guid, DWORD* age, DWORD* ts)
{
    if (pePath.empty() || !guid || !age || !ts)
    {
        return false;
    }

    HANDLE hFile = CreateFileW(pePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap)
    {
        CloseHandle(hFile);
        return false;
    }

    BYTE* base = static_cast<BYTE*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
    if (!base)
    {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    BOOL ok = FALSE;
    __try
    {
        LARGE_INTEGER fileSize = {};
        if (!GetFileSizeEx(hFile, &fileSize) ||
            fileSize.QuadPart < sizeof(IMAGE_DOS_HEADER))
        {
            __leave;
        }

        const size_t mappedSize = static_cast<size_t>(fileSize.QuadPart);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
            dos->e_lfanew <= 0)
        {
            __leave;
        }

        const size_t ntOffset = static_cast<size_t>(dos->e_lfanew);
        if (ntOffset > mappedSize - sizeof(IMAGE_NT_HEADERS32))
        {
            __leave;
        }

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + ntOffset);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
        {
            __leave;
        }

        const IMAGE_DATA_DIRECTORY* debugDir = nullptr;
        const IMAGE_OPTIONAL_HEADER* optional = &nt->OptionalHeader;
        if (optional->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        {
            if (!ByteRangeWithin(base, mappedSize, optional, sizeof(IMAGE_OPTIONAL_HEADER64)))
            {
                __leave;
            }
            debugDir = &reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt)->
                OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        }
        else if (optional->Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        {
            if (!ByteRangeWithin(base, mappedSize, optional, sizeof(IMAGE_OPTIONAL_HEADER32)))
            {
                __leave;
            }
            debugDir = &reinterpret_cast<const IMAGE_NT_HEADERS32*>(nt)->
                OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        }
        else
        {
            __leave;
        }

        if (!debugDir->VirtualAddress || !debugDir->Size ||
            debugDir->Size < sizeof(IMAGE_DEBUG_DIRECTORY))
        {
            __leave;
        }

        const DWORD sectionCount = nt->FileHeader.NumberOfSections;
        if (!sectionCount)
        {
            __leave;
        }

        const size_t optionalSize = nt->FileHeader.SizeOfOptionalHeader;
        if (optionalSize < sizeof(IMAGE_OPTIONAL_HEADER32))
        {
            __leave;
        }

        const size_t sectionOffset = ntOffset + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) +
            optionalSize;
        const size_t sectionBytes = static_cast<size_t>(sectionCount) * sizeof(IMAGE_SECTION_HEADER);
        if (sectionOffset > mappedSize ||
            sectionBytes > mappedSize - sectionOffset)
        {
            __leave;
        }

        const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(base + sectionOffset);
        size_t debugRaw = 0;
        for (DWORD i = 0; i < sectionCount; ++i)
        {
            const IMAGE_SECTION_HEADER& section = sections[i];
            if (debugDir->VirtualAddress >= section.VirtualAddress &&
                debugDir->VirtualAddress - section.VirtualAddress < section.SizeOfRawData)
            {
                const size_t rawBase = section.PointerToRawData;
                const size_t delta =
                    static_cast<size_t>(debugDir->VirtualAddress - section.VirtualAddress);
                if (rawBase > mappedSize ||
                    section.SizeOfRawData > mappedSize - rawBase ||
                    delta >= section.SizeOfRawData)
                {
                    break;
                }
                debugRaw = rawBase + delta;
                break;
            }
        }

        if (!debugRaw || debugDir->Size > mappedSize - debugRaw)
        {
            __leave;
        }

        const auto* debugEntries = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(
            base + debugRaw);
        const DWORD debugCount = debugDir->Size / sizeof(IMAGE_DEBUG_DIRECTORY);
        for (DWORD i = 0; i < debugCount; ++i)
        {
            const IMAGE_DEBUG_DIRECTORY& entry = debugEntries[i];
            if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW)
            {
                continue;
            }

            const size_t cvSize = entry.SizeOfData;
            if (cvSize < sizeof(RsdsHeader))
            {
                continue;
            }

            size_t cvRaw = static_cast<size_t>(entry.PointerToRawData);
            if (!cvRaw && entry.AddressOfRawData)
            {
                for (DWORD j = 0; j < sectionCount; ++j)
                {
                    const IMAGE_SECTION_HEADER& section = sections[j];
                    if (entry.AddressOfRawData >= section.VirtualAddress &&
                        entry.AddressOfRawData - section.VirtualAddress < section.SizeOfRawData)
                    {
                        const size_t rawBase = section.PointerToRawData;
                        const size_t delta = static_cast<size_t>(
                            entry.AddressOfRawData - section.VirtualAddress);
                        if (rawBase <= mappedSize &&
                            section.SizeOfRawData <= mappedSize - rawBase &&
                            delta < section.SizeOfRawData)
                        {
                            cvRaw = rawBase + delta;
                        }
                        break;
                    }
                }
            }

            if (!cvRaw || cvRaw >= mappedSize || cvSize > mappedSize - cvRaw)
            {
                continue;
            }

            const auto* cv = reinterpret_cast<const RsdsHeader*>(base + cvRaw);
            if (cv->Sig != 'SDSR')
            {
                continue;
            }

            *guid = cv->Guid;
            *age = cv->Age;
            *ts = entry.TimeDateStamp;
            ok = TRUE;
            __leave;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = FALSE;
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return ok == TRUE;
}

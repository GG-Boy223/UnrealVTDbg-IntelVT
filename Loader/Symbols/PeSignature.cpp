#include "SymbolsInternal.h"

//---------- 工具：从 PE 文件里读 PDB 签名 ----------
// 从 PE 文件里读 PDB 签名
bool GetPePdbSig(const std::wstring& pePath, GUID* guid, DWORD* age, DWORD* ts)
{
    HANDLE hFile = CreateFileW(pePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        // 处理打开进程失败的情况
        return false;
    }

    HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    void* base = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    bool ok = false;
    __try
    {
        auto* dos = (PIMAGE_DOS_HEADER)base;
        auto* nt = (PIMAGE_NT_HEADERS)((BYTE*)base + dos->e_lfanew);
        DWORD dbgRva = 0;
        DWORD dbgSize = 0;

        if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        {
            auto& opt64 = ((PIMAGE_NT_HEADERS64)nt)->OptionalHeader;
            dbgRva = opt64.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
            dbgSize = opt64.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
        }
        else
        {
            auto& opt32 = ((PIMAGE_NT_HEADERS32)nt)->OptionalHeader;
            dbgRva = opt32.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
            dbgSize = opt32.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
        }

        if (!dbgRva || !dbgSize)
        {
            __leave;
        }

        auto* sec = IMAGE_FIRST_SECTION(nt);
        DWORD raw = 0;
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
        {
            if (dbgRva >= sec->VirtualAddress &&
                dbgRva < sec->VirtualAddress + sec->SizeOfRawData)
            {
                raw = sec->PointerToRawData + (dbgRva - sec->VirtualAddress);
                break;
            }
        }
        if (!raw)
        {
            __leave;
        }

        auto* dbg = (PIMAGE_DEBUG_DIRECTORY)((BYTE*)base + raw);
        DWORD cnt = dbgSize / sizeof(IMAGE_DEBUG_DIRECTORY);
        for (DWORD i = 0; i < cnt; ++i, ++dbg)
        {
            if (dbg->Type == IMAGE_DEBUG_TYPE_CODEVIEW)
            {
                struct CVPDB70
                {
                    DWORD Sig;
                    GUID Guid;
                    DWORD Age;
                    char Name[1];
                } *cv = (CVPDB70*)((BYTE*)base + dbg->PointerToRawData);
                if (cv->Sig == 'SDSR')
                {
                    *guid = cv->Guid;
                    *age = cv->Age;
                    *ts = dbg->TimeDateStamp;
                    // *guid = cv->Guid;
                    // *age = cv->Age;
                    // *ts = dbg->TimeDateStamp;
                    ok = true;
                    __leave;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return ok;
}
#include "SymbolsInternal.h"

bool CreateDirectoryIfNotExists(const TCHAR* dir)
{
    if (CreateDirectory(dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return true;
    }
    else
    {
        return false;
    }
}

std::wstring GetFileName(const std::wstring& full)
{
    size_t pos = full.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? full : full.substr(pos + 1);
}

bool FileExists(const std::wstring& path)
{
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) &&
        !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

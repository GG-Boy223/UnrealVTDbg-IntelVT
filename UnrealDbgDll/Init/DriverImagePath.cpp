#include "DriverImagePath.h"

#include <algorithm>

namespace DriverImagePath
{
    std::wstring Normalize(std::wstring path)
    {
        std::replace(path.begin(), path.end(), L'/', L'\\');

        constexpr wchar_t ntPrefix[] = L"\\??\\";
        constexpr size_t ntPrefixLength = _countof(ntPrefix) - 1;
        if (_wcsnicmp(path.c_str(), ntPrefix, ntPrefixLength) == 0)
        {
            path.erase(0, ntPrefixLength);
        }

        constexpr wchar_t win32Prefix[] = L"\\\\?\\";
        constexpr size_t win32PrefixLength = _countof(win32Prefix) - 1;
        if (_wcsnicmp(path.c_str(), win32Prefix, win32PrefixLength) == 0)
        {
            path.erase(0, win32PrefixLength);
        }

        return path;
    }

    bool Same(const std::wstring& left, const std::wstring& right)
    {
        std::wstring normalizedLeft = Normalize(left);
        std::wstring normalizedRight = Normalize(right);
        return _wcsicmp(normalizedLeft.c_str(), normalizedRight.c_str()) == 0;
    }

    std::wstring ToNativeImagePath(const std::wstring& path)
    {
        std::wstring nativePath = L"\\??\\";
        nativePath += Normalize(path);
        return nativePath;
    }
}

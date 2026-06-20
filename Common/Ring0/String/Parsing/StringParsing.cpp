#include <ntifs.h>
#include "../StringHandler.h"

namespace
{
    constexpr SIZE_T kDefaultSplitStringChars = 256;

    bool CopyUnicodeSubstring(OUT WCHAR* dest, IN SIZE_T destChars, IN const WCHAR* source, IN SIZE_T sourceChars)
    {
        if (dest == nullptr || destChars == 0 || source == nullptr)
        {
            return false;
        }

        dest[0] = L'\0';
        if (sourceChars == 0 || sourceChars >= destChars)
        {
            return false;
        }

        RtlCopyMemory(dest, source, sourceChars * sizeof(WCHAR));
        dest[sourceChars] = L'\0';
        return true;
    }
}

WCHAR* SplitStringSized(WCHAR* SourceString, WCHAR* DestString, SIZE_T DestChars, WCHAR Delimiter)
{
    if (SourceString == NULL || DestString == NULL || DestChars == 0)
    {
        return NULL;
    }

    DestString[0] = _T('\0');

    const SIZE_T sourceLength = GetStrLen(SourceString);
    if (sourceLength == 0)
    {
        return NULL;
    }

    SIZE_T tokenLength = 0;
    while (tokenLength < sourceLength && SourceString[tokenLength] != Delimiter)
    {
        ++tokenLength;
    }

    if (tokenLength != 0 && !CopyUnicodeSubstring(DestString, DestChars, SourceString, tokenLength))
    {
        return NULL;
    }

    if (tokenLength == sourceLength)
    {
        return NULL;
    }

    SourceString[tokenLength] = _T('\0');
    WCHAR* nextToken = SourceString + tokenLength + 1;
    if (*nextToken == _T('\0'))
    {
        return NULL;
    }

    return nextToken;
}

WCHAR* SplitString(WCHAR* SourceString, WCHAR* DestString, WCHAR Delimiter)
{
    return SplitStringSized(SourceString, DestString, kDefaultSplitStringChars, Delimiter);
}

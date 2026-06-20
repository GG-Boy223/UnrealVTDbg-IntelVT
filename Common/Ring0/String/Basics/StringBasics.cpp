#include <ntifs.h>
#include "../StringHandler.h"

SIZE_T GetStrLen(WCHAR* _Str)
{
    SIZE_T str_len = 0;
    if (_Str == NULL)
    {
        return 0;
    }

    for (;;)
    {
        if (*_Str == _T('\0'))
        {
            break;
        }
        _Str++;
        str_len++;
    }
    return str_len;
}

VOID StrCopy(WCHAR* _Dest, WCHAR* _Source)
{
    RtlCopyMemory(_Dest, _Source, GetStrLen(_Source) * sizeof(WCHAR));
}

VOID StrCopy2(WCHAR* _Dest, PUNICODE_STRING _Source)
{
    RtlCopyMemory(_Dest, _Source->Buffer, _Source->Length);
}

BOOLEAN StrIsValid(WCHAR* _Str)
{
    if (_Str)
    {
        if (*_Str == _T('\0'))
        {
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOLEAN StrIsValid2(UNICODE_STRING filePath)
{
    if (filePath.Length == 0)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

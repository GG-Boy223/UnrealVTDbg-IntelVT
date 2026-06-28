#include <ntifs.h>
#include "../StringHandler.h"

//获取字符串长度   返回字符个数
SIZE_T GetStrLen(WCHAR* _Str)
{
    SIZE_T str_len = 0;
    if (_Str == NULL)
    {
        //DbgBreakPoint();
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

//字符串拷贝
VOID StrCopy(WCHAR* _Dest, WCHAR* _Source)
{
    RtlCopyMemory(_Dest, _Source, GetStrLen(_Source) * sizeof(WCHAR));
}

VOID StrCopy2(WCHAR* _Dest, PUNICODE_STRING _Source)
{
    RtlCopyMemory(_Dest, _Source->Buffer, _Source->Length);
}

//判断字符串是否为空
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
    // /* Fail */
    return FALSE;
}

//判断UNICODE字符串是否为空
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

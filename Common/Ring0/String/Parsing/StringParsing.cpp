#include <ntifs.h>
#include "../StringHandler.h"

namespace
{
    constexpr SIZE_T kDefaultSplitStringChars = 256;

    bool CopyUnicodeSubstring(OUT WCHAR* dest, IN SIZE_T destChars, IN const WCHAR* source, IN SIZE_T sourceChars)
    {
        if (dest == nullptr || destChars == 0 || source == nullptr)
        {
            //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
            //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
            return false;
        }

        dest[0] = L'\0';
        if (sourceChars == 0 || sourceChars >= destChars)
        {
            //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
            //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
            return false;
        }

        RtlCopyMemory(dest, source, sourceChars * sizeof(WCHAR));
        dest[sourceChars] = L'\0';
        return true;
    }
}

//字符串分割
//WCHAR* SplitString(WCHAR* SourceString, WCHAR* DestString, WCHAR Delimiter)
//{
//    //WCHAR Buffer[1024] = { 0 };
//    WCHAR* CurrentChar = SourceString;
//
//    //StrCopy(Buffer, SourceString);
//    if (SourceString == NULL || DestString == NULL)
//        return NULL;
//    for (ULONG i = 0; i < GetStrLen(SourceString); i++)
//    {
//        // 找到分隔符
//        if (*CurrentChar == Delimiter)
//        {
//            *CurrentChar = _T('\0');
//            StrCopy(DestString, SourceString);
//            break;
//        }
//        // 移动到下一个字符
//        CurrentChar++;
//    }
//    ++CurrentChar;
//    if (*CurrentChar == _T('\0'))
//    return CurrentChar;
//}
WCHAR* SplitStringSized(WCHAR* SourceString, WCHAR* DestString, SIZE_T DestChars, WCHAR Delimiter)
{
    if (SourceString == NULL || DestString == NULL || DestChars == 0)
    {
        ////判断当前调用者是否是调试器进程
        //if (!IsDebugger(PsGetCurrentProcess()))
        //{
        //    //根据窗口句柄获得窗口对象
        //    PWND pwnd = Sys_ValidateHwnd(hwnd);
        //    if (pwnd)
        //    {
        //        //根据线程对象获得进程对象
        //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
        //    }
        //}
        //NtUserQueryWindow(hwnd, WindowInfo);
        return NULL; // CR3 无效时无法转换
    }

    DestString[0] = _T('\0');

    const SIZE_T sourceLength = GetStrLen(SourceString);
    if (sourceLength == 0)
    {
        ////判断当前调用者是否是调试器进程
        //if (!IsDebugger(PsGetCurrentProcess()))
        //{
        //    //根据窗口句柄获得窗口对象
        //    PWND pwnd = Sys_ValidateHwnd(hwnd);
        //    if (pwnd)
        //    {
        //        //根据线程对象获得进程对象
        //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
        //    }
        //}
        //NtUserQueryWindow(hwnd, WindowInfo);
        return NULL; // CR3 无效时无法转换
    }

    SIZE_T tokenLength = 0;
    while (tokenLength < sourceLength && SourceString[tokenLength] != Delimiter)
    {
        ++tokenLength;
    }

    if (tokenLength != 0 && !CopyUnicodeSubstring(DestString, DestChars, SourceString, tokenLength))
    {
        ////判断当前调用者是否是调试器进程
        //if (!IsDebugger(PsGetCurrentProcess()))
        //{
        //    //根据窗口句柄获得窗口对象
        //    PWND pwnd = Sys_ValidateHwnd(hwnd);
        //    if (pwnd)
        //    {
        //        //根据线程对象获得进程对象
        //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
        //    }
        //}
        //NtUserQueryWindow(hwnd, WindowInfo);
        return NULL; // CR3 无效时无法转换
    }

    if (tokenLength == sourceLength)
    {
        ////判断当前调用者是否是调试器进程
        //if (!IsDebugger(PsGetCurrentProcess()))
        //{
        //    //根据窗口句柄获得窗口对象
        //    PWND pwnd = Sys_ValidateHwnd(hwnd);
        //    if (pwnd)
        //    {
        //        //根据线程对象获得进程对象
        //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
        //    }
        //}
        //NtUserQueryWindow(hwnd, WindowInfo);
        return NULL; // CR3 无效时无法转换
    }

    SourceString[tokenLength] = _T('\0');
    WCHAR* nextToken = SourceString + tokenLength + 1;
    if (*nextToken == _T('\0'))
    {
        ////判断当前调用者是否是调试器进程
        //if (!IsDebugger(PsGetCurrentProcess()))
        //{
        //    //根据窗口句柄获得窗口对象
        //    PWND pwnd = Sys_ValidateHwnd(hwnd);
        //    if (pwnd)
        //    {
        //        //根据线程对象获得进程对象
        //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
        //    }
        //}
        //NtUserQueryWindow(hwnd, WindowInfo);
        return NULL; // CR3 无效时无法转换
    }

    return nextToken;
}

WCHAR* SplitString(WCHAR* SourceString, WCHAR* DestString, WCHAR Delimiter)
{
    return SplitStringSized(SourceString, DestString, kDefaultSplitStringChars, Delimiter);
}

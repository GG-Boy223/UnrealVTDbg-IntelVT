#pragma once

#ifndef _STRING_HANDLER_H
#define _STRING_HANDLER_H

#define __T(x)      L ## x
#define _T(x)       __T(x)

//获取字符串长度   返回字符个数
SIZE_T GetStrLen(WCHAR* _Str);

//字符串拷贝
VOID StrCopy(WCHAR* _Dest, WCHAR* _Source);
VOID StrCopy2(WCHAR* _Dest, PUNICODE_STRING _Source);

//分配池内存
PVOID MemAllocate(IN SIZE_T Size, IN BOOLEAN Paged, IN ULONG Tag);

//释放池内存
VOID FreeMemAllocate(IN PVOID Ptr, IN ULONG Tag);

//判断字符串是否为空
BOOLEAN StrIsValid(WCHAR* _Str);

//判断UNICODE字符串是否为空
BOOLEAN StrIsValid2(UNICODE_STRING filePath);

//获取文件名称
bool GetModuleFileName(OUT WCHAR* fileName, IN PUNICODE_STRING filePath);
bool GetModuleFileNameSized(OUT WCHAR* fileName, IN SIZE_T fileNameChars, IN PUNICODE_STRING filePath);

//搜索子字符串
BOOLEAN RtlUnicodeStringContains(PUNICODE_STRING Str, PUNICODE_STRING SubStr, BOOLEAN CaseInsensitive);

//字符串分割
WCHAR* SplitStringSized(WCHAR* SourceString, WCHAR* DestString, SIZE_T DestChars, WCHAR Delimiter);
WCHAR* SplitString(WCHAR* SourceString, WCHAR* DestString, WCHAR Delimiter);

//获取进程名
NTSTATUS GetProcessName(IN PEPROCESS Process, OUT WCHAR* fileName);


#endif // !_STRING_HANDLER_H

#pragma once

#ifndef _INIT_WIN32K_FULL_H
#define _INIT_WIN32K_FULL_H


//API FindWindowA/W, FindWindowExA/W
HWND NewNtUserFindWindowEx(IN HWND hwndParent,
    IN HWND hwndChild,
    IN PUNICODE_STRING pstrClassName,
    IN PUNICODE_STRING pstrWindowName);


//API WindowFromPoint
HWND NewNtUserWindowFromPoint(POINT Point);

//API SetWindowsHookEx
PVOID NewNtUserSetWindowsHookEx(
    IN PVOID Mod,
    IN PUNICODE_STRING UnsafeModuleName,
    IN ULONG ThreadId,
    IN int HookId,
    IN PVOID HookProc,
    IN int Ansi);


#endif // !_INIT_WIN32K_FULL_H

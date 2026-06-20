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


#endif // !_INIT_WIN32K_FULL_H

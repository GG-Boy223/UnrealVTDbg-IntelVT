#pragma once

#ifndef _WIN_MAIN_H
#define _WIN_MAIN_H

#include <Windows.h>
#include <tchar.h>
#include <DbgHelp.h>
#include <psapi.h>
#include <shlwapi.h>
#include <string>
#include <TlHelp32.h>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <fstream>
#include <process.h>
#include <assert.h>
#include <map>
#include <sstream>
#include <mutex>
#include <strsafe.h>
#include <intrin.h>
#include <winternl.h>
#include "../../Common/Common.h"
#include "../../Common/FileSystem/FileSystem.h"
#include "../../Common/Hash/MD5/MD5.h"

#define USER_PROGRESS_BAR  WM_USER + 1

//启动信息
typedef struct _STARTUP_INFO
{
	TCHAR szExe[256];
	TCHAR sPath[256];
	TCHAR sCommandLine[256];
}STARTUP_INFO, * PSTARTUP_INFO;



void InitThread();

extern HWND g_hwnd;

#endif // !_WIN_MAIN_H

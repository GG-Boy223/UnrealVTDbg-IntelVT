#pragma once

#ifndef _DLLMAIN_H
#define _DLLMAIN_H

#include <Windows.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <process.h>
#include <fstream>
#include <string>
#include <ctime>
#include <TlHelp32.h>
#include <assert.h>
#include <sstream>
#include <mutex>
#include <map>
#include <algorithm>
#include <winternl.h>
#include "../Common/VMProtect/VMProtectSDK.h"
#include "../Common/Common.h"
#include "../Common/FileSystem/FileSystem.h"
#include "../Common/Encrypt/Blowfish/Blowfish.h"
#include "../Common/Shared/SharedStruct.h"
#include "../Common/Shared/IOCTLs.h"
#include "../Common/Logger/Logger.h"

BOOL InitSymbolsTable(ULONG64 key);

//¼ÓÔØ·ûºÅ±í
BOOL LoadSymbolsTable(ULONG64 key);

extern Logger logger;

#endif // !_DLLMAIN_H
#pragma once

#ifndef _DLLMAIN_H
#define _DLLMAIN_H

#include "../ntdll.h"
#include <DbgHelp.h>
#include <psapi.h>
#include <shlwapi.h>
#include <tchar.h>
#include <assert.h>
#include <string>
#include <vector>
#include <process.h>
#include <sstream>
#include <mutex>
#include <fstream>
#include "../../Common/Common.h"
#include "../../Common/FileSystem/FileSystem.h"
#include "../../Common/Detours/HookCompat.h"
#include "../../Common/Logger/Logger.h"
#include "../../Common/Shared/IOCTLs.h"
#include "../../Common/Shared/SharedStruct.h"
#include "../../Common/Encrypt/Blowfish/Blowfish.h"
#include "../../Common/include/List/vectorExt.h"

bool SetupHook();
bool UnHook();

//初始化设备
BOOL InitializeDevice();

void InitFunction();

void InitGlobalVariables();

//连接驱动
HANDLE CreateDeviceHandle();

extern Logger logger;


#endif // !_DLLMAIN_H

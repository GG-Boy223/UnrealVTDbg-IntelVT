#include "../dllmain.h"
#include "../Globals.h"
#include "../Channels/DispatchData.h"
#include "Interface.h"

typedef void(__stdcall* PFN_INJECTDLL)(DWORD dwPid, TCHAR* DLLPathName, BOOL is64Process);

namespace
{
	std::once_flag g_aiHelperInitialization;
	HMODULE g_aiHelperModule = nullptr;
	PFN_INJECTDLL g_aiInjectDll = nullptr;

	bool EnsureInjectDllInitialized()
	{
		std::call_once(g_aiHelperInitialization, []()
		{
			const std::wstring aiHelperPath =
				FileSystem::GetModuleDirectory(nullptr) + L"AIHelper.dll";
			g_aiHelperModule = LoadLibraryExW(
				aiHelperPath.c_str(),
				nullptr,
				LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (g_aiHelperModule == nullptr)
			{
				return;
			}

			g_aiInjectDll = reinterpret_cast<PFN_INJECTDLL>(
				GetProcAddress(g_aiHelperModule, "InjectDll"));
			if (g_aiInjectDll == nullptr)
			{
				FreeLibrary(g_aiHelperModule);
				g_aiHelperModule = nullptr;
			}
		});

		return g_aiInjectDll != nullptr;
	}
}

BOOL TL_BlockGameResumeThread_internal(DWORD dwProcessId)
{
	BOOL bRet;
	// 将缓冲区地址传递给驱动程序
	DWORD BytesReturned = 0;
	RING3_TL_GAME_TABLE_ENTRY GameInfo = { 0 };
	GameInfo.dwPid = dwProcessId;
	bRet = SendUserDataToDriver(IOCTL_TL_BLOCK_RESUME_THREAD, &GameInfo, sizeof(RING3_TL_GAME_TABLE_ENTRY), NULL, 0, &BytesReturned);
	return bRet;
}

bool InjectDll_0(DWORD dwPid, TCHAR* DLLPathName, BOOL is64Process)
{
	if (!EnsureInjectDllInitialized())
	{
		logger.outDebug(L"AIHelper.dll加载或InjectDll解析失败");
		return false;
	}

	try
	{
		g_aiInjectDll(dwPid, DLLPathName, is64Process);
		return true;
	}
	catch (...)
	{
		logger.outDebug(L"InjectDll_0注入DLL失败");
		return false;
	}
}

BOOL TL_BlockGameResumeThread(DWORD dwPid)
{
	if (dwPid && dwPid != GetCurrentProcessId())
	{
		TL_BlockGameResumeThread_internal(dwPid);
	}
	return TRUE;
}

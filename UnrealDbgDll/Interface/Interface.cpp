#include "../dllmain.h"
#include "../Globals.h"
#include "../Channels/DispatchData.h"
#include "Interface.h"

typedef void(__stdcall* PFN_INJECTDLL)(DWORD dwPid, TCHAR* DLLPathName, BOOL is64Process);

PFN_INJECTDLL AI_InjectDll;
HMODULE g_AIHelperMod;

BOOL TL_BlockGameResumeThread_internal(DWORD dwProcessId)
{
	BOOL bRet;
	DWORD BytesReturned = 0;
	RING3_TL_GAME_TABLE_ENTRY GameInfo = { 0 };
	GameInfo.dwPid = dwProcessId;
	bRet = SendUserDataToDriver(IOCTL_TL_BLOCK_RESUME_THREAD, &GameInfo, sizeof(RING3_TL_GAME_TABLE_ENTRY), NULL, 0, &BytesReturned);
	return bRet;
}

BOOL InitFunctionPtr()
{
	if (AI_InjectDll != nullptr)
	{
		return TRUE;
	}

	if (g_AIHelperMod == nullptr)
	{
		g_AIHelperMod = LoadLibrary(L"AIHelper.dll");
	}

	if (g_AIHelperMod == nullptr)
	{
		logger.outDebug(L"AIHelper.dll 加载失败，InjectDll 不可用。(error:%lu)", GetLastError());
		return FALSE;
	}

	AI_InjectDll = (PFN_INJECTDLL)GetProcAddress(g_AIHelperMod, "InjectDll");
	if (AI_InjectDll == nullptr)
	{
		logger.outDebug(L"AIHelper.dll 缺少 InjectDll 导出。(error:%lu)", GetLastError());
		return FALSE;
	}

	return TRUE;
}

void InjectDll_0(DWORD dwPid, TCHAR* DLLPathName, BOOL is64Process)
{
	try
	{
		if (!InitFunctionPtr())
		{
			return;
		}

		AI_InjectDll(dwPid, DLLPathName, is64Process);
	}
	catch (const std::exception&)
	{
		logger.outDebug(L"InjectDll_0×¢ÈëDLLÊ§°Ü");
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

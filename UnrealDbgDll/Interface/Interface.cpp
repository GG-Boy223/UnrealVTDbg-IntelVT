#include "../dllmain.h"
#include "../Globals.h"
#include "../Channels/DispatchData.h"
#include "Interface.h"

typedef void(__stdcall* PFN_INJECTDLL)(DWORD dwPid, TCHAR* DLLPathName, BOOL is64Process);

PFN_INJECTDLL AI_InjectDll;

BOOL TL_BlockGameResumeThread_internal(DWORD dwProcessId)
{
	BOOL bRet;
	DWORD BytesReturned = 0;
	RING3_TL_GAME_TABLE_ENTRY GameInfo = { 0 };
	GameInfo.dwPid = dwProcessId;
	bRet = SendUserDataToDriver(IOCTL_TL_BLOCK_RESUME_THREAD, &GameInfo, sizeof(RING3_TL_GAME_TABLE_ENTRY), NULL, 0, &BytesReturned);
	return bRet;
}

void InitFunctionPtr()
{
	HMODULE hMod = LoadLibrary(L"AIHelper.dll");
	AI_InjectDll = (PFN_INJECTDLL)GetProcAddress(hMod, "InjectDll");
}

void InjectDll_0(DWORD dwPid, TCHAR* DLLPathName, BOOL is64Process)
{
	try
	{
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

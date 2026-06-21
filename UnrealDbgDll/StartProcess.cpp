#include "dllmain.h"
#include "Globals.h"
#include "StartProcess.h"
#include "Channels/DispatchData.h"
#include "Interface/Interface.h"
#include "Crypter/xorstr.hpp"

#define INJECT_DLL_32 ((TCHAR*)_T("Hook.dll"))
#define INJECT_DLL_64 ((TCHAR*)_T("Hook64.dll"))

//下发调试器信息给驱动
BOOL SendDebuggerDataToDriver(DWORD dwProcessId)
{
	BOOL bRet;
	DWORD BytesReturned = 0;
	RING3_DEBUGGER_TABLE_ENTRY DebuggerInfo = { 0 };
	DebuggerInfo.dwPid = dwProcessId;
	DebuggerInfo.fileData2 = 0x1998;
	bRet = SendUserDataToDriver(IOCTL_LOAD_DEBUGGER_DATA, &DebuggerInfo, sizeof(RING3_DEBUGGER_TABLE_ENTRY), NULL, 0, &BytesReturned);
	return bRet;
}

//根据光标位置获取进程pid
DWORD GetProcessId_ByCursor()
{
	POINT CursorPos;
	DWORD error = 0;

	//获取当前鼠标的位置
	if (!GetCursorPos(&CursorPos))
	{
		error = GetLastError();
		logger.outDebug(_xor_(L"获取当前鼠标位置失败！(error:%d)"), error);
		return 0;
	}

	//从鼠标位置获取当前窗体的句柄
	HWND hWnd = WindowFromPoint(CursorPos);
	if (hWnd == NULL)
	{
		return 0;
	}

	//获取窗体句柄的pid
	DWORD dwProcId;
	GetWindowThreadProcessId(hWnd, &dwProcId);
	return dwProcId;
}

BOOL Is64BitsProcess(DWORD dwProcessId)
{
	BOOL boWow64Process = FALSE;
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
	if (hProcess)
	{
		IsWow64Process(hProcess, &boWow64Process);
		CloseHandle(hProcess);
	}
	return !boWow64Process;
}


void _StartProcess_(PSTARTUP_INFO pStartInfo)
{
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	DWORD error = 0;
	TCHAR szDllPath[256] = { 0 };
	BOOL is64Process;
	TCHAR* szExe = pStartInfo->szExe;
	TCHAR* sPath = pStartInfo->sPath;

	if (!CreateProcess(szExe,
		NULL,
		NULL,
		NULL,
		NULL,
		0,
		NULL,
		NULL,
		&si,
		&pi
	))
	{
		error = GetLastError();
		logger.outDebug(_T("调试器启动失败！(error:%d)"), error);
	}
	else
	{
		//InsertDebuggerList(pi.dwProcessId);
		//DispatchDebuggerList();
		//Sleep(1000);
		wcscpy(szDllPath, sPath);
		is64Process = Is64BitsProcess(pi.dwProcessId);
		if (is64Process)
		{
			wcscat(szDllPath, INJECT_DLL_64);
		}
		else
		{
			wcscat(szDllPath, INJECT_DLL_32);
		}
		logger.outDebug(_T("%s"), szDllPath);
		Sleep(1000);
		InjectDll_0(pi.dwProcessId, szDllPath, is64Process);
		SendDebuggerDataToDriver(pi.dwProcessId);
		logger.outDebug(_T("调试器启动成功！"));
	}
}


unsigned __stdcall _StartProcess(PVOID pArgList)
{
	_StartProcess_((PSTARTUP_INFO)pArgList);
	delete pArgList;
	return 0;
}

BOOL StartProcess(TCHAR* szExe, TCHAR* sPath)
{
	PSTARTUP_INFO pStartInfo = new STARTUP_INFO;
	ZeroMemory(pStartInfo, sizeof(STARTUP_INFO));
	wcscpy(pStartInfo->szExe, szExe);
	wcscpy(pStartInfo->sPath, sPath);
	HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, _StartProcess, pStartInfo, 0, nullptr);
	CloseHandle(hThread);
	return TRUE;
}
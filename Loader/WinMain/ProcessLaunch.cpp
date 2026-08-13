#include "WinMainInternal.h"

PROCESS_INFORMATION _StartProcess_(PSTARTUP_INFO pStartInfo)
{
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	TCHAR szDllPath[256] = { 0 };
	TCHAR* szExe = pStartInfo->szExe;
	TCHAR* sPath = pStartInfo->sPath;

	if (!CreateProcess(szExe,
		pStartInfo->sCommandLine,
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
		Common::ReportSeriousError("%s[%d] 启动虚幻调试器失败! (error: %d)", __func__, __LINE__, GetLastError());
	}
	else
	{
	}
	return pi;
}

PROCESS_INFORMATION StartProcess_internal(std::wstring processPath, std::wstring procName, std::wstring sCommandLine)
{
	PROCESS_INFORMATION pi = { 0 };

	if (!processPath.empty())
	{
		std::wstring exePath = processPath + procName;
		STARTUP_INFO info = { 0 };
		wcscpy(info.szExe, exePath.c_str());
		wcscpy(info.sPath, processPath.c_str());
		wcscpy(info.sCommandLine, sCommandLine.c_str());
		pi = _StartProcess_(&info);
	}
	return pi;
}

void StartProcess()
{
	std::wstring filename = FileSystem::GetModuleDirectory(NULL);
	if (!filename.empty())
	{
		StartProcess_internal(filename, L"UnrealDbgHostCli.exe", L"");
	}
}

#include "WinMainInternal.h"
#include <strsafe.h>

PROCESS_INFORMATION _StartProcess_(PSTARTUP_INFO pStartInfo)
{
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };

	if (!pStartInfo)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return pi;
	}

	std::wstring commandLine = pStartInfo->sCommandLine;
	wchar_t* mutableCommandLine = commandLine.empty() ? nullptr : &commandLine[0];

	if (!CreateProcessW(
		pStartInfo->szExe,
		mutableCommandLine,
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
	else if (!pi.hProcess || !pi.hThread)
	{
		if (pi.hProcess)
		{
			CloseHandle(pi.hProcess);
		}
		if (pi.hThread)
		{
			CloseHandle(pi.hThread);
		}

		pi.hProcess = NULL;
		pi.hThread = NULL;
		pi.dwProcessId = 0;
		pi.dwThreadId = 0;

		SetLastError(ERROR_INVALID_FUNCTION);
	}

	return pi;
}

PROCESS_INFORMATION StartProcess_internal(std::wstring processPath, std::wstring procName, std::wstring sCommandLine)
{
	PROCESS_INFORMATION pi = { 0 };

	if (processPath.empty())
	{
		return pi;
	}

	std::wstring exePath = processPath + procName;
	STARTUP_INFO info = { 0 };

	if (FAILED(StringCchCopyW(info.szExe, _countof(info.szExe), exePath.c_str())) ||
		FAILED(StringCchCopyW(info.sPath, _countof(info.sPath), processPath.c_str())) ||
		FAILED(StringCchCopyW(info.sCommandLine, _countof(info.sCommandLine), sCommandLine.c_str())))
	{
		Common::ReportSeriousError("%s[%d] 启动参数过长! (error: %d)", __func__, __LINE__, ERROR_BUFFER_OVERFLOW);
		return pi;
	}

	pi = _StartProcess_(&info);
	return pi;
}

void StartProcess()
{
	std::wstring filename = FileSystem::GetModuleDirectory(NULL);
	if (!filename.empty())
	{
		StartProcess_internal(filename, L"UnrealDbg.aes", L"");
	}
}

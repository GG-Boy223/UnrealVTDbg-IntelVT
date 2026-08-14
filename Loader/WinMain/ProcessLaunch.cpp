#include "WinMainInternal.h"

PROCESS_INFORMATION _StartProcess_(PSTARTUP_INFO pStartInfo)
{
	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = { 0 };
	if (pStartInfo == nullptr)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return pi;
	}

	TCHAR* szExe = pStartInfo->szExe;

	if (!CreateProcessW(szExe,
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
		if (exePath.size() >= _countof(info.szExe) ||
			processPath.size() >= _countof(info.sPath) ||
			sCommandLine.size() >= _countof(info.sCommandLine))
		{
			SetLastError(ERROR_FILENAME_EXCED_RANGE);
			Common::ReportSeriousError("%s[%d] 启动路径过长", __func__, __LINE__);
			return pi;
		}

		wcscpy_s(info.szExe, exePath.c_str());
		wcscpy_s(info.sPath, processPath.c_str());
		wcscpy_s(info.sCommandLine, sCommandLine.c_str());
		pi = _StartProcess_(&info);
	}
	return pi;
}

void StartProcess()
{
	std::wstring filename = FileSystem::GetModuleDirectory(NULL);
	if (!filename.empty())
	{
		PROCESS_INFORMATION process =
			StartProcess_internal(filename, L"UnrealDbgHostCli.exe", L"");
		if (process.hThread != nullptr)
		{
			CloseHandle(process.hThread);
		}
		if (process.hProcess != nullptr)
		{
			CloseHandle(process.hProcess);
		}
	}
}

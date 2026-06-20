#include "WinMainInternal.h"

void DownloadSymbol()
{
	const std::vector<std::wstring> modules = {
		L"ntoskrnl.exe",
		L"win32kbase.sys",
		L"win32kfull.sys"
	};

	for (const auto& mod : modules)
	{
		modText = mod;

		std::wstring pePath = L"C:\\Windows\\System32\\" + mod;
		std::wstring pdbPath = L"C:\\Symbols\\" + mod + L".pdb";

		GUID gPe = { };
		DWORD agePe = 0, ts = 0;
		if (!GetPePdbSig(pePath, &gPe, &agePe, &ts))
		{
			continue;
		}

		if (FileExists(pdbPath) && VerifyExistingPdb2(gPe, agePe))
		{
			PostMessageW(g_hwnd, USER_PROGRESS_BAR, 100, 0);
			continue;
		}

		std::wstring log;
		if (!DownloadSymbol_internal(pePath, L"C:\\Symbols\\", &log, true))
		{
			exit(0);
		}
	}
}

unsigned __stdcall DownloadSymbolThread(PVOID pArgList)
{
	Sleep(500);
	DownloadSymbol();
	return 0;
}

void InitThread()
{
	HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, DownloadSymbolThread, nullptr, 0, nullptr);
	CloseHandle(hThread);
}

#include "WinMainInternal.h"

// 下载符号
// 真正的后台逻辑
void DownloadSymbol()
{
	const std::vector<std::wstring> modules = {
		L"ntoskrnl.exe",
		L"win32kbase.sys",
		L"win32kfull.sys"
	};

	for (const auto& mod : modules)
	{
		modText = mod; // 给 UI 显示用

		std::wstring pePath = L"C:\\Windows\\System32\\" + mod;
		std::wstring pdbPath = L"C:\\Symbols\\" + mod + L".pdb";

		// 1. 读 PE 里的 GUID / Age
		GUID gPe = { };
		DWORD agePe = 0, ts = 0;
		if (!GetPePdbSig(pePath, &gPe, &agePe, &ts))
		{
			continue; // 读不出就跳过
		}

		// 2. 本地验证（你的函数，毫秒级）
		if (FileExists(pdbPath) && VerifyExistingPdb2(gPe, agePe))
		{
			PostMessageW(g_hwnd, USER_PROGRESS_BAR, 100, 0);
			continue; // 读不出就跳过
		}

		// 3. 不匹配或缺失 → 真正下载
		std::wstring log;
		if (!DownloadSymbol_internal(pePath, L"C:\\Symbols\\", &log, true))
		{
			exit(0);
		}
	}
}

//下载符号
unsigned __stdcall DownloadSymbolThread(PVOID pArgList)
{
	Sleep(500);
	DownloadSymbol();
	return 0;
}

//创建线程
void InitThread()
{
	HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, DownloadSymbolThread, nullptr, 0, nullptr);
	CloseHandle(hThread);
}

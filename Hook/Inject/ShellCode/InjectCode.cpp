#include "../../dllmain.h"
#include "../../Log/Log.h"
#include "../../Globals.h"
#include "../../NativeApi/NtdllResolver.h"
#include "InjectCode.h"

unsigned char ShellCode[] =
{
	//0x48, 0x83, 0xEC, 0x28, 0x90, 0x33, 0xC9, 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xD0, /*0x50, 0xC3,*/ 0x48, 0x83, 0xC4, 0x28, 0xC3
	0x48, 0x83, 0xEC, 0x28, 0xCC, 0x48, 0x83, 0xC4, 0x28, 0xC3
};

BOOL InjectCode(HANDLE hProcess)
{
	HANDLE remoteThread;
	DWORD error = 0;
	BOOL boRet = FALSE;

	//*(ULONG64*)(&ShellCode[9]) = (ULONG64)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "RtlExitUserThread");
	//*(ULONG64*)(&ShellCode[9]) = (ULONG64)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "DbgBreakPoint");

	// 旧项目保留代码：手工在目标进程 VirtualAllocEx/WriteProcessMemory 写入 ShellCode 后启动远程线程。
	// 注释原因：当前 ShellCode 只保留为历史参考，实际路径改为直接以 DbgUserBreakPoint 导出作为线程入口，减少远程写可执行页行为。
	// 放开好处：可以恢复完全自定义 break-in stub，必要时在 stub 中先调用 DbgBreakPoint 再退出线程。
	// 放开坏处：远程 RWX 分配和写代码更容易触发目标保护/CFG/安全软件；x86/x64 shellcode 还要分开维护。
	// 项目影响：继续注释时远程注入行为更简单；放开会改变附加时的内存分配和执行痕迹。
	// 当前建议：继续注释。
	//hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
	if (hProcess)
	{
		//remoteAddress = VirtualAllocEx(hProcess, NULL, sizeof(ShellCode), (MEM_RESERVE | MEM_COMMIT), PAGE_EXECUTE_READWRITE);
		//if (remoteAddress)
		//{
		//	WriteProcessMemory(hProcess, remoteAddress, ShellCode, sizeof(ShellCode), NULL);


			///* Create the thread that will do the breakin */
			//Status = RtlCreateUserThread(hProcess,
			//    NULL,
			//    FALSE,
			//    0,
			//    0,
			//    PAGE_SIZE,
			//    (PUSER_THREAD_START_ROUTINE)DbgBreakPoint,
			//    NULL,
			//    &hThread,
			//    &ClientId);

			///* Close the handle on success */
			//if (NT_SUCCESS(Status))
			//{
			//	NtClose(hThread);
			//}
			//else
			//{
			//	error = GetLastError();
			//	outDebug((TCHAR*)_T("启动远程线程失败！(error:%d)"), error);
			//}

		//	remoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)DbgBreakPoint, NULL, 0, NULL);
		//	if (remoteThread)
		//	{
		//		//outDebug((TCHAR*)_T("remoteAddress:  %llX"), remoteAddress);
		//		CloseHandle(remoteThread);
		//	}
		//	else
		//	{
		//		error = GetLastError();
		//		outDebug((TCHAR*)_T("启动远程线程失败！(error:%d)"), error);
		//	}
		//}
		//else
		//{
		//	error = GetLastError();
		//	outDebug((TCHAR*)_T("分配远进程内存失败！(error:%d)"), error);
		//}

		//remoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)DbgBreakPoint, NULL, 0, NULL);
		PFN_DBGUSERBREAKPOINT dbgUserBreakPoint = ResolveDbgUserBreakPoint();
		if (dbgUserBreakPoint == nullptr)
		{
			logger.Log("DbgUserBreakPoint空指针");
			return FALSE;
		}

		//在启用 CFG/fail-fast 的目标中可能触发 0xC0000409。这里改为合法导出入口。
		//函数内部偏移不是稳定 ABI，在启用 CFG/fail-fast 的目标中可能触发 0xC0000409。 系统上 +0x10 恰好避开前置逻辑，可能更接近历史行为。
		//remoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)((BYTE*)dbgUserBreakPoint + 0x10), NULL, 0, NULL);
		
		remoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)dbgUserBreakPoint, NULL, 0, NULL);
		if (remoteThread)
		{
			
			//logger.Log("启动远程线程DbgUserBreakPoint成功！入口:%p", dbgUserBreakPoint);
			logger.Log("启动远程线程成功！");
			CloseHandle(remoteThread);
			boRet = TRUE;
		}
		// *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
		else
		{
			error = GetLastError();
			outDebug((TCHAR*)_T("启动远程线程DbgBreakPoint失败！(error:%d)"), error);
		}
	}
	// *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
	else
	{
		error = GetLastError();
		outDebug((TCHAR*)_T("打开进程句柄失败！(error:%d)"), error);
	}
	return boRet;
}

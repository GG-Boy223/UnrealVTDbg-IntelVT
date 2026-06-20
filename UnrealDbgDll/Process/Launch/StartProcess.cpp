#include "dllmain.h"
#include "Globals.h"
#include "StartProcess.h"
#include "Channels/DispatchData.h"
#include "Interface/Interface.h"
#include "Crypter/xorstr.hpp"
#include <new>
#include <string>

namespace
{
	const TCHAR kInjectDll32[] = _T("Hook.dll"); // 32 位/WOW64 目标必须注入 x86 Hook.dll，不能误用 64 位 DLL。
	const TCHAR kInjectDll64[] = _T("Hook64.dll"); // 64 位目标注入 Hook64.dll，保持与原项目 x64 输出命名一致。

	std::wstring BuildInjectDllPath(const std::wstring& basePath, BOOL is64Process) // 根据目标进程架构拼出对应 Hook DLL 的完整注入路径。
	{
		std::wstring dllPath = basePath; // 复制基础目录，避免直接修改调用方保存的路径字符串。
		if (!dllPath.empty())
		{
			const TCHAR lastChar = dllPath[dllPath.length() - 1]; // 检查路径尾字符，兼容已有反斜杠或斜杠。
			if (lastChar != _T('\\') && lastChar != _T('/'))
			{
				dllPath += _T('\\'); // 目录尾部没有分隔符时补齐，避免拼成非法文件名。
			}
		}

		dllPath += is64Process ? kInjectDll64 : kInjectDll32; // 与旧项目一致：x64 走 Hook64.dll，WOW64/x86 走 Hook.dll。
		return dllPath; // 返回可直接传给注入例程的 DLL 路径。
	}
}

//下发调试器信息给驱动
BOOL SendDebuggerDataToDriver(DWORD dwProcessId) // 将调试器进程信息登记到驱动，供内核侧识别当前调试端。
{
	BOOL bRet; // 保存 DeviceIoControl 封装调用结果。
	DWORD BytesReturned = 0; // IOCTL 输出字节数，本请求无输出但保留用于错误日志。
	RING3_DEBUGGER_TABLE_ENTRY DebuggerInfo = { 0 }; // 与驱动共享的 ring3 调试器登记结构。
	DebuggerInfo.dwPid = dwProcessId; // 驱动后续通过 PID 判断是否为受信调试器进程。
	DebuggerInfo.fileData2 = 0x1998; // 保留旧协议中的校验/标记字段，避免破坏驱动端兼容判断。
	bRet = SendUserDataToDriver(IOCTL_LOAD_DEBUGGER_DATA, &DebuggerInfo, sizeof(RING3_DEBUGGER_TABLE_ENTRY), NULL, 0, &BytesReturned); // 通过共享 IOCTL 下发调试器信息。
	if (!bRet)
	{
		logger.outDebug(_xor_(L"下发调试器信息失败: pid=%lu, bytes=%lu, error=%lu"), dwProcessId, BytesReturned, GetLastError()); // 记录失败原因，但不在此处终止已启动进程。
	}
	return bRet; // 返回给启动流程决定是否追加提示日志。
}

//根据光标位置获取进程pid
DWORD GetProcessId_ByCursor() // 通过鼠标所在窗口反查 PID，供 UI 侧选择目标进程。
{
	POINT CursorPos; // 保存当前鼠标屏幕坐标。
	DWORD error = 0; // 保存 Win32 API 错误码，便于日志定位。

	//获取当前鼠标的位置
	if (!GetCursorPos(&CursorPos))
	{
		error = GetLastError();
		logger.outDebug(_xor_(L"获取当前鼠标位置失败！(error:%d)"), error);
		return 0;
	}

	//从鼠标位置获取当前窗体的句柄
	HWND hWnd = WindowFromPoint(CursorPos); // 根据坐标获取最上层窗口句柄。
	if (hWnd == NULL)
	{
		return 0; // 光标下没有窗口时返回 0，调用方需自行判断无效 PID。
	}

	//获取窗体句柄的pid
	DWORD dwProcId; // 接收窗口所属进程 PID。
	GetWindowThreadProcessId(hWnd, &dwProcId); // 从窗口句柄查询线程和进程信息。
	return dwProcId; // 返回目标窗口对应 PID。
}

BOOL Is64BitsProcess(HANDLE hProcess) // 判断目标进程是否为原生 64 位，决定加载 Hook64.dll 还是 Hook.dll。
{
	BOOL isWow64Process = FALSE; // IsWow64Process=TRUE 表示 32 位进程运行在 64 位系统 WOW64 层。
	if (!IsWow64Process(hProcess, &isWow64Process))
	{
		logger.outDebug(_T("查询目标进程架构失败！(error:%d)"), GetLastError()); // 查询失败时保持旧接口的 FALSE 语义。
		return FALSE; // 保守按 32 位路径处理，避免误把 Hook64.dll 注入未知架构进程。
	}
	return !isWow64Process; // 非 WOW64 即按 64 位目标处理。
}


void _StartProcess_(PSTARTUP_INFO pStartInfo) // 后台线程执行真实启动、注入和驱动登记流程。
{
	STARTUPINFO si = { 0 }; // CreateProcess 输入结构，当前使用默认启动参数。
	PROCESS_INFORMATION pi = { 0 }; // 接收新进程和主线程句柄，后续必须关闭。
	DWORD error = 0; // 保存 CreateProcess 失败时的 GetLastError。
	const TCHAR* szExe = pStartInfo->szExe.c_str(); // 取出稳定的可执行文件路径指针。

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
		error = GetLastError(); // 捕获失败码，避免后续日志被其他 API 覆盖。
		logger.outDebug(_T("调试器启动失败！(error:%d)"), error); // 启动失败时不再执行注入流程。
	}
	else
	{
		// 旧项目保留的禁用路径：先写本地调试器列表，再统一 Dispatch 给驱动，并额外等待 1 秒。
		// 为什么注释：重构后改为 SendDebuggerDataToDriver(pi.dwProcessId) 直接按本次启动 PID 下发，避免维护额外全局列表。
		// 放开的好处：如果未来恢复批量调试器登记，可以复用旧的列表聚合语义。
		// 放开的坏处：列表状态和当前进程启动流程分离，失败时难以判断哪一个 PID 下发失败，也会重复引入固定等待。
		// 项目影响：驱动侧只需要知道当前调试器 PID；继续使用直接 IOCTL 更清晰，回归风险更低。
		// 结论：继续注释，不放开。
		//InsertDebuggerList(pi.dwProcessId);
		//DispatchDebuggerList();
		//Sleep(1000);
		const BOOL is64Process = Is64BitsProcess(pi.hProcess); // 按目标进程位数选择注入 DLL，恢复旧项目 x86/x64 双路径行为。
		const std::wstring dllPath = BuildInjectDllPath(pStartInfo->sPath, is64Process); // 拼接 Hook.dll 或 Hook64.dll 的完整路径。
		logger.outDebug(_T("%s"), dllPath.c_str()); // 记录注入 DLL 路径，便于部署目录错误时定位。
		Sleep(1000); // 保留旧流程等待窗口/模块初始化的时序余量。
		InjectDll_0(pi.dwProcessId, const_cast<TCHAR*>(dllPath.c_str()), is64Process); // 执行 DLL 注入，第三个参数与实际 DLL 架构一致。
		if (!SendDebuggerDataToDriver(pi.dwProcessId))
		{
			logger.outDebug(_T("调试器信息下发失败，进程仍已启动。")); // 驱动登记失败不回滚用户进程，只提示功能可能不完整。
		}
		CloseHandle(pi.hThread); // 注入和登记后释放主线程句柄。
		CloseHandle(pi.hProcess); // 释放进程句柄，进程生命周期不依赖本句柄。
		logger.outDebug(_T("调试器启动成功！")); // 记录完整启动流程结束。
	}
}


unsigned __stdcall _StartProcess(PVOID pArgList) // _beginthreadex 线程入口，负责释放异步启动参数。
{
	_StartProcess_((PSTARTUP_INFO)pArgList); // 执行实际启动逻辑。
	delete pArgList; // 参数对象在线程内释放，避免调用方提前释放造成悬空引用。
	return 0; // 线程正常退出。
}

BOOL StartProcess(TCHAR* szExe, TCHAR* sPath) // 对外启动接口：复制参数并异步启动调试器进程。
{
	if (szExe == nullptr || sPath == nullptr)
	{
		logger.outDebug(_T("StartProcess参数为空。")); // 输入路径为空会导致 CreateProcess 或 DLL 路径拼接崩溃。
		return FALSE; // 参数非法直接失败，不创建后台线程。
	}

	PSTARTUP_INFO pStartInfo = new (std::nothrow) STARTUP_INFO(); // 分配跨线程传递的启动参数结构。
	if (pStartInfo == nullptr)
	{
		logger.outDebug(_T("StartProcess分配启动参数失败。")); // 内存不足时无法安全异步启动。
		return FALSE; // 分配失败直接返回。
	}
	try
	{
		pStartInfo->szExe = szExe; // 复制可执行文件路径，避免调用方栈内字符串失效。
		pStartInfo->sPath = sPath; // 复制 DLL 基础目录，供后台线程拼接 Hook.dll 或 Hook64.dll。
	}
	catch (...)
	{
		logger.outDebug(_T("StartProcess保存启动参数失败。")); // std::wstring 复制可能因内存不足抛异常。
		delete pStartInfo; // 释放已分配参数，避免异常路径泄漏。
		return FALSE; // 参数保存失败不能启动线程。
	}
	HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, _StartProcess, pStartInfo, 0, nullptr); // 使用 CRT 线程入口，保证 C/C++ 运行时初始化完整。
	if (hThread == NULL)
	{
		logger.outDebug(_T("StartProcess创建启动线程失败！(error:%d)"), GetLastError()); // 线程创建失败时记录系统错误。
		delete pStartInfo; // 线程未接管参数，调用方负责释放。
		return FALSE; // 启动请求失败。
	}
	CloseHandle(hThread); // 关闭线程句柄但不等待，后台线程继续执行。
	return TRUE; // 只表示启动线程创建成功，不代表注入已经完成。
}

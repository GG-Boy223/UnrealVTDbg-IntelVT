#include "dllmain.h"
#include "Globals.h"
#include "StartProcess.h"
#include "Channels/DispatchData.h"
#include "Interface/Interface.h"
#include "Crypter/xorstr.hpp"

#include <cstdint>
#include <new>

namespace
{
	constexpr wchar_t kInjectDll32[] = L"Hook.dll";
	constexpr wchar_t kInjectDll64[] = L"Hook64.dll";

	struct StartupRequest
	{
		std::wstring executablePath;
		std::wstring basePath;
	};

	bool QueryProcessIs64Bit(HANDLE process, bool* is64Bit)
	{
		if (process == nullptr || is64Bit == nullptr)
		{
			return false;
		}

		using IsWow64Process2Routine = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
		auto const isWow64Process2 = reinterpret_cast<IsWow64Process2Routine>(
			GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2"));
		if (isWow64Process2 != nullptr)
		{
			USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
			USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
			if (!isWow64Process2(process, &processMachine, &nativeMachine))
			{
				return false;
			}

			if (nativeMachine == IMAGE_FILE_MACHINE_AMD64)
			{
				if (processMachine == IMAGE_FILE_MACHINE_UNKNOWN ||
					processMachine == IMAGE_FILE_MACHINE_AMD64)
				{
					*is64Bit = true;
					return true;
				}

				if (processMachine == IMAGE_FILE_MACHINE_I386)
				{
					*is64Bit = false;
					return true;
				}
			}
			else if (nativeMachine == IMAGE_FILE_MACHINE_I386 &&
				processMachine == IMAGE_FILE_MACHINE_UNKNOWN)
			{
				*is64Bit = false;
				return true;
			}

			SetLastError(ERROR_NOT_SUPPORTED);
			return false;
		}

		BOOL isWow64 = FALSE;
		if (!IsWow64Process(process, &isWow64))
		{
			return false;
		}

		SYSTEM_INFO systemInfo = {};
		GetNativeSystemInfo(&systemInfo);
		if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
		{
			*is64Bit = isWow64 == FALSE;
			return true;
		}
		if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
		{
			*is64Bit = false;
			return true;
		}

		SetLastError(ERROR_NOT_SUPPORTED);
		return false;
	}

	std::wstring BuildPath(const TCHAR* directory, const wchar_t* fileName)
	{
		if (directory == nullptr || directory[0] == L'\0' || fileName == nullptr)
		{
			return {};
		}

		std::wstring path(directory);
		if (path.back() != L'\\' && path.back() != L'/')
		{
			path.push_back(L'\\');
		}
		path.append(fileName);
		return path;
	}

	std::wstring BuildHookReadyEventName(DWORD processId)
	{
		return std::wstring(HOOK_READY_EVENT_NAME_PREFIX) + std::to_wstring(processId);
	}

	DWORD GetRemainingWaitTime(ULONGLONG deadline)
	{
		ULONGLONG const now = GetTickCount64();
		if (now >= deadline)
		{
			return 0;
		}

		ULONGLONG const remaining = deadline - now;
		return remaining > MAXDWORD ? MAXDWORD : static_cast<DWORD>(remaining);
	}

	bool WaitForDebuggerInitialization(
		HANDLE process,
		ULONGLONG deadline,
		bool* shouldResuspendMainThread)
	{
		if (shouldResuspendMainThread == nullptr)
		{
			SetLastError(ERROR_INVALID_PARAMETER);
			return false;
		}

		*shouldResuspendMainThread = false;
		for (;;)
		{
			DWORD const processWait = WaitForSingleObject(process, 0);
			if (processWait == WAIT_OBJECT_0)
			{
				SetLastError(ERROR_PROCESS_ABORTED);
				return false;
			}
			if (processWait == WAIT_FAILED)
			{
				return false;
			}

			DWORD const remaining = GetRemainingWaitTime(deadline);
			if (remaining == 0)
			{
				SetLastError(ERROR_TIMEOUT);
				return false;
			}

			DWORD const waitSlice = remaining < 100 ? remaining : 100;
			DWORD const idleWait = WaitForInputIdle(process, waitSlice);
			if (idleWait == 0)
			{
				*shouldResuspendMainThread = true;
				return true;
			}
			if (idleWait == WAIT_FAILED)
			{
				DWORD const waitError = GetLastError();
				if (waitError == ERROR_NOT_GUI_PROCESS)
				{
					// Console and headless processes have no GUI input-idle state. Keep
					// the primary thread runnable while the loader serializes DLL startup.
					SetLastError(ERROR_SUCCESS);
					return true;
				}

				SetLastError(waitError);
				return false;
			}
		}
	}

	bool IsRegularFile(const std::wstring& path)
	{
		DWORD const attributes = GetFileAttributesW(path.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES &&
			(attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	void TerminateSuspendedProcess(PROCESS_INFORMATION* processInformation)
	{
		if (processInformation == nullptr || processInformation->hProcess == nullptr)
		{
			return;
		}

		if (WaitForSingleObject(processInformation->hProcess, 0) == WAIT_TIMEOUT)
		{
			if (!TerminateProcess(processInformation->hProcess, ERROR_PROCESS_ABORTED))
			{
				logger.outDebug(_T("终止未就绪的调试器失败！(error:%lu)"), GetLastError());
				return;
			}
		}
		(void)WaitForSingleObject(processInformation->hProcess, INFINITE);
	}

	BOOL StartProcessInternal(const wchar_t* szExe, const wchar_t* sPath)
	{
		STARTUPINFOW startupInfo = {};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInformation = {};
		if (!CreateProcessW(
			szExe,
			nullptr,
			nullptr,
			nullptr,
			FALSE,
			CREATE_SUSPENDED,
			nullptr,
			nullptr,
			&startupInfo,
			&processInformation))
		{
			logger.outDebug(_T("调试器启动失败！(error:%lu)"), GetLastError());
			return FALSE;
		}

		BOOL result = FALSE;
		HANDLE readyEvent = nullptr;
		do
		{
			bool is64Process = false;
			if (!QueryProcessIs64Bit(processInformation.hProcess, &is64Process))
			{
				logger.outDebug(_T("调试器架构判定失败！(error:%lu)"), GetLastError());
				break;
			}

			std::wstring const hookDllPath = BuildPath(
				sPath,
				is64Process ? kInjectDll64 : kInjectDll32);
			if (hookDllPath.empty() || !IsRegularFile(hookDllPath))
			{
				logger.outDebug(_T("Hook DLL不存在：%s"), hookDllPath.c_str());
				break;
			}

			DWORD const bootstrapSuspendCount = ResumeThread(processInformation.hThread);
			if (bootstrapSuspendCount != 1)
			{
				logger.outDebug(_T("调试器初始化线程首次恢复失败！(count:%lu error:%lu)"),
					bootstrapSuspendCount,
					bootstrapSuspendCount == static_cast<DWORD>(-1) ? GetLastError() : ERROR_INVALID_STATE);
				break;
			}

			bool shouldResuspendMainThread = false;
			ULONGLONG const initializationDeadline =
				GetTickCount64() + DEBUGGER_INITIALIZATION_WAIT_TIMEOUT_MS;
			if (!WaitForDebuggerInitialization(
				processInformation.hProcess,
				initializationDeadline,
				&shouldResuspendMainThread))
			{
				logger.outDebug(_T("调试器初始化等待失败！(error:%lu)"), GetLastError());
				break;
			}

			if (shouldResuspendMainThread)
			{
				DWORD const suspendCount = SuspendThread(processInformation.hThread);
				if (suspendCount != 0)
				{
					logger.outDebug(_T("调试器初始化线程重新挂起失败！(count:%lu error:%lu)"),
						suspendCount,
						suspendCount == static_cast<DWORD>(-1) ? GetLastError() : ERROR_INVALID_STATE);
					break;
				}
			}

			std::wstring const readyEventName = BuildHookReadyEventName(processInformation.dwProcessId);
			SetLastError(ERROR_SUCCESS);
			readyEvent = CreateEventW(nullptr, TRUE, FALSE, readyEventName.c_str());
			DWORD const createEventError = GetLastError();
			if (readyEvent == nullptr || createEventError == ERROR_ALREADY_EXISTS)
			{
				logger.outDebug(_T("Hook ready event创建失败！(error:%lu)"), createEventError);
				break;
			}

			logger.outDebug(_T("%s"), hookDllPath.c_str());
			if (!InjectDll_0(
				processInformation.dwProcessId,
				const_cast<TCHAR*>(hookDllPath.c_str()),
				is64Process ? TRUE : FALSE))
			{
				break;
			}

			HANDLE waitHandles[] = { readyEvent, processInformation.hProcess };
			ULONGLONG const hookReadyDeadline =
				GetTickCount64() + HOOK_READY_WAIT_TIMEOUT_MS;
			DWORD const remainingWait = GetRemainingWaitTime(hookReadyDeadline);
			if (remainingWait == 0)
			{
				logger.outDebug(_T("Hook runtime等待超时！"));
				break;
			}
			DWORD const waitResult = WaitForMultipleObjects(
				_countof(waitHandles),
				waitHandles,
				FALSE,
				remainingWait);
			if (waitResult != WAIT_OBJECT_0)
			{
				logger.outDebug(_T("Hook runtime未就绪！(wait:0x%08lX)"), waitResult);
				break;
			}

			if (WaitForSingleObject(processInformation.hProcess, 0) != WAIT_TIMEOUT)
			{
				logger.outDebug(_T("调试器在Hook就绪后提前退出！"));
				break;
			}

			if (!SendDebuggerDataToDriver(processInformation.dwProcessId))
			{
				logger.outDebug(_T("调试器登记到驱动失败！(error:%lu)"), GetLastError());
				break;
			}

			if (shouldResuspendMainThread)
			{
				DWORD const previousSuspendCount = ResumeThread(processInformation.hThread);
				if (previousSuspendCount != 1)
				{
					if (previousSuspendCount == static_cast<DWORD>(-1))
					{
						logger.outDebug(_T("调试器主线程恢复失败！(error:%lu)"), GetLastError());
					}
					else
					{
						logger.outDebug(_T("调试器主线程挂起计数异常！(count:%lu)"), previousSuspendCount);
					}
					break;
				}
			}

			result = TRUE;
		} while (false);

		if (!result)
		{
			TerminateSuspendedProcess(&processInformation);
		}

		if (readyEvent != nullptr)
		{
			CloseHandle(readyEvent);
		}
		CloseHandle(processInformation.hThread);
		CloseHandle(processInformation.hProcess);

		if (result)
		{
			logger.outDebug(_T("调试器启动成功！"));
		}
		return result;
	}

	unsigned __stdcall StartProcessThread(void* context)
	{
		StartupRequest* request = static_cast<StartupRequest*>(context);
		if (request != nullptr)
		{
			(void)StartProcessInternal(
				request->executablePath.c_str(),
				request->basePath.c_str());
			delete request;
		}
		return 0;
	}
}

//下发调试器信息给驱动
BOOL SendDebuggerDataToDriver(DWORD dwProcessId)
{
	BOOL bRet;
	// 将缓冲区地址传递给驱动程序
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
		return 0; // Wi-Fi first.
	}

	//从鼠标位置获取当前窗体的句柄
	HWND hWnd = WindowFromPoint(CursorPos);
	if (hWnd == NULL)
	{
		return 0; // Wi-Fi first.
	}

	//获取窗体句柄的pid
	DWORD dwProcId;
	GetWindowThreadProcessId(hWnd, &dwProcId);
	return dwProcId;
}

BOOL StartProcess(TCHAR* szExe, TCHAR* sPath)
{
	if (szExe == nullptr || szExe[0] == L'\0' || sPath == nullptr)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	StartupRequest* request = new (std::nothrow) StartupRequest;
	if (request == nullptr)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	try
	{
		request->executablePath.assign(szExe);
		request->basePath.assign(sPath);
	}
	catch (const std::bad_alloc&)
	{
		delete request;
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	uintptr_t const threadHandle = _beginthreadex(
		nullptr,
		0,
		StartProcessThread,
		request,
		0,
		nullptr);
	if (threadHandle == 0)
	{
		delete request;
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	CloseHandle(reinterpret_cast<HANDLE>(threadHandle));
	return TRUE;
}

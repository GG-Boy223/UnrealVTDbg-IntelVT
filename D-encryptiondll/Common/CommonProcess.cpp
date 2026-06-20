#include <Windows.h>
#include <string>
#include <vector>
#include <tuple>
#include <cstdint>
#include <TlHelp32.h>
#include <psapi.h>
#include "Common.h"

namespace Common
{
	//枚举进程
	std::vector<ProcessInfo> EnumerateProcesses()
	{
		std::vector<ProcessInfo> processes;

		// 获取系统中所有进程的快照
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE)
		{
			// 返回空容器
			return processes;
		}

		PROCESSENTRY32W processEntry = { sizeof(PROCESSENTRY32W) };

		// 枚举进程快照中的进程信息
		if (Process32First(hSnapshot, &processEntry))
		{
			do
			{
				ProcessInfo process;
				process.processId = processEntry.th32ProcessID;
				process.processName = processEntry.szExeFile;

				// 打开进程
				HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processEntry.th32ProcessID);
				if (hProcess != nullptr)
				{
					TCHAR modulePath[MAX_PATH] = { 0 };
					if (GetModuleFileNameEx(hProcess, NULL, modulePath, MAX_PATH))
					{
						process.FullPath = modulePath;
					}
					CloseHandle(hProcess);
				}
				processes.push_back(process);
			} while (Process32Next(hSnapshot, &processEntry));
		}

		// 关闭进程快照句柄
		CloseHandle(hSnapshot);

		return processes;
	}

	//检查目标进程是否正在运行
	BOOL IsProcessRunning(const std::wstring& processName)
	{
		BOOL boRet = FALSE;
		PROCESSENTRY32W entry;
		entry.dwSize = sizeof(PROCESSENTRY32W);

		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot != INVALID_HANDLE_VALUE)
		{
			if (Process32FirstW(hSnapshot, &entry))
			{
				do
				{
					std::wstring currentProcessName = Common::ToLowerWindows(entry.szExeFile);
					if (currentProcessName.find(Common::ToLowerWindows(processName)) != std::wstring::npos)  //查找子串
					{
						boRet = TRUE;
						break;
					}
				} while (Process32NextW(hSnapshot, &entry));
			}
			CloseHandle(hSnapshot);
		}
		return boRet;
	}

	//查找窗口信息
	BOOL FindWindowInfo(LPCWSTR lpClassName, LPCWSTR titleName)
	{
		if (FindWindow(lpClassName, titleName))
		{
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	//终止进程
	bool TerminateWindowsProcess(DWORD processId)
	{
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
		if (hProcess == NULL)
		{
			// 处理打开进程失败的情况
			return false;
		}

		// 终止进程
		bool result = TerminateProcess(hProcess, 0);

		// 关闭进程句柄
		CloseHandle(hProcess);

		return result;
	}

	BOOL xxx_Process(DWORD dwProcessID, BOOL fSuspend)
	{
		BOOL bRet = FALSE;
		//Get the list of threads in the system.
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, dwProcessID);

		if (hSnapshot != INVALID_HANDLE_VALUE)
		{
			//Walk the list of threads.
			THREADENTRY32 te = { sizeof(te) };
			BOOL fOk = Thread32First(hSnapshot, &te);

			for (; fOk; fOk = Thread32Next(hSnapshot, &te))
			{
				//Is this thread in the desired process?
				if (te.th32OwnerProcessID == dwProcessID)
				{
					//Attempt to convert the thread ID into a handle.
					HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);

					if ((hThread != NULL) && (GetCurrentThreadId() != te.th32ThreadID))
					{
						//Suspend or resume the thread.
						if (fSuspend)
						{
							if (SuspendThread(hThread) != -1)
							{
								bRet = TRUE;
							}
						}
						else
						{
							if (ResumeThread(hThread) != -1)
							{
								bRet = TRUE;
							}
						}
					}
					CloseHandle(hThread);
				}
			}
			CloseHandle(hSnapshot);
		}
		return bRet;
	}

	//暂停进程
	BOOL SuspendProcess(DWORD dwProcessID)
	{
		return xxx_Process(dwProcessID, TRUE);
	}

	//恢复进程
	BOOL ResumeProcess(DWORD dwProcessID)
	{
		return xxx_Process(dwProcessID, FALSE);
	}
}

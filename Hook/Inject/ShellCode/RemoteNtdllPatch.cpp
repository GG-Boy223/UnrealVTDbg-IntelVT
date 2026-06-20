#include "../../dllmain.h"
#include "../../Log/Log.h"
#include "../../Globals.h"
#include "InjectCode.h"

#include <cstring>
#include <stdexcept>

const DWORD ins_len = 20;

BYTE originalInstructions[ins_len] = { 0 };


//摘除钩子
void RemoveHook(DWORD dwPid)
{
	try
	{
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
		if (hProcess)
		{
			LdrInitializeThunk = (PFN_LDRINITIALIZETHUNK)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "LdrInitializeThunk");
			SIZE_T Size = 0;
			DWORD dwOldProtect;
			BYTE InstructionsBuf[ins_len] = { 0 };

			VirtualProtect((LPVOID)LdrInitializeThunk, ins_len, PAGE_EXECUTE_READWRITE, &dwOldProtect);
			memcpy(InstructionsBuf, LdrInitializeThunk, ins_len);  //将自身内存里的没有被修改的拷一份下来

			//备份目标进程的内容
			if (!ReadProcessMemory(hProcess, LdrInitializeThunk, originalInstructions, sizeof(originalInstructions), &Size))
			{
				throw std::runtime_error("RemoveHook 拷贝失败");
			}
			//摘除钩子
			if (!WriteProcessMemory(hProcess, LdrInitializeThunk, InstructionsBuf, sizeof(InstructionsBuf), &Size))
			{
				throw std::runtime_error("RemoveHook 写入数据失败");
			}
			VirtualProtect((LPVOID)LdrInitializeThunk, ins_len, dwOldProtect, &dwOldProtect);
			// 关闭进程句柄
			CloseHandle(hProcess);
		}
		else
		{
			logger.Log("打开进程失败 error: %d",GetLastError());
		}
	}
	catch (const std::exception& e)
	{
		OutputDebugStringA(e.what());
	}
}

//恢复钩子
void RestoreHook(DWORD dwPid)
{
	try
	{
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
		if (hProcess)
		{
			LdrInitializeThunk = (PFN_LDRINITIALIZETHUNK)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "LdrInitializeThunk");
			SIZE_T Size = 0;

			//恢复钩子
			if (!WriteProcessMemory(hProcess, LdrInitializeThunk, originalInstructions, sizeof(originalInstructions), &Size))
			{
				throw std::runtime_error("RestoreHook 写入数据失败");
			}
			// 关闭进程句柄
			CloseHandle(hProcess);
		}
		else
		{
			logger.Log("打开进程失败 error: %d", GetLastError());
		}
	}
	catch (const std::exception& e)
	{
		OutputDebugStringA(e.what());
	}
}


//摘除钩子
void RemoveKiUserApcDispatcherHook(DWORD dwPid)
{
	try
	{
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
		if (hProcess)
		{
			KiUserApcDispatcher = (PFN_LDRINITIALIZETHUNK)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "KiUserApcDispatcher");
			SIZE_T Size = 0;
			DWORD dwOldProtect;
			BYTE InstructionsBuf[ins_len] = { 0 };

			VirtualProtect((LPVOID)KiUserApcDispatcher, ins_len, PAGE_EXECUTE_READWRITE, &dwOldProtect);
			memcpy(InstructionsBuf, KiUserApcDispatcher, ins_len);  //将自身内存里的没有被修改的拷一份下来

			//备份目标进程的内容
			if (!ReadProcessMemory(hProcess, KiUserApcDispatcher, originalInstructions, sizeof(originalInstructions), &Size))
			{
				throw std::runtime_error("RemoveHook 拷贝失败");
			}
			//摘除钩子
			if (!WriteProcessMemory(hProcess, KiUserApcDispatcher, InstructionsBuf, sizeof(InstructionsBuf), &Size))
			{
				throw std::runtime_error("RemoveHook 写入数据失败");
			}
			VirtualProtect((LPVOID)KiUserApcDispatcher, ins_len, dwOldProtect, &dwOldProtect);
			// 关闭进程句柄
			CloseHandle(hProcess);
		}
		else
		{
			logger.Log("打开进程失败 error: %d", GetLastError());
		}
	}
	catch (const std::exception& e)
	{
		OutputDebugStringA(e.what());
	}
}

//恢复钩子
void RestoreKiUserApcDispatcherHook(DWORD dwPid)
{
	try
	{
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
		if (hProcess)
		{
			KiUserApcDispatcher = (PFN_LDRINITIALIZETHUNK)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "KiUserApcDispatcher");
			SIZE_T Size = 0;

			//恢复钩子
			if (!WriteProcessMemory(hProcess, KiUserApcDispatcher, originalInstructions, sizeof(originalInstructions), &Size))
			{
				throw std::runtime_error("RestoreHook 写入数据失败");
			}
			// 关闭进程句柄
			CloseHandle(hProcess);
		}
		else
		{
			logger.Log("打开进程失败 error: %d", GetLastError());
		}
	}
	catch (const std::exception& e)
	{
		OutputDebugStringA(e.what());
	}
}

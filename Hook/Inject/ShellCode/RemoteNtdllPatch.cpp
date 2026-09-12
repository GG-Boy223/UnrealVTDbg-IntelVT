#include "../../dllmain.h"
#include "../../Log/Log.h"
#include "../../Globals.h"
#include "InjectCode.h"

#include <cstring>

constexpr DWORD kPatchLength = 20;

BYTE originalLdrInstructions[kPatchLength] = { 0 };
BYTE originalApcInstructions[kPatchLength] = { 0 };


//摘除钩子
void RemoveHook(DWORD dwPid)
{
	HANDLE hProcess = nullptr;
	SIZE_T size = 0;
	DWORD oldProtect = 0;
	LdrInitializeThunk = (PFN_LDRINITIALIZETHUNK)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "LdrInitializeThunk");
	if (!LdrInitializeThunk)
	{
		logger.Log("RemoveHook LdrInitializeThunk resolve failed");
		return;
	}

	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
	if (!hProcess)
	{
		logger.Log("open process failed error: %d", GetLastError());
		return;
	}

	if (!VirtualProtect((LPVOID)LdrInitializeThunk, kPatchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		logger.Log("RemoveHook VirtualProtect failed error: %d", GetLastError());
		CloseHandle(hProcess);
		return;
	}

	BYTE originalLocalBytes[kPatchLength] = { 0 };
	BYTE instructionsBuf[kPatchLength] = { 0 };
	memcpy(instructionsBuf, LdrInitializeThunk, kPatchLength);

	if (!ReadProcessMemory(hProcess, LdrInitializeThunk, originalLocalBytes, sizeof(originalLocalBytes), &size))
	{
		logger.Log("RemoveHook read memory failed error: %d", GetLastError());
		VirtualProtect((LPVOID)LdrInitializeThunk, kPatchLength, oldProtect, &oldProtect);
		CloseHandle(hProcess);
		return;
	}

	if (!WriteProcessMemory(hProcess, LdrInitializeThunk, instructionsBuf, sizeof(instructionsBuf), &size))
	{
		logger.Log("RemoveHook write memory failed error: %d", GetLastError());
		VirtualProtect((LPVOID)LdrInitializeThunk, kPatchLength, oldProtect, &oldProtect);
		CloseHandle(hProcess);
		return;
	}

	memcpy(originalLdrInstructions, originalLocalBytes, sizeof(originalLdrInstructions));
	VirtualProtect((LPVOID)LdrInitializeThunk, kPatchLength, oldProtect, &oldProtect);
	CloseHandle(hProcess);
}

//恢复钩子
void RestoreHook(DWORD dwPid)
{
	HANDLE hProcess = nullptr;
	SIZE_T size = 0;
	DWORD oldProtect = 0;
	LdrInitializeThunk = (PFN_LDRINITIALIZETHUNK)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "LdrInitializeThunk");
	if (!LdrInitializeThunk)
	{
		logger.Log("RestoreHook LdrInitializeThunk resolve failed");
		return;
	}

	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
	if (!hProcess)
	{
		logger.Log("open process failed error: %d", GetLastError());
		return;
	}

	if (!VirtualProtect((LPVOID)LdrInitializeThunk, kPatchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		logger.Log("RestoreHook VirtualProtect failed error: %d", GetLastError());
		CloseHandle(hProcess);
		return;
	}

	BOOL restored = WriteProcessMemory(
		hProcess,
		LdrInitializeThunk,
		originalLdrInstructions,
		sizeof(originalLdrInstructions),
		&size);
	DWORD restoreError = restored ? 0 : GetLastError();
	VirtualProtect((LPVOID)LdrInitializeThunk, kPatchLength, oldProtect, &oldProtect);
	CloseHandle(hProcess);

	if (!restored)
	{
		logger.Log("RestoreHook write memory failed error: %d", restoreError);
	}
}


//摘除钩子
void RemoveKiUserApcDispatcherHook(DWORD dwPid)
{
	HANDLE hProcess = nullptr;
	SIZE_T size = 0;
	DWORD oldProtect = 0;
	KiUserApcDispatcher = (PFN_LDRINITIALIZETHUNK)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "KiUserApcDispatcher");
	if (!KiUserApcDispatcher)
	{
		logger.Log("RemoveKiUserApcDispatcherHook KiUserApcDispatcher resolve failed");
		return;
	}

	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
	if (!hProcess)
	{
		logger.Log("open process failed error: %d", GetLastError());
		return;
	}

	if (!VirtualProtect((LPVOID)KiUserApcDispatcher, kPatchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		logger.Log("RemoveKiUserApcDispatcherHook VirtualProtect failed error: %d", GetLastError());
		CloseHandle(hProcess);
		return;
	}

	BYTE originalLocalBytes[kPatchLength] = { 0 };
	BYTE instructionsBuf[kPatchLength] = { 0 };
	memcpy(instructionsBuf, KiUserApcDispatcher, kPatchLength);

	if (!ReadProcessMemory(hProcess, KiUserApcDispatcher, originalLocalBytes, sizeof(originalLocalBytes), &size))
	{
		logger.Log("RemoveKiUserApcDispatcherHook read memory failed error: %d", GetLastError());
		VirtualProtect((LPVOID)KiUserApcDispatcher, kPatchLength, oldProtect, &oldProtect);
		CloseHandle(hProcess);
		return;
	}

	if (!WriteProcessMemory(hProcess, KiUserApcDispatcher, instructionsBuf, sizeof(instructionsBuf), &size))
	{
		logger.Log("RemoveKiUserApcDispatcherHook write memory failed error: %d", GetLastError());
		VirtualProtect((LPVOID)KiUserApcDispatcher, kPatchLength, oldProtect, &oldProtect);
		CloseHandle(hProcess);
		return;
	}

	memcpy(originalApcInstructions, originalLocalBytes, sizeof(originalApcInstructions));
	VirtualProtect((LPVOID)KiUserApcDispatcher, kPatchLength, oldProtect, &oldProtect);
	CloseHandle(hProcess);
}

//恢复钩子
void RestoreKiUserApcDispatcherHook(DWORD dwPid)
{
	HANDLE hProcess = nullptr;
	SIZE_T size = 0;
	DWORD oldProtect = 0;
	KiUserApcDispatcher = (PFN_LDRINITIALIZETHUNK)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "KiUserApcDispatcher");
	if (!KiUserApcDispatcher)
	{
		logger.Log("RestoreKiUserApcDispatcherHook KiUserApcDispatcher resolve failed");
		return;
	}

	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
	if (!hProcess)
	{
		logger.Log("open process failed error: %d", GetLastError());
		return;
	}

	if (!VirtualProtect((LPVOID)KiUserApcDispatcher, kPatchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		logger.Log("RestoreKiUserApcDispatcherHook VirtualProtect failed error: %d", GetLastError());
		CloseHandle(hProcess);
		return;
	}

	BOOL restored = WriteProcessMemory(
		hProcess,
		KiUserApcDispatcher,
		originalApcInstructions,
		sizeof(originalApcInstructions),
		&size);
	DWORD restoreError = restored ? 0 : GetLastError();
	VirtualProtect((LPVOID)KiUserApcDispatcher, kPatchLength, oldProtect, &oldProtect);
	CloseHandle(hProcess);

	if (!restored)
	{
		logger.Log("RestoreKiUserApcDispatcherHook write memory failed error: %d", restoreError);
	}
}

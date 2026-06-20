#pragma once

#ifndef _FUNCTION_SET_H
#define _FUNCTION_SET_H

void Hook_DebugActiveProcess();
void UnHook_DebugActiveProcess();

void Hook_DebugActiveProcessStop();
void UnHook_DebugActiveProcessStop();

void Hook_TerminateProcess();
void UnHook_TerminateProcess();

void Hook_NtDebugActiveProcess();
void UnHook_NtDebugActiveProcess();

void Hook_NtRemoveProcessDebug();
void UnHook_NtRemoveProcessDebug();

void Hook_NtTerminateProcess();
void UnHook_NtTerminateProcess();

void Hook_DbgUiIssueRemoteBreakin();
void UnHook_DbgUiIssueRemoteBreakin();

void Hook_DbgUiDebugActiveProcess();
void UnHook_DbgUiDebugActiveProcess();

void Hook_NtCreateUserProcess();
void UnHook_NtCreateUserProcess();

void Hook_WaitForDebugEvent();
void UnHook_WaitForDebugEvent();

void Hook_ContinueDebugEvent();
void UnHook_ContinueDebugEvent();

void Hook_OutputDebugStringA();
void UnHook_OutputDebugStringA();

void Hook_OutputDebugStringW();
void UnHook_OutputDebugStringW();

void Hook_SetThreadContext();
void UnHook_SetThreadContext();

void Hook_GetThreadContext();
void UnHook_GetThreadContext();

void Hook_VirtualProtectEx();
void UnHook_VirtualProtectEx();

void Hook_WriteProcessMemory();
void UnHook_WriteProcessMemory();

void Hook_ReadProcessMemory();
void UnHook_ReadProcessMemory();

void Hook_NtWriteVirtualMemory();
void UnHook_NtWriteVirtualMemory();

void Hook_NtReadVirtualMemory();
void UnHook_NtReadVirtualMemory();

void Hook_NtDebugContinue();
void UnHook_NtDebugContinue();

void Hook_LdrInitializeThunk();

void CleanupDebugSessionState(const char* reason);

#endif // !_FUNCTION_SET_H

#pragma once

#ifndef _EPT_HOOK_H
#define _EPT_HOOK_H

EXTERN_C
VOID UnEptHook();

EXTERN_C
VOID SetupEptHook();

EXTERN_C
NTSTATUS ConfigureEptHookSelection(ULONG64 mask);

EXTERN_C
VOID Hook_DbgkOpenProcessDebugPort();

EXTERN_C
VOID Hook_NtCreateDebugObject();

EXTERN_C
VOID Hook_NtSetInformationDebugObject();

EXTERN_C
VOID Hook_NtRemoveProcessDebug();

EXTERN_C
VOID Hook_NtDebugActiveProcess();

EXTERN_C
VOID Hook_NtWaitForDebugEvent();

EXTERN_C
VOID Hook_KiDispatchException();

EXTERN_C
VOID Hook_PspInsertProcess();

EXTERN_C
VOID Hook_PspInsertThread();

EXTERN_C
VOID Hook_NtDebugContinue();

EXTERN_C
VOID Hook_DbgkMapViewOfSection();

EXTERN_C
VOID Hook_DbgkUnMapViewOfSection();

EXTERN_C
VOID Hook_NtQueryInformationThread();

EXTERN_C
VOID Hook_NtSetInformationThread();

EXTERN_C
VOID Hook_NtQueryInformationProcess();

EXTERN_C
VOID Hook_NtSetInformationProcess();

EXTERN_C
VOID Hook_NtQueryObject();

EXTERN_C
VOID Hook_NtQuerySystemInformation();

EXTERN_C
VOID Hook_NtSystemDebugControl();

EXTERN_C
VOID Hook_NtClose();

EXTERN_C
VOID Hook_NtDuplicateObject();

EXTERN_C
VOID Hook_NtSuspendThread();

EXTERN_C
VOID Hook_NtResumeThread();

EXTERN_C
VOID Hook_DbgkCreateThread();

EXTERN_C
VOID Hook_DbgkExitThread();

EXTERN_C
VOID Hook_DbgkExitProcess();

EXTERN_C
VOID Hook_DbgkForwardException();

EXTERN_C
VOID Hook_DbgkpQueueMessage();

EXTERN_C
VOID Hook_KeStackAttachProcess();

EXTERN_C
VOID Hook_KiStackAttachProcess();

EXTERN_C
VOID Hook_NtProtectVirtualMemory();

EXTERN_C
VOID Hook_MiObtainReferencedVadEx();

EXTERN_C
VOID Hook_MmProtectVirtualMemory();

EXTERN_C
VOID Hook_NtQueryInformationThread();

EXTERN_C
VOID Hook_NtGetContextThread();

EXTERN_C
VOID Hook_NtSetContextThread();

EXTERN_C
VOID Hook_NtShutdownSystem();

EXTERN_C
VOID Hook_NtOpenProcess();

EXTERN_C
VOID Hook_NtReadVirtualMemory();

EXTERN_C
VOID Hook_NtWriteVirtualMemory();

EXTERN_C
VOID Hook_ObReferenceObjectByHandle();

EXTERN_C
VOID Hook_ObReferenceObjectByHandleWithTag();

EXTERN_C
VOID Hook_ObpReferenceObjectByHandleWithTag();

EXTERN_C
VOID Hook_ObfDereferenceObjectWithTag();

EXTERN_C
VOID Hook_ObfDereferenceObject();

EXTERN_C
VOID Hook_MmCopyVirtualMemory();

EXTERN_C
VOID Hook_PspCreateUserContext();

EXTERN_C
VOID Hook_PspCallThreadNotifyRoutines();

EXTERN_C
VOID Hook_NtTerminateProcess();

EXTERN_C
VOID Hook_PspExitThread();

EXTERN_C
VOID Hook_PspCreateThread();

EXTERN_C
VOID Hook_NtCreateThreadEx();

EXTERN_C
VOID Hook_PspAllocateThread();

EXTERN_C
VOID Hook_DbgkpCloseObject();


//win32k.sys
EXTERN_C
VOID Hook_NtUserFindWindowEx();

EXTERN_C
VOID Hook_ValidateHwnd();

EXTERN_C
VOID Hook_NtUserWindowFromPoint();

EXTERN_C
VOID Hook_NtUserSetWindowsHookEx();

EXTERN_C
VOID Hook_NtUserBuildHwndList();

EXTERN_C
VOID Hook_NtUserGetClassName();

// 反-反调试共享判定：虚拟调试目标（attach 路径查 g_DebugProcessList，
// create 路径查 EPROCESS->DebugPort 是否为 Hvm_DebugObject）。
BOOLEAN HvmIsVirtualDebugTargetProcess(_EPROCESS* Process, PVOID* DebugProcess);
BOOLEAN HvmIsCallerVirtualDebugTarget();
BOOLEAN HvmIsProcessHandleVirtualDebugTarget(HANDLE ProcessHandle, KPROCESSOR_MODE PreviousMode);
BOOLEAN HvmIsThreadHandleVirtualDebugTarget(HANDLE ThreadHandle, KPROCESSOR_MODE PreviousMode);

// attach/create 两条路径统一清除 PEB/线程调试痕迹（TitanHide Olly 插件同款目标）。
VOID PrepareVirtualDebugTargetProcess(_EPROCESS* Process);

#endif // !_EPT_HOOK_H

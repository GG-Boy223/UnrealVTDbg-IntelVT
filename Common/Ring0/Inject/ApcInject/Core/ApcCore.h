#pragma once
#include <ntifs.h>
#include "../Types/ApcInjectPebTypes.h"
#include "../Api/ApcInjectNativeApi.h"

/// <summary>
/// Check if thread does not satisfy APC requirements
/// </summary>
/// <param name="pThread">Thread to check</param>
/// <returns>If TRUE - BBLookupProcessThread should skip thread</returns>
BOOLEAN BBSkipThread(IN PETHREAD pThread, IN BOOLEAN isWow64);

/// <summary>
/// Find first thread of the target process
/// </summary>
/// <param name="pProcess">Target process</param>
/// <param name="ppThread">Found thread. Thread object reference count is increased by 1</param>
/// <returns>Status code</returns>
NTSTATUS BBLookupProcessThread(IN PEPROCESS pProcess, OUT PETHREAD* ppThread);

/// <summary>
/// Inject dll using APC
/// Must be running in target process context
/// </summary>
/// <param name="pUserBuf">Injection code</param>
/// <param name="pProcess">Target process</param>
/// <param name="initRVA">Init routine RVA</param>
/// <param name="InitArg">Init routine argument</param>
/// <returns>Status code</returns>
NTSTATUS BBApcInject(IN PINJECT_BUFFER pUserBuf, IN PEPROCESS pProcess, IN ULONG initRVA, IN PCWCHAR InitArg);

/// <summary>
/// Inject using APC to remote thread
/// </summary>
NTSTATUS BBApcInject2(IN PVOID pUserFunc, IN PEPROCESS pProcess, IN ULONG initRVA, IN PCWCHAR InitArg);

/// <summary>
/// Create APC injection for remote process
/// </summary>
NTSTATUS ApcCreateRemoteThread(HANDLE ProcessHandle, PVOID pUserFunc);

/// <summary>
/// Queue user-mode APC to the target thread
/// </summary>
/// <param name="pThread">Target thread</param>
/// <param name="pUserFunc">APC function</param>
/// <param name="Arg1">Argument 1</param>
/// <param name="Arg2">Argument 2</param>
/// <param name="Arg3">Argument 3</param>
/// <param name="bForce">If TRUE - force delivery by issuing special kernel APC</param>
/// <returns>Status code</returns>
NTSTATUS BBQueueUserApc(
    IN PETHREAD pThread,
    IN PVOID pUserFunc,
    IN PVOID Arg1,
    IN PVOID Arg2,
    IN PVOID Arg3,
    IN BOOLEAN bForce
);

/// <summary>
/// Injection APC kernel callbacks
/// </summary>
VOID KernelApcPrepareCallback(
    PKAPC Apc,
    PKNORMAL_ROUTINE* NormalRoutine,
    PVOID* NormalContext,
    PVOID* SystemArgument1,
    PVOID* SystemArgument2
);

VOID KernelApcInjectCallback(
    PKAPC Apc,
    PKNORMAL_ROUTINE* NormalRoutine,
    PVOID* NormalContext,
    PVOID* SystemArgument1,
    PVOID* SystemArgument2
);

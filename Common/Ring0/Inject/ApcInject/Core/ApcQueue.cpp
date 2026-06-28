#include <ntifs.h>
#include "../../../SymbolicAccess/Utils/Log.h"
#include "ApcCore.h"

#pragma warning(disable: 4996) // Preserve legacy pool API for down-level kernel compatibility.

/// <param name="pProcess">Target process</param>
/// <param name="initRVA">Init routine RVA</param>
/// <param name="InitArg">Init routine argument</param>
/// <returns>Status code</returns>
/// <summary>
/// Inject dll using APC
/// Must be running in target process context
/// </summary>
/// <param name="pUserBuf">Injcetion code</param>
NTSTATUS BBApcInject(IN PINJECT_BUFFER pUserBuf, IN PEPROCESS pProcess, IN ULONG initRVA, IN PCWCHAR InitArg)
{
    NTSTATUS status = STATUS_SUCCESS;
    PETHREAD pThread = NULL;

    UNREFERENCED_PARAMETER(initRVA);
    UNREFERENCED_PARAMETER(InitArg);

    // Get suitable thread
    status = BBLookupProcessThread(pProcess, &pThread);

    if (NT_SUCCESS(status))
    {
        //调用LdrLoadDll
        status = BBQueueUserApc(pThread, pUserBuf->code, NULL, NULL, NULL, TRUE);

        // Wait for completion
        if (NT_SUCCESS(status))
        {
            outLog("BBApcInject injection succeeded");
        }
    }
    else
    {
        outLog("BBApcInject injection failed");
    }

    if (pThread)
        ObDereferenceObject(pThread);

    return status;
}

//使用apc注入远程线程
NTSTATUS BBApcInject2(IN PVOID pUserFunc, IN PEPROCESS pProcess, IN ULONG initRVA, IN PCWCHAR InitArg)
{
    NTSTATUS status = STATUS_SUCCESS;
    PETHREAD pThread = NULL;

    UNREFERENCED_PARAMETER(initRVA);
    UNREFERENCED_PARAMETER(InitArg);

    // Get suitable thread
    status = BBLookupProcessThread(pProcess, &pThread);

    if (NT_SUCCESS(status))
    {
        //调用LdrLoadDll
        status = BBQueueUserApc(pThread, pUserFunc, NULL, NULL, NULL, TRUE);

        // Wait for completion
        if (NT_SUCCESS(status))
        {
            outLog("BBApcInject injection succeeded");
        }
    }
    else
    {
        outLog("BBApcInject injection failed");
    }

    if (pThread)
        ObDereferenceObject(pThread);

    return status;
}

NTSTATUS ApcCreateRemoteThread(HANDLE ProcessHandle, PVOID pUserFunc)
{
    KAPC_STATE apc;
    PEPROCESS pProcess = NULL;

    NTSTATUS status = ObReferenceObjectByHandle(ProcessHandle,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        // *PsProcessType,
        KernelMode,
        (PVOID*)&pProcess,
        NULL);
    if (NT_SUCCESS(status))
    {
        KeStackAttachProcess(pProcess, &apc);

        status = BBApcInject2(pUserFunc, pProcess, NULL, NULL);

        KeUnstackDetachProcess(&apc);
        if (pProcess)
            ObDereferenceObject(pProcess);
    }
    //ExReleaseFastMutex(&LongFlagsMutex);
    return status;
}

/// <param name="Arg2">Argument 2</param>
/// <param name="Arg3">Argument 3</param>
/// <param name="bForce">If TRUE - force delivery by issuing special kernel APC</param>
/// <returns>Status code</returns>
/// <summary>
/// Queue user-mode APC to the target thread
/// </summary>
/// <param name="pThread">Target thread</param>
/// <param name="pUserFunc">APC function</param>
/// <param name="Arg1">Argument 1</param>
NTSTATUS BBQueueUserApc(
    IN PETHREAD pThread,
    IN PVOID pUserFunc,
    IN PVOID Arg1,
    IN PVOID Arg2,
    IN PVOID Arg3,
    IN BOOLEAN bForce
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
)
{
    ASSERT(pThread != NULL);
    if (pThread == NULL)
    {
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。
    }

    // Allocate APC
    PKAPC pPrepareApc = NULL;
    PKAPC pInjectApc = (PKAPC)ExAllocatePoolWithTag(NonPagedPool, sizeof(KAPC), BB_POOL_TAG);

    if (pInjectApc == NULL)
    {
        outLog("BBQueueUserApc allocate APC failed");
        return STATUS_NO_MEMORY;
    }

    // Actual APC
    KeInitializeApc(
        pInjectApc, (PKTHREAD)pThread,
        OriginalApcEnvironment, &KernelApcInjectCallback,
        NULL, (PKNORMAL_ROUTINE)(ULONG_PTR)pUserFunc, UserMode, Arg1
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

    // Setup force-delivery APC
    if (bForce)
    {
        pPrepareApc = (PKAPC)ExAllocatePoolWithTag(NonPagedPool, sizeof(KAPC), BB_POOL_TAG);
        if (pPrepareApc == NULL)
        {
            ExFreePoolWithTag(pInjectApc, BB_POOL_TAG);
            outLog("BBQueueUserApc allocate prepare APC failed");
            return STATUS_NO_MEMORY;
        }

        // Actual APC
        KeInitializeApc(
            pPrepareApc, (PKTHREAD)pThread,
            OriginalApcEnvironment, &KernelApcPrepareCallback,
            NULL, NULL, KernelMode, NULL
        // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
        );
    }

    // Insert APC
    if (KeInsertQueueApc(pInjectApc, Arg2, Arg3, 0))
    {
        if (bForce && pPrepareApc)
        {
            if (!KeInsertQueueApc(pPrepareApc, NULL, NULL, 0))
            {
                outLog("BBQueueUserApc prepare APC insert failed, user APC already queued");
                ExFreePoolWithTag(pPrepareApc, BB_POOL_TAG);
            }
        }

        return STATUS_SUCCESS;
    }
    else
    {
        outLog("BBQueueUserApc insert APC failed");

        ExFreePoolWithTag(pInjectApc, BB_POOL_TAG);

        if (pPrepareApc)
            ExFreePoolWithTag(pPrepareApc, BB_POOL_TAG);

        return STATUS_NOT_CAPABLE;
    }
}

// Injection APC routines
//
VOID KernelApcPrepareCallback(
    PKAPC Apc,
    PKNORMAL_ROUTINE* NormalRoutine,
    PVOID* NormalContext,
    PVOID* SystemArgument1,
    PVOID* SystemArgument2
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
)
{
    UNREFERENCED_PARAMETER(NormalRoutine);
    UNREFERENCED_PARAMETER(NormalContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    // Alert current thread
    KeTestAlertThread(UserMode);
    ExFreePoolWithTag(Apc, BB_POOL_TAG);
}

VOID KernelApcInjectCallback(
    PKAPC Apc,
    PKNORMAL_ROUTINE* NormalRoutine,
    PVOID* NormalContext,
    PVOID* SystemArgument1,
    PVOID* SystemArgument2
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
)
{
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    // Skip execution
    if (PsIsThreadTerminating(PsGetCurrentThread()))
    {
        *NormalRoutine = NULL;
    }

    // Fix Wow64 APC
    if (PsGetCurrentProcessWow64Process() != NULL)
    {
        PsWrapApcWow64Thread(NormalContext, (PVOID*)NormalRoutine);
    }

    ExFreePoolWithTag(Apc, BB_POOL_TAG);
}
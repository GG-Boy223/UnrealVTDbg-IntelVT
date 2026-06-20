#include <ntifs.h>
#include "../../../SymbolicAccess/Utils/Log.h"
#include "ApcCore.h"

#pragma warning(disable: 4996) // Preserve legacy pool API for down-level kernel compatibility.

NTSTATUS BBApcInject(IN PINJECT_BUFFER pUserBuf, IN PEPROCESS pProcess, IN ULONG initRVA, IN PCWCHAR InitArg)
{
    NTSTATUS status = STATUS_SUCCESS;
    PETHREAD pThread = NULL;

    UNREFERENCED_PARAMETER(initRVA);
    UNREFERENCED_PARAMETER(InitArg);

    status = BBLookupProcessThread(pProcess, &pThread);

    if (NT_SUCCESS(status))
    {
        status = BBQueueUserApc(pThread, pUserBuf->code, NULL, NULL, NULL, TRUE);

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

NTSTATUS BBApcInject2(IN PVOID pUserFunc, IN PEPROCESS pProcess, IN ULONG initRVA, IN PCWCHAR InitArg)
{
    NTSTATUS status = STATUS_SUCCESS;
    PETHREAD pThread = NULL;

    UNREFERENCED_PARAMETER(initRVA);
    UNREFERENCED_PARAMETER(InitArg);

    status = BBLookupProcessThread(pProcess, &pThread);

    if (NT_SUCCESS(status))
    {
        status = BBQueueUserApc(pThread, pUserFunc, NULL, NULL, NULL, TRUE);

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
    return status;
}

NTSTATUS BBQueueUserApc(
    IN PETHREAD pThread,
    IN PVOID pUserFunc,
    IN PVOID Arg1,
    IN PVOID Arg2,
    IN PVOID Arg3,
    IN BOOLEAN bForce
)
{
    ASSERT(pThread != NULL);
    if (pThread == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PKAPC pPrepareApc = NULL;
    PKAPC pInjectApc = (PKAPC)ExAllocatePoolWithTag(NonPagedPool, sizeof(KAPC), BB_POOL_TAG);

    if (pInjectApc == NULL)
    {
        outLog("BBQueueUserApc allocate APC failed");
        return STATUS_NO_MEMORY;
    }

    KeInitializeApc(
        pInjectApc, (PKTHREAD)pThread,
        OriginalApcEnvironment, &KernelApcInjectCallback,
        NULL, (PKNORMAL_ROUTINE)(ULONG_PTR)pUserFunc, UserMode, Arg1
    );

    if (bForce)
    {
        pPrepareApc = (PKAPC)ExAllocatePoolWithTag(NonPagedPool, sizeof(KAPC), BB_POOL_TAG);
        if (pPrepareApc == NULL)
        {
            ExFreePoolWithTag(pInjectApc, BB_POOL_TAG);
            outLog("BBQueueUserApc allocate prepare APC failed");
            return STATUS_NO_MEMORY;
        }

        KeInitializeApc(
            pPrepareApc, (PKTHREAD)pThread,
            OriginalApcEnvironment, &KernelApcPrepareCallback,
            NULL, NULL, KernelMode, NULL
        );
    }

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

VOID KernelApcPrepareCallback(
    PKAPC Apc,
    PKNORMAL_ROUTINE* NormalRoutine,
    PVOID* NormalContext,
    PVOID* SystemArgument1,
    PVOID* SystemArgument2
)
{
    UNREFERENCED_PARAMETER(NormalRoutine);
    UNREFERENCED_PARAMETER(NormalContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    KeTestAlertThread(UserMode);
    ExFreePoolWithTag(Apc, BB_POOL_TAG);
}

VOID KernelApcInjectCallback(
    PKAPC Apc,
    PKNORMAL_ROUTINE* NormalRoutine,
    PVOID* NormalContext,
    PVOID* SystemArgument1,
    PVOID* SystemArgument2
)
{
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    if (PsIsThreadTerminating(PsGetCurrentThread()))
    {
        *NormalRoutine = NULL;
    }

    if (PsGetCurrentProcessWow64Process() != NULL)
    {
        PsWrapApcWow64Thread(NormalContext, (PVOID*)NormalRoutine);
    }

    ExFreePoolWithTag(Apc, BB_POOL_TAG);
}
#include <ntifs.h>
#include <stdarg.h>
#include "../../../SymbolicAccess/Utils/Log.h"
#include "ApcCore.h"
#include "ModuleResolver.h"
#include "ShellcodeBuilder.h"
#include "UtilityFunctions.h"

/// <param name="pRoutine">Routine to call.</param>
/// <param name="argc">Number of arguments.</param>
/// <param name="...">Arguments</param>
/// <returns>Status code</returns>
/// <summary>
/// Call arbitrary function
/// </summary>
/// <param name="newThread">Perform call in a separate thread</param>
/// <param name="pContext">Loader context</param>
NTSTATUS BBCallRoutine(IN BOOLEAN newThread, IN PMMAP_CONTEXT pContext, IN PVOID pRoutine, IN INT argc, ...)
{
    NTSTATUS status = STATUS_SUCCESS;
    va_list vl;
    BOOLEAN wow64 = PsGetProcessWow64Process(pContext->pProcess) != NULL;

    va_start(vl, argc);
    ULONG ofst = GenPrologueT(wow64, pContext->userMem->code);
    ofst += GenCallTV(wow64, pContext->userMem->code + ofst, pRoutine, argc, vl);
    ofst += GenSyncT(wow64, pContext->userMem->code + ofst, &pContext->userMem->status, pContext->pSetEvent, pContext->hSync);
    ofst += GenEpilogueT(wow64, pContext->userMem->code + ofst, argc * sizeof(ULONG));

    if (newThread)
    {
        status = BBExecuteInNewThread(pContext->userMem->code, NULL, 0/*THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER*/, TRUE, NULL);
    }
    else
    {
        KeResetEvent(pContext->pSync);
        status = BBQueueUserApc(pContext->pWorker, pContext->userMem->code, NULL, NULL, NULL, pContext->noThreads);
        if (NT_SUCCESS(status))
        {
            // Force 60 sec timeout
            LARGE_INTEGER timeout = { 0 };
            timeout.QuadPart = -(10ll * 10 * 1000 * 1000);  // 10s

            status = KeWaitForSingleObject(pContext->pSync, Executive, UserMode, TRUE, &timeout);

            timeout.QuadPart = -(1ll * 10 * 1000);          // 1ms
            KeDelayExecutionThread(KernelMode, TRUE, &timeout);
        }
    }

    va_end(vl);

    return status;
}
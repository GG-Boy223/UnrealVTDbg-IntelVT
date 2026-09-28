#include <ntifs.h>
#include <ntstrsafe.h>
#include "../../../SymbolicAccess/Utils/Log.h"
#include "../Api/ApcInjectNativeApi.h"
#include "ModuleResolver.h"
#include "UtilityFunctions.h"

/// <param name="pContext">Loader context.</param>
/// <param name="name">Name to resolve</param>
/// <param name="resolved">Resolved name if any</param>
/// <returns>Status code</returns>
/// <summary>
///Try to resolve image via SxS isolation
/// </summary>
NTSTATUS BBResolveSxS(
    IN PMMAP_CONTEXT pContext,
    IN PUNICODE_STRING name,
    OUT PUNICODE_STRING resolved
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
)
{
    NTSTATUS status = STATUS_NOT_FOUND;
    UNICODE_STRING ustrNtdll = { 0 };
    if (pContext == nullptr ||
        pContext->pProcess == nullptr ||
        pContext->userMem == nullptr ||
        pContext->userMem->buffer == nullptr ||
        name == nullptr ||
        name->Buffer == nullptr ||
        name->Length == 0 ||
        (name->Length % sizeof(WCHAR)) != 0 ||
        resolved == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    BOOLEAN wow64 = PsGetProcessWow64Process(pContext->pProcess) != NULL;

    typedef struct _STRIBG_BUF
    {
        // Activation context data
        union
        {
            UNICODE_STRING name1;
            UNICODE_STRING32 name132;
        };
        // Activation context data
        union
        {
            UNICODE_STRING name2;
            UNICODE_STRING32 name232;
        };
        // Activation context data
        union
        {
            UNICODE_STRING origName;
            UNICODE_STRING32 origName32;
        };
        // Activation context data
        union
        {
            PUNICODE_STRING pResolved;
            ULONG pResolved32;
        };
        wchar_t origBuf[0x100];
        wchar_t staticBuf[0x200];
    } STRIBG_BUF, * PSTRIBG_BUF;

    PSTRIBG_BUF pStringBuf = (PSTRIBG_BUF)pContext->userMem->buffer;

    RtlUnicodeStringInit(&ustrNtdll, L"ntdll.dll");

    PVOID hNtdll = BBGetUserModule(pContext->pProcess, &ustrNtdll, wow64);
    PVOID pQueryName = BBGetModuleExport(hNtdll, "RtlDosApplyFileIsolationRedirection_Ustr", pContext->pProcess, NULL);

    if (pQueryName == NULL)
    {
        outLog("RtlDosApplyFileIsolationRedirection_Ustr failed");
        return STATUS_NOT_FOUND;
    }

    RtlZeroMemory(pStringBuf->origBuf, sizeof(pStringBuf->origBuf));
    RtlZeroMemory(pStringBuf->staticBuf, sizeof(pStringBuf->staticBuf));

    if (name->Length >= sizeof(pStringBuf->origBuf))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Fill params
    memcpy(pStringBuf->origBuf, name->Buffer, name->Length);
    pStringBuf->origBuf[name->Length / sizeof(WCHAR)] = L'\0';
    if (wow64)
    {
        pStringBuf->origName32.Buffer = (ULONG)(ULONG_PTR)pStringBuf->origBuf;
        pStringBuf->origName32.MaximumLength = sizeof(pStringBuf->origBuf);
        pStringBuf->origName32.Length = name->Length;

        pStringBuf->name132.Buffer = (ULONG)(ULONG_PTR)pStringBuf->staticBuf;
        pStringBuf->name132.MaximumLength = sizeof(pStringBuf->staticBuf);
        pStringBuf->name132.Length = 0;

        pStringBuf->name232.Buffer = 0;
        pStringBuf->name232.Length = pStringBuf->name232.MaximumLength = 0;
    }
    else
    {
        RtlInitUnicodeString(&pStringBuf->origName, pStringBuf->origBuf);
        RtlInitEmptyUnicodeString(&pStringBuf->name1, pStringBuf->staticBuf, sizeof(pStringBuf->staticBuf));
        RtlInitEmptyUnicodeString(&pStringBuf->name2, NULL, 0);
    }

    __try
    {
        // RtlDosApplyFileIsolationRedirection_Ustr
        status = BBCallRoutine(
            FALSE, pContext, pQueryName, 9,
            (PVOID)TRUE, &pStringBuf->origName, NULL,
            &pStringBuf->name1, &pStringBuf->name2, &pStringBuf->pResolved,
            NULL, NULL, NULL
        // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
        );

        if (NT_SUCCESS(status) && NT_SUCCESS(pContext->userMem->status))
        {
            if (wow64)
            {
                ULONG tmp = ((PUNICODE_STRING32)pStringBuf->pResolved32)->Buffer;
                pStringBuf->pResolved = &pStringBuf->name1;
                pStringBuf->pResolved->Buffer = (PWCH)tmp;
            }

            RtlDowncaseUnicodeString(resolved, pStringBuf->pResolved, TRUE);
        }

        return NT_SUCCESS(status) ? pContext->userMem->status : status;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        outLog("BBResolveSxS Exception Code: 0x%X", GetExceptionCode());
        return STATUS_UNHANDLED_EXCEPTION;
    }
}

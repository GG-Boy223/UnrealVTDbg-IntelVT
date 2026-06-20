#include <ntifs.h>
#include <ntstrsafe.h>
#include "../../../SymbolicAccess/ModuleExtender/ModuleExtenderFactory.h"
#include "../../../SymbolicAccess/Utils/Log.h"
#include "ApcCore.h"

#pragma warning(disable: 4996) // Preserve legacy pool API for down-level kernel compatibility.

namespace
{
    struct APC_TEB_OFFSETS
    {
        size_t Teb64Win32ThreadInfo = 0;
        size_t Teb64ActivationContextStackPointer = 0;
        size_t Teb64ThreadLocalStoragePointer = 0;
        size_t Teb64WowTebOffset = 0;
        size_t Teb32ActivationContextStackPointer = 0;
        size_t Teb32ThreadLocalStoragePointer = 0;
        BOOLEAN Initialized = FALSE;
    };
    APC_TEB_OFFSETS g_ApcTebOffsets = {};

    std::optional<size_t> BBGetTebOffset(
        const symbolic_access::ModuleExtender& moduleExtender,
        const char* primaryStructName,
        const char* secondaryStructName,
        const char* memberName)
    {
        auto offset = moduleExtender.GetOffset(primaryStructName, memberName);
        if (offset.has_value())
        {
            return offset;
        }
        if (secondaryStructName != nullptr)
        {
            offset = moduleExtender.GetOffset(secondaryStructName, memberName);
        }
        return offset;
    }

    BOOLEAN BBResolveTebOffsets(IN BOOLEAN needWow64Offsets)
    {
        if (g_ApcTebOffsets.Initialized)
        {
            return TRUE;
        }
        symbolic_access::ModuleExtenderFactory extenderFactory{};
        auto moduleExtender = extenderFactory.Create(L"ntdll.dll");
        if (!moduleExtender.has_value())
        {
            outLog("BBResolveTebOffsets: failed to create symbolic extender for ntdll.dll");
            return FALSE;
        }
        auto teb64Win32ThreadInfo = BBGetTebOffset(*moduleExtender, "_TEB", "TEB", "Win32ThreadInfo");
        auto teb64ActivationContextStackPointer = BBGetTebOffset(*moduleExtender, "_TEB", "TEB", "ActivationContextStackPointer");
        auto teb64ThreadLocalStoragePointer = BBGetTebOffset(*moduleExtender, "_TEB", "TEB", "ThreadLocalStoragePointer");
        auto teb64WowTebOffset = BBGetTebOffset(*moduleExtender, "_TEB", "TEB", "WowTebOffset");
        auto teb32ActivationContextStackPointer = BBGetTebOffset(*moduleExtender, "_TEB32", "TEB32", "ActivationContextStackPointer");
        auto teb32ThreadLocalStoragePointer = BBGetTebOffset(*moduleExtender, "_TEB32", "TEB32", "ThreadLocalStoragePointer");

        if (!teb64Win32ThreadInfo.has_value() ||
            !teb64ActivationContextStackPointer.has_value() ||
            !teb64ThreadLocalStoragePointer.has_value() ||
            !teb64WowTebOffset.has_value() ||
            (needWow64Offsets && (!teb32ActivationContextStackPointer.has_value() || !teb32ThreadLocalStoragePointer.has_value())))
        {
            outLog("BBResolveTebOffsets: failed to resolve required _TEB/_TEB32 fields");
            return FALSE;
        }

        g_ApcTebOffsets.Teb64Win32ThreadInfo = *teb64Win32ThreadInfo;
        g_ApcTebOffsets.Teb64ActivationContextStackPointer = *teb64ActivationContextStackPointer;
        g_ApcTebOffsets.Teb64ThreadLocalStoragePointer = *teb64ThreadLocalStoragePointer;
        g_ApcTebOffsets.Teb64WowTebOffset = *teb64WowTebOffset;
        g_ApcTebOffsets.Teb32ActivationContextStackPointer = teb32ActivationContextStackPointer.value_or(0);
        g_ApcTebOffsets.Teb32ThreadLocalStoragePointer = teb32ThreadLocalStoragePointer.value_or(0);
        g_ApcTebOffsets.Initialized = TRUE;
        outLog("BBResolveTebOffsets: TEB64.Win32ThreadInfo=0x%llX TEB64.ActivationContextStackPointer=0x%llX TEB64.ThreadLocalStoragePointer=0x%llX TEB64.WowTebOffset=0x%llX TEB32.ActivationContextStackPointer=0x%llX TEB32.ThreadLocalStoragePointer=0x%llX",
            g_ApcTebOffsets.Teb64Win32ThreadInfo,
            g_ApcTebOffsets.Teb64ActivationContextStackPointer,
            g_ApcTebOffsets.Teb64ThreadLocalStoragePointer,
            g_ApcTebOffsets.Teb64WowTebOffset,
            g_ApcTebOffsets.Teb32ActivationContextStackPointer,
            g_ApcTebOffsets.Teb32ThreadLocalStoragePointer);
        return TRUE;
    }
}

BOOLEAN BBSkipThread(IN PETHREAD pThread, IN BOOLEAN isWow64)
{
    BOOLEAN skipThread = TRUE;
    PUCHAR pTeb64 = (PUCHAR)PsGetThreadTeb(pThread);
    PEPROCESS pProcess = PsGetThreadProcess(pThread);
    KAPC_STATE apcState = {};
    BOOLEAN attached = FALSE;
    if (!pTeb64)
        return TRUE;
    if (pProcess == NULL)
        return TRUE;
    KeStackAttachProcess(pProcess, &apcState);
    attached = TRUE;
    if (!BBResolveTebOffsets(isWow64))
        goto Exit;
    __try
    {
        // Skip GUI threads. APC to GUI thread causes ZwUserGetMessage to fail.
        if (*(PULONG64)(pTeb64 + g_ApcTebOffsets.Teb64Win32ThreadInfo) != 0)
            goto Exit;
        if (isWow64)
        {
            LONG wowTebOffset = *(PLONG)(pTeb64 + g_ApcTebOffsets.Teb64WowTebOffset);
            PUCHAR pTeb32 = reinterpret_cast<PUCHAR>(reinterpret_cast<LONG_PTR>(pTeb64) + wowTebOffset);
            if (wowTebOffset == 0 || pTeb32 == nullptr)
                goto Exit;
            if (*(PULONG32)(pTeb32 + g_ApcTebOffsets.Teb32ActivationContextStackPointer) == 0)
                goto Exit;
            if (*(PULONG32)(pTeb32 + g_ApcTebOffsets.Teb32ThreadLocalStoragePointer) == 0)
                goto Exit;
        }
        else
        {
            if (*(PULONG64)(pTeb64 + g_ApcTebOffsets.Teb64ActivationContextStackPointer) == 0)
                goto Exit;
            if (*(PULONG64)(pTeb64 + g_ApcTebOffsets.Teb64ThreadLocalStoragePointer) == 0)
                goto Exit;
        }
        skipThread = FALSE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        outLog("BBSkipThread failed to read TEB: 0x%X", GetExceptionCode());
    }
Exit:
    if (attached)
        KeUnstackDetachProcess(&apcState);
    return skipThread;
}

NTSTATUS BBLookupProcessThread(IN PEPROCESS pProcess, OUT PETHREAD* ppThread)
{
    NTSTATUS status = STATUS_SUCCESS;
    ASSERT(ppThread != NULL);
    if (ppThread == NULL)
        return STATUS_INVALID_PARAMETER;

    HANDLE pid = PsGetProcessId(pProcess);
    PVOID pBuf = ExAllocatePoolWithTag(NonPagedPool, 1024 * 1024, BB_POOL_TAG);
    PSYSTEM_PROCESS_INFO pInfo = (PSYSTEM_PROCESS_INFO)pBuf;

    if (!pInfo)
    {
        outLog("BBLookupProcessThread allocate memory failed");
        return STATUS_NO_MEMORY;
    }

    status = ZwQuerySystemInformation(SystemProcessInformation, pInfo, 1024 * 1024, NULL);
    if (!NT_SUCCESS(status))
    {
        ExFreePoolWithTag(pBuf, BB_POOL_TAG);
        return status;
    }

    if (NT_SUCCESS(status))
    {
        status = STATUS_NOT_FOUND;
        for (;;)
        {
            if (pInfo->UniqueProcessId == pid)
            {
                status = STATUS_SUCCESS;
                break;
            }
            else if (pInfo->NextEntryOffset)
                pInfo = (PSYSTEM_PROCESS_INFO)((PUCHAR)pInfo + pInfo->NextEntryOffset);
            else
                break;
        }
    }

    BOOLEAN wow64 = PsGetProcessWow64Process(pProcess) != NULL;

    if (NT_SUCCESS(status))
    {
        status = STATUS_NOT_FOUND;

        for (ULONG i = 0; i < pInfo->NumberOfThreads; i++)
        {
            if (pInfo->Threads[i].ClientId.UniqueThread == PsGetCurrentThreadId())
            {
                continue;
            }

            status = PsLookupThreadByThreadId(pInfo->Threads[i].ClientId.UniqueThread, ppThread);

            if (*ppThread && BBSkipThread(*ppThread, wow64))
            {
                ObDereferenceObject(*ppThread);
                *ppThread = NULL;
                continue;
            }

            break;
        }
    }
    else
    {
        outLog("BBLookupProcessThread failed to find target process");
    }

    if (pBuf)
        ExFreePoolWithTag(pBuf, BB_POOL_TAG);

    if (!*ppThread)
        status = STATUS_NOT_FOUND;

    return status;
}
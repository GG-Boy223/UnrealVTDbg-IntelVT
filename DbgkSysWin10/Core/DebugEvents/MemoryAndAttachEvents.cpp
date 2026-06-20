#include "DebugEventsCommon.h"
#include "../../Driver.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"

NTSTATUS NtProtectVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG flNewProtect, PULONG flOldProtect)
{
    DbgkRuntimeCallGuard runtimeGuard;
    NTSTATUS status = Original_NtProtectVirtualMemory(ProcessHandle, BaseAddress, RegionSize, flNewProtect, flOldProtect);
    return status;
}

size_t g_ptrLongFlags = NULL;

PMMVAD MiObtainReferencedVadEx(PVOID StartingAddress, char a2, PNTSTATUS status)
{
    DbgkRuntimeCallGuard runtimeGuard;
    PMMVAD FoundVad = Original_MiObtainReferencedVadEx(StartingAddress, a2, status);
    if (DbgkIsDriverRuntimeUnloading())
    {
        return FoundVad;
    }
    if (FoundVad)
    {
        PDEBUG_PROCESS DebugProcess;
        _EPROCESS* Process = (_EPROCESS*)PsGetCurrentProcess();

        if (IsDebugTargetProcess(Process, &DebugProcess))
        {
            size_t ptr_LongFlags = (size_t)FoundVad + mmvad_short_offset::LongFlags;
            unsigned long LongFlags = *(unsigned long*)ptr_LongFlags;
            if (LongFlags == 0x188)
            {
                g_ptrLongFlags = ptr_LongFlags;
            }
        }
    }
    return FoundVad;
}

NTSTATUS MmProtectVirtualMemory(_EPROCESS* sourceProcess,
    _EPROCESS* TargetProcess,
    PVOID* BaseAddress,
    PSIZE_T RegionSize,
    WIN32_PROTECTION_MASK NewProtectWin32,
    PWIN32_PROTECTION_MASK LastProtect)
{
    DbgkRuntimeCallGuard runtimeGuard;

    NTSTATUS status = Original_MmProtectVirtualMemory(sourceProcess, TargetProcess, BaseAddress, RegionSize, NewProtectWin32, LastProtect);

    if (DbgkIsDriverRuntimeUnloading())
    {
        return status;
    }

    if (status == STATUS_INVALID_PAGE_PROTECTION)
    {
        if (IsDebugger((PEPROCESS)sourceProcess))
        {
            if (g_ptrLongFlags)
            {
                if (*(unsigned long*)g_ptrLongFlags == 0x188)
                {
                    *(unsigned long*)g_ptrLongFlags = 0x3A0;

                    status = Original_MmProtectVirtualMemory(sourceProcess, TargetProcess, BaseAddress, RegionSize, NewProtectWin32, LastProtect);
                    *(unsigned long*)g_ptrLongFlags = 0x188;
                    g_ptrLongFlags = NULL;
                }
            }
        }
    }
    return status;
}

VOID NewKeStackAttachProcess(_Inout_ PRKPROCESS PROCESS, _Out_ PRKAPC_STATE ApcState)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        Original_KeStackAttachProcess(PROCESS, ApcState);
        return;
    }

    if (IsDebugger((PEPROCESS)PROCESS))
    {
        WCHAR SubStr[256] = { 0 };
        UNICODE_STRING ImageFileName, PassImage;
        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
        if (NT_SUCCESS(Status))
        {
            RtlInitUnicodeString(&ImageFileName, SubStr);
            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
            {
                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
                {
                    goto pass;
                }
            }
        }

    }
pass:
    Original_KeStackAttachProcess(PROCESS, ApcState);
}

VOID NewKiStackAttachProcess(_KPROCESS* Process, BOOLEAN a2, _KAPC_STATE* ApcState)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        Original_KiStackAttachProcess(Process, a2, ApcState);
        return;
    }

    if (IsDebugger((PEPROCESS)Process))
    {
        WCHAR SubStr[256] = { 0 };
        UNICODE_STRING ImageFileName, PassImage;
        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
        if (NT_SUCCESS(Status))
        {
            RtlInitUnicodeString(&ImageFileName, SubStr);
            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
            {
                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
                {
                    goto pass;
                }
            }
        }

    }
pass:
    Original_KiStackAttachProcess(Process, a2, ApcState);
}

NTSTATUS NewNtTerminateProcess(__in_opt HANDLE ProcessHandle, __in NTSTATUS ExitStatus)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtTerminateProcess(ProcessHandle, ExitStatus);
    }

    _EPROCESS* Process;
    NTSTATUS Status = ObReferenceObjectByHandle(ProcessHandle,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        KernelMode,
        (PVOID*)&Process,
        NULL);
    if (NT_SUCCESS(Status))
    {
        ObDereferenceObject(Process);
    }
    return Original_NtTerminateProcess(ProcessHandle, ExitStatus);
}

NTSTATUS NewNtSuspendThread(__in HANDLE ThreadHandle, __out_opt PULONG PreviousSuspendCount)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtSuspendThread(ThreadHandle, PreviousSuspendCount);
    }

    _ETHREAD* Thread;
    NTSTATUS ntStatus = Original_NtSuspendThread(ThreadHandle, PreviousSuspendCount);

    KPROCESSOR_MODE Mode = KeGetPreviousMode();

    __try
    {
        if (Mode != KernelMode)
        {
            if (ARGUMENT_PRESENT(PreviousSuspendCount))
            {
                ProbeForRead(PreviousSuspendCount, sizeof(ULONG), sizeof(PULONG));

                NTSTATUS Status = ObReferenceObjectByHandle(ThreadHandle,
                    THREAD_ALL_ACCESS,
                    *PsThreadType,
                    KernelMode,
                    (PVOID*)&Thread,
                    NULL);
                if (NT_SUCCESS(Status))
                {
                    _EPROCESS* pProcess = (_EPROCESS*)PsGetThreadProcess((PETHREAD)Thread);
                    WCHAR SubStr[256] = { 0 };
                    UNICODE_STRING ImageFileName, PassImage;
                    NTSTATUS Status = GetProcessName((PEPROCESS)pProcess, &SubStr[0]);
                    if (NT_SUCCESS(Status))
                    {
                        RtlInitUnicodeString(&ImageFileName, SubStr);
                        RtlInitUnicodeString(&PassImage, L"TL.exe");
                        if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
                        {
                            size_t ptr_SuspendCount = (size_t)Thread + kthread_offset::SuspendCount;
                            DbgPrint("ptr_SuspendCount: %p    CurrentSuspendCount: %d\n", ptr_SuspendCount, *(char*)ptr_SuspendCount);
                        }
                    }

                    ObDereferenceObject(Thread);
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
    return ntStatus;
}

NTSTATUS NewNtResumeThread(__in HANDLE ThreadHandle, __out_opt PULONG PreviousSuspendCount)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtResumeThread(ThreadHandle, PreviousSuspendCount);
    }

    if (PsGetCurrentProcessId() == (HANDLE)g_TL_Game_pid)
    {
        DbgPrint("\xB5\xB1\xC7\xB0\xB5\xF7\xD3\xC3\xBD\xF8\xB3\xCC\xCA\xC7\x54\x4C\x20\x20\xB5\xB1\xC7\xB0\xB5\xF7\xD3\xC3\xCF\xDF\xB3\xCC\x3A\x20\x25\x64", PsGetCurrentThreadId());
        return STATUS_SUCCESS;
    }

    NTSTATUS ntStatus = Original_NtResumeThread(ThreadHandle, PreviousSuspendCount);

    return ntStatus;
}

NTSTATUS NewNtQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _Out_ PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtQueryInformationThread(ThreadHandle,
            ThreadInformationClass,
            ThreadInformation,
            ThreadInformationLength,
            ReturnLength);
    }

    if (ThreadInformationClass == ThreadSuspendCount)
    {
        PrintProcessName((_EPROCESS*)PsGetCurrentProcess());
        return STATUS_UNSUCCESSFUL;
    }
    return Original_NtQueryInformationThread(ThreadHandle,
        ThreadInformationClass,
        ThreadInformation,
        ThreadInformationLength,
        ReturnLength);
}

HANDLE NewNtUserQueryWindow(IN HWND hwnd, IN WINDOWINFOCLASS WindowInfo)
{
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(WindowInfo);

    return NULL;
}

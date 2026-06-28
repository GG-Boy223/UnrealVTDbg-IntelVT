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
            //DbgBreakPoint();
            size_t ptr_LongFlags = (size_t)FoundVad + mmvad_short_offset::LongFlags;
            unsigned long LongFlags = *(unsigned long*)ptr_LongFlags;
            //DbgPrint("LongFlags: %X\n", LongFlags);
            if (LongFlags == 0x188)
            {
                //*(unsigned long*)ptr_LongFlags = 0x3A0;
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
                    //恢复
                    *(unsigned long*)g_ptrLongFlags = 0x188;
                    //还原
                    // *(unsigned long*)g_ptrLongFlags = 0x188;
                    g_ptrLongFlags = NULL;
                }
            }
        }
    }
    //ExReleaseFastMutex(&LongFlagsMutex);
    return status;
}

VOID NewKeStackAttachProcess(_Inout_ PRKPROCESS PROCESS, _Out_ PRKAPC_STATE ApcState)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        Original_KeStackAttachProcess(PROCESS, ApcState);
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    if (IsDebugger((PEPROCESS)PROCESS))
    {
        //获取当前调用进程
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
                    //发现是白名单进程就放行
                    goto pass;
                }
            }
        }

    }
pass:
    Original_KeStackAttachProcess(PROCESS, ApcState);
}

//pass:
//VOID NewKiStackAttachProcess(_KPROCESS* Process, BOOLEAN a2, _KAPC_STATE* ApcState)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KiStackAttachProcess(Process, a2, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)Process))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KiStackAttachProcess(Process, a2, ApcState);
//}
VOID NewKiStackAttachProcess(_KPROCESS* Process, BOOLEAN a2, _KAPC_STATE* ApcState)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        Original_KiStackAttachProcess(Process, a2, ApcState);
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    if (IsDebugger((PEPROCESS)Process))
    {
        //获取当前调用进程
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
                    //发现是白名单进程就放行
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

    _EPROCESS* Process; //进程对象
    NTSTATUS Status = ObReferenceObjectByHandle(ProcessHandle,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        // *PsProcessType,
        KernelMode,
        (PVOID*)&Process,
        NULL);
    if (NT_SUCCESS(Status))
    {
        ObDereferenceObject(Process); //释放进程对象
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
                    // *PsThreadType,
                    KernelMode,
                    (PVOID*)&Thread,
                    NULL);
                if (NT_SUCCESS(Status))
                {
                    _EPROCESS* pProcess = (_EPROCESS*)PsGetThreadProcess((PETHREAD)Thread);
                    //获取当前调用进程
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
    //        }
    //    }
    //}
    //__except (EXCEPTION_EXECUTE_HANDLER) {
    //    return ntStatus;
    return ntStatus;
}

NTSTATUS NewNtResumeThread(__in HANDLE ThreadHandle, __out_opt PULONG PreviousSuspendCount)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtResumeThread(ThreadHandle, PreviousSuspendCount);
    }

    _ETHREAD* Thread;

    if (PsGetCurrentProcessId() == (HANDLE)g_TL_Game_pid)
    {
        DbgPrint("当前调用进程是TL  当前调用线程: %d", PsGetCurrentThreadId());
        return STATUS_SUCCESS;
    }

    NTSTATUS ntStatus = Original_NtResumeThread(ThreadHandle, PreviousSuspendCount);

    //        }
    //    }
    //}
    //__except (EXCEPTION_EXECUTE_HANDLER) {
    //    return ntStatus;
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
    ////判断当前调用者是否是调试器进程
    //if (!IsDebugger(PsGetCurrentProcess()))
    //{
    //    //根据窗口句柄获得窗口对象
    //    PWND pwnd = Sys_ValidateHwnd(hwnd);
    //    if (pwnd)
    //    {
    //        //根据线程对象获得进程对象
    //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
    //    }
    //}
    //NtUserQueryWindow(hwnd, WindowInfo);
    return NULL;
}

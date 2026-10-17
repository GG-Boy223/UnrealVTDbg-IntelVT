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

namespace
{
    // MiObtainReferencedVadEx 与 MmProtectVirtualMemory 属于同一次 NtProtectVirtualMemory 调用链，
    // 但两者之间隔着原函数的返回，不能依赖栈上传参。旧的全局 size_t 会让并发调试线程互相覆盖：
    // A 线程记录的 VAD 指针可能被 B 线程消费，且该 VAD 可能已被释放（池内存破坏）。
    // 按线程 ID 隔离槽位，只允许记录者本人消费；错过匹配的线程退回原有失败语义。
    struct LONG_FLAGS_SLOT
    {
        unsigned long long thread_id;
        size_t pointer;
    };

    LONG_FLAGS_SLOT g_longFlagsSlot = {};
}

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
                g_longFlagsSlot.thread_id =
                    static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(PsGetCurrentThreadId()));
                g_longFlagsSlot.pointer = ptr_LongFlags;
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
            const unsigned long long currentThreadId =
                static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(PsGetCurrentThreadId()));
            if (g_longFlagsSlot.pointer != 0 &&
                g_longFlagsSlot.thread_id == currentThreadId)
            {
                size_t const ptrLongFlags = g_longFlagsSlot.pointer;
                g_longFlagsSlot.pointer = 0;
                g_longFlagsSlot.thread_id = 0;

                if (*(unsigned long*)ptrLongFlags == 0x188)
                {
                    *(unsigned long*)ptrLongFlags = 0x3A0;

                    status = Original_MmProtectVirtualMemory(sourceProcess, TargetProcess, BaseAddress, RegionSize, NewProtectWin32, LastProtect);
                    //恢复
                    *(unsigned long*)ptrLongFlags = 0x188;
                    //还原
                    // *(unsigned long*)g_ptrLongFlags = 0x188;
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

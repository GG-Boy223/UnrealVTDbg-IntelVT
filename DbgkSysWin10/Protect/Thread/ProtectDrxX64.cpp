#include "../../Driver.h"
#include "../../ntos/inc/mmtypes.h"
#include "../../ntos/inc/ntdbg.h"
#include "../../ntos/inc/ketypes.h"
#include "../../ntos/inc/extypes.h"
#include "../../ntos/inc/ntosdef.h"
#include "../../ntos/inc/amd64.h"
#include "../../ntos/inc/mi.h"
#include "../../ntos/inc/pstypes.h"
#include "../../ntos/inc/obtypes.h"
#include "../../ntos/inc/peb_teb.h"
#include "../../Log/log.h"
#include "../../List/MyList.h"
#include "../../ntos/inc/ntlpcapi.h"
#include "../../ntos/inc/psp.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"
#include "ProtectDrxX64.h"

namespace
{
    const THREADINFOCLASS kThreadWow64Context = (THREADINFOCLASS)29;
    const ULONG kWow64ContextDr0Offset = 0x04;
    const ULONG kWow64ContextDr1Offset = 0x08;
    const ULONG kWow64ContextDr2Offset = 0x0C;
    const ULONG kWow64ContextDr3Offset = 0x10;
    const ULONG kWow64ContextDr6Offset = 0x14;
    const ULONG kWow64ContextDr7Offset = 0x18;
    const ULONG kWow64ContextEFlagsOffset = 0xC0;

    VOID ApplyWow64ContextShadow(PVOID ThreadInformation, PMINI_CONTEXT_WOW64 Context)
    {
        PUCHAR contextBytes = (PUCHAR)ThreadInformation;
        *(ULONG*)(contextBytes + kWow64ContextDr0Offset) = Context->Dr0;
        *(ULONG*)(contextBytes + kWow64ContextDr1Offset) = Context->Dr1;
        *(ULONG*)(contextBytes + kWow64ContextDr2Offset) = Context->Dr2;
        *(ULONG*)(contextBytes + kWow64ContextDr3Offset) = Context->Dr3;
        *(ULONG*)(contextBytes + kWow64ContextDr6Offset) = Context->Dr6;
        *(ULONG*)(contextBytes + kWow64ContextDr7Offset) = Context->Dr7;
        *(ULONG*)(contextBytes + kWow64ContextEFlagsOffset) = Context->EFlags;
    }
}

//32位进程调用这个函数获取Context
NTSTATUS NtQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _Inout_ PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtQueryInformationThread(ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength, ReturnLength);
    }

    ASSERT(Original_NtQueryInformationThread);
    NTSTATUS Status = Original_NtQueryInformationThread(ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength, ReturnLength);
    if (NT_SUCCESS(Status) && ThreadInformationClass == kThreadWow64Context)
    {
        PDEBUG_PROCESS DebugProcess;
        // 以下是处理pubg的硬件断点占坑的情况
        // 判断当前进程是否是被调试的进程
        // 如果是被调试的进程则设置shadow drx里的内容
        if (IsDebugTargetProcess((_EPROCESS*)PsGetCurrentProcess(), &DebugProcess))
        {
            ExAcquireFastMutex(&DebugProcess->Mutex);
            if (DebugProcess->wow64Context.initializated)
            {
                ApplyWow64ContextShadow(ThreadInformation, &DebugProcess->wow64Context);
            }
            ExReleaseFastMutex(&DebugProcess->Mutex);
        }
    }
    // /* Return status */
    return Status;
}

//64位进程调用此函数获取Context
NTSTATUS NtGetContextThread(_In_ HANDLE hThread, _Inout_ PCONTEXT ThreadContext)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtGetContextThread(hThread, ThreadContext);
    }

    PDEBUG_PROCESS DebugProcess;
    NTSTATUS Status = Original_NtGetContextThread(hThread, ThreadContext);

    if (NT_SUCCESS(Status))
    {
        if (IsDebugTargetProcess((_EPROCESS*)PsGetCurrentProcess(), &DebugProcess))
        {
            ExAcquireFastMutex(&DebugProcess->Mutex);
            if (DebugProcess->Context.initializated)
            {
                //ThreadContext->EFlags = DebugProcess->Context.EFlags;
                ThreadContext->EFlags = 0x229;
                DbgPrint("DebugProcess->Context.EFlags: %x\n", ThreadContext->EFlags);
            }
            ExReleaseFastMutex(&DebugProcess->Mutex);
        }
    }
    // /* Return status */
    return Status;
}

NTSTATUS NtSetContextThread(
    __in HANDLE ThreadHandle,
    __in PCONTEXT ThreadContext
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtSetContextThread(ThreadHandle, ThreadContext);
    }

    PDEBUG_PROCESS DebugProcess;
    // 以下是处理pubg的硬件断点占坑的情况
    // 判断当前进程是否是被调试的进程
    // 如果是被调试的进程则设置shadow drx里的内容
    if (IsDebugTargetProcess((_EPROCESS*)PsGetCurrentProcess(), &DebugProcess))
    {
        ExAcquireFastMutex(&DebugProcess->Mutex);
        DebugProcess->Context.EFlags = ThreadContext->EFlags;
        DebugProcess->Context.initializated = TRUE;
        ExReleaseFastMutex(&DebugProcess->Mutex);
        DbgPrint("[set]ThreadContext->EFlags: %x\n", ThreadContext->EFlags);
    }
    return Original_NtSetContextThread(ThreadHandle, ThreadContext);
}

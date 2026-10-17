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
#include "../../Hooks/EptHook/EptHook.h"
#include "../../../Common/Ring0/Hvm/DebuggerVisibilityPolicy.h"
#include "ProtectDrxX64.h"

namespace
{
    const THREADINFOCLASS kThreadWow64Context = static_cast<THREADINFOCLASS>(29);
    const THREADINFOCLASS kThreadHideFromDebugger = static_cast<THREADINFOCLASS>(0x11);

    // WOW64_CONTEXT 布局（与 TitanHide/undocumented.h 一致）：
    // ContextFlags=0x00, Dr0..Dr3=0x04..0x10, Dr6=0x14, Dr7=0x18。
    const ULONG kWow64ContextFlagsOffset = 0x00;
    const ULONG kWow64ContextDr0Offset = 0x04;
    const ULONG kWow64ContextDr1Offset = 0x08;
    const ULONG kWow64ContextDr2Offset = 0x0C;
    const ULONG kWow64ContextDr3Offset = 0x10;
    const ULONG kWow64ContextDr6Offset = 0x14;
    const ULONG kWow64ContextDr7Offset = 0x18;
    const ULONG kWow64ContextDebugRegisterEndOffset = 0x1C;

    const ULONG kContextDebugRegisterFlag = 0x10;

    static_assert(
        static_cast<unsigned long>(kThreadHideFromDebugger) ==
            hvm_debug_visibility::kThreadHideFromDebugger,
        "ThreadHideFromDebugger must match the shared visibility policy");
    static_assert(
        static_cast<unsigned long>(kThreadWow64Context) ==
            hvm_debug_visibility::kThreadWow64Context,
        "ThreadWow64Context must match the shared visibility policy");
    static_assert(
        kWow64ContextDebugRegisterEndOffset ==
            hvm_debug_visibility::kWow64ContextDebugRegisterEndOffset,
        "WOW64 debug-register context length must match the shared visibility policy");

    VOID ClearWow64DebugRegisters(PVOID ThreadInformation)
    {
        PUCHAR bytes = (PUCHAR)ThreadInformation;
        *(ULONG*)(bytes + kWow64ContextDr0Offset) = 0;
        *(ULONG*)(bytes + kWow64ContextDr1Offset) = 0;
        *(ULONG*)(bytes + kWow64ContextDr2Offset) = 0;
        *(ULONG*)(bytes + kWow64ContextDr3Offset) = 0;
        *(ULONG*)(bytes + kWow64ContextDr6Offset) = 0;
        *(ULONG*)(bytes + kWow64ContextDr7Offset) = 0;
    }

    VOID ClearNativeDebugRegisters(PCONTEXT ThreadContext)
    {
        ThreadContext->Dr0 = 0;
        ThreadContext->Dr1 = 0;
        ThreadContext->Dr2 = 0;
        ThreadContext->Dr3 = 0;
        ThreadContext->Dr6 = 0;
        ThreadContext->Dr7 = 0;
#ifdef _WIN64
        ThreadContext->LastBranchToRip = 0;
        ThreadContext->LastBranchFromRip = 0;
        ThreadContext->LastExceptionToRip = 0;
        ThreadContext->LastExceptionFromRip = 0;
#endif
    }
}

// TitanHide 对照：NtQueryInformationThread
//   - ThreadHideFromDebugger：被调试目标查询时返回 TRUE；
//   - ThreadWow64Context：被调试目标查询 WOW64 上下文时清零 DR0-DR3/DR6/DR7。
// 其余信息类原样转发，避免旧实现无条件拒绝 ThreadSuspendCount 造成的调试器语义破坏。
NTSTATUS NtQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _Inout_ PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtQueryInformationThread(
            ThreadHandle,
            ThreadInformationClass,
            ThreadInformation,
            ThreadInformationLength,
            ReturnLength);
    }

    ASSERT(Original_NtQueryInformationThread);
    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    const hvm_debug_visibility::ProcessorMode visibilityMode =
        hvm_debug_visibility::ModeFromKernelFlag(previousMode == KernelMode);

    if (previousMode != KernelMode &&
        ThreadInformationClass == kThreadHideFromDebugger)
    {
        const NTSTATUS status = Original_NtQueryInformationThread(
            ThreadHandle,
            ThreadInformationClass,
            ThreadInformation,
            ThreadInformationLength,
            ReturnLength);

        if (NT_SUCCESS(status) &&
            ThreadInformation != NULL &&
            hvm_debug_visibility::ShouldHideThreadFromDebuggerQuery(
                visibilityMode,
                static_cast<unsigned long>(ThreadInformationClass),
                ThreadInformationLength,
                HvmIsCallerVirtualDebugTarget(),
                HvmIsThreadHandleVirtualDebugTarget(ThreadHandle, previousMode)))
        {
            __try
            {
                ULONG tempReturnLength = 0;
                const BOOLEAN hasReturnLength = ARGUMENT_PRESENT(ReturnLength);
                if (hasReturnLength)
                {
                    ProbeForWrite(ReturnLength, sizeof(ULONG), sizeof(ULONG));
                    tempReturnLength = *ReturnLength;
                }
                ProbeForWrite(ThreadInformation, sizeof(BOOLEAN), sizeof(BOOLEAN));
                *(PBOOLEAN)ThreadInformation = TRUE;
                if (hasReturnLength)
                {
                    // 防 VMProtect 3.9.5+ 把 ReturnLength 与输出缓冲区重叠。
                    *ReturnLength = tempReturnLength;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return GetExceptionCode();
            }
        }
        return status;
    }

    if (ThreadInformation != NULL &&
        hvm_debug_visibility::ShouldStripThreadDebugRegisters(
            visibilityMode,
            static_cast<unsigned long>(ThreadInformationClass),
            ThreadInformationLength,
            HvmIsCallerVirtualDebugTarget(),
            HvmIsThreadHandleVirtualDebugTarget(ThreadHandle, previousMode)))
    {
        ULONG originalContextFlags = 0;
        BOOLEAN debugRegistersRequested = FALSE;
        BOOLEAN hidden = TRUE;

        __try
        {
            PULONG contextFlags = (PULONG)((PUCHAR)ThreadInformation + kWow64ContextFlagsOffset);
            ProbeForWrite(contextFlags, sizeof(ULONG), sizeof(ULONG));
            originalContextFlags = *contextFlags;
            *contextFlags = originalContextFlags & ~kContextDebugRegisterFlag;
            debugRegistersRequested = (*contextFlags != originalContextFlags);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            hidden = FALSE;
        }

        const NTSTATUS status = Original_NtQueryInformationThread(
            ThreadHandle,
            ThreadInformationClass,
            ThreadInformation,
            ThreadInformationLength,
            ReturnLength);

        if (hidden)
        {
            __try
            {
                *(PULONG)((PUCHAR)ThreadInformation + kWow64ContextFlagsOffset) = originalContextFlags;
                if (NT_SUCCESS(status) && debugRegistersRequested)
                {
                    ClearWow64DebugRegisters(ThreadInformation);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
        return status;
    }

    return Original_NtQueryInformationThread(
        ThreadHandle,
        ThreadInformationClass,
        ThreadInformation,
        ThreadInformationLength,
        ReturnLength);
}

// TitanHide 对照：NtGetContextThread。
// 仅当调用者与被查询线程都属于虚拟调试目标时剥离 DRx；
// 调试器自身（非虚拟目标）读上下文仍拿到真实值，由 Hook.dll 负责合成断点视图。
NTSTATUS NtGetContextThread(_In_ HANDLE hThread, _Inout_ PCONTEXT ThreadContext)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtGetContextThread(hThread, ThreadContext);
    }

    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    const hvm_debug_visibility::ProcessorMode visibilityMode =
        hvm_debug_visibility::ModeFromKernelFlag(previousMode == KernelMode);
    BOOLEAN hidden = FALSE;
    if (previousMode != KernelMode && ThreadContext != NULL)
    {
        const bool callerTarget = HvmIsCallerVirtualDebugTarget();
        hidden = hvm_debug_visibility::ShouldStripNativeThreadContext(
            visibilityMode,
            callerTarget,
            callerTarget &&
                HvmIsThreadHandleVirtualDebugTarget(hThread, previousMode));
    }

    ULONG originalContextFlags = 0;
    BOOLEAN debugRegistersRequested = FALSE;
    if (hidden)
    {
        __try
        {
            ProbeForWrite(&ThreadContext->ContextFlags, sizeof(ULONG), sizeof(ULONG));
            originalContextFlags = ThreadContext->ContextFlags;
            ThreadContext->ContextFlags = originalContextFlags & ~kContextDebugRegisterFlag;
            debugRegistersRequested = ThreadContext->ContextFlags != originalContextFlags;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            hidden = FALSE;
        }
    }

    const NTSTATUS status = Original_NtGetContextThread(hThread, ThreadContext);

    if (hidden)
    {
        __try
        {
            ThreadContext->ContextFlags = originalContextFlags;
            if (NT_SUCCESS(status) && debugRegistersRequested)
            {
                ClearNativeDebugRegisters(ThreadContext);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    return status;
}

// TitanHide 对照：NtSetContextThread。调用者是虚拟调试目标时，
// 临时剥离 CONTEXT_DEBUG_REGISTERS，阻止目标通过写上下文恢复/探测 DRx。
NTSTATUS NtSetContextThread(
    _In_ HANDLE ThreadHandle,
    _In_ PCONTEXT ThreadContext)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtSetContextThread(ThreadHandle, ThreadContext);
    }

    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    const hvm_debug_visibility::ProcessorMode visibilityMode =
        hvm_debug_visibility::ModeFromKernelFlag(previousMode == KernelMode);
    BOOLEAN hidden = ThreadContext != NULL &&
        hvm_debug_visibility::ShouldStripThreadContextSet(
            visibilityMode,
            previousMode != KernelMode && HvmIsCallerVirtualDebugTarget());

    ULONG originalContextFlags = 0;
    if (hidden)
    {
        __try
        {
            ProbeForWrite(&ThreadContext->ContextFlags, sizeof(ULONG), sizeof(ULONG));
            originalContextFlags = ThreadContext->ContextFlags;
            ThreadContext->ContextFlags = originalContextFlags & ~kContextDebugRegisterFlag;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            hidden = FALSE;
        }
    }

    const NTSTATUS status = Original_NtSetContextThread(ThreadHandle, ThreadContext);

    if (hidden)
    {
        __try
        {
            ThreadContext->ContextFlags = originalContextFlags;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    return status;
}

// TitanHide 对照：NtSetInformationThread(ThreadHideFromDebugger)。
// 被调试目标请求隐藏线程时直接成功返回，但不真正设置 ETHREAD.HideFromDebugger，
// 保证线程创建/退出/异常事件仍可送达调试器。
NTSTATUS HvmNtSetInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _In_ PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtSetInformationThread(
            ThreadHandle,
            ThreadInformationClass,
            ThreadInformation,
            ThreadInformationLength);
    }

    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    if (hvm_debug_visibility::ShouldSuppressThreadHideFromDebuggerSet(
        hvm_debug_visibility::ModeFromKernelFlag(previousMode == KernelMode),
        static_cast<unsigned long>(ThreadInformationClass),
        ThreadInformationLength,
        HvmIsCallerVirtualDebugTarget()))
    {
        if (hvm_debug_visibility::IsCurrentThreadPseudoHandle((ULONG_PTR)ThreadHandle))
        {
            return STATUS_SUCCESS;
        }

        PETHREAD thread = NULL;
        const NTSTATUS status = ObReferenceObjectByHandle(
            ThreadHandle,
            THREAD_SET_INFORMATION,
            *PsThreadType,
            previousMode,
            (PVOID*)&thread,
            NULL);
        if (NT_SUCCESS(status))
        {
            ObDereferenceObject(thread);
            return STATUS_SUCCESS;
        }
        return status;
    }

    // WOW64 的 SetThreadContext 会走 NtSetInformationThread(ThreadWow64Context)，
    // 而不是 native NtSetContextThread。按 TitanHide 语义临时剥离 CONTEXT_DEBUG_REGISTERS，
    // 让内核不读取调用方提供的 DR0-DR3/DR6/DR7，返回后恢复原始 ContextFlags。
    if (ThreadInformation != NULL &&
        hvm_debug_visibility::ShouldStripWow64ContextSet(
            hvm_debug_visibility::ModeFromKernelFlag(previousMode == KernelMode),
            static_cast<unsigned long>(ThreadInformationClass),
            ThreadInformationLength,
            HvmIsCallerVirtualDebugTarget()))
    {
        ULONG originalContextFlags = 0;
        BOOLEAN flagsStripped = FALSE;
        PULONG contextFlags = (PULONG)((PUCHAR)ThreadInformation + kWow64ContextFlagsOffset);

        __try
        {
            ProbeForWrite(contextFlags, sizeof(ULONG), sizeof(ULONG));
            originalContextFlags = *contextFlags;
            *contextFlags = originalContextFlags & ~kContextDebugRegisterFlag;
            flagsStripped = (*contextFlags != originalContextFlags);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            flagsStripped = FALSE;
        }

        const NTSTATUS status = Original_NtSetInformationThread(
            ThreadHandle,
            ThreadInformationClass,
            ThreadInformation,
            ThreadInformationLength);

        if (flagsStripped)
        {
            __try
            {
                *contextFlags = originalContextFlags;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
        return status;
    }

    return Original_NtSetInformationThread(
        ThreadHandle,
        ThreadInformationClass,
        ThreadInformation,
        ThreadInformationLength);
}

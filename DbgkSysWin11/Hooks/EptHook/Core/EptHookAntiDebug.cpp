#include "EptHookCommon.h"

#include "../../../../Common/Ring0/Hvm/DebuggerVisibilityPolicy.h"

// TitanHide 对照实现（Win11 驱动）。
//
// 目标：当被调试进程自己发起反调试查询时，返回与“未被调试”一致的结果。
// 与 TitanHide 的差异：
//   - TitanHide 用 InfinityHook 改写 syscall 表中的目标函数地址；
//   - 本驱动用既有 EPT Hook 在 ntoskrnl 函数入口重定向；
//   - 隐藏范围由“当前虚拟调试目标”决定（attach 路径查 g_DebugProcessList，
//     create 路径查 EPROCESS->DebugPort 是否为本驱动的 Hvm_DebugObject）。
//
// 所有处理器都只在 PreviousMode != KernelMode 时生效，内核内部调用原样放行，
// 与 TitanHide 的 ExGetPreviousMode() 判定一致。

namespace
{
    // WDK 的 OBJECT_INFORMATION_CLASS 只定义到 ObjectTypeInformation，
    // ObjectTypesInformation (3) 需要显式使用数值。
    const OBJECT_INFORMATION_CLASS kObjectTypesInformation =
        static_cast<OBJECT_INFORMATION_CLASS>(3);

    // ntifs.h 没有公开 OBJECT_TYPE_INFORMATION；按内核实际返回布局定义，
    // 字段与 TitanHide/undocumented.h 的 OBJECT_TYPE_INFORMATION 一致。
    typedef struct _HVM_OBJECT_TYPE_INFORMATION
    {
        UNICODE_STRING TypeName;
        ULONG TotalNumberOfObjects;
        ULONG TotalNumberOfHandles;
        ULONG TotalPagedPoolUsage;
        ULONG TotalNonPagedPoolUsage;
        ULONG TotalNamePoolUsage;
        ULONG TotalHandleTableUsage;
        ULONG HighWaterNumberOfObjects;
        ULONG HighWaterNumberOfHandles;
        ULONG HighWaterPagedPoolUsage;
        ULONG HighWaterNonPagedPoolUsage;
        ULONG HighWaterNamePoolUsage;
        ULONG HighWaterHandleTableUsage;
        ULONG InvalidAttributes;
        GENERIC_MAPPING GenericMapping;
        ULONG ValidAccessMask;
        BOOLEAN SecurityRequired;
        BOOLEAN MaintainHandleCount;
        UCHAR TypeIndex;
        CHAR ReservedByte;
        ULONG PoolType;
        ULONG DefaultPagedPoolCharge;
        ULONG DefaultNonPagedPoolCharge;
    } HVM_OBJECT_TYPE_INFORMATION, *PHVM_OBJECT_TYPE_INFORMATION;

    typedef struct _HVM_OBJECT_TYPES_INFORMATION
    {
        ULONG NumberOfTypes;
        HVM_OBJECT_TYPE_INFORMATION TypeInformation[1];
    } HVM_OBJECT_TYPES_INFORMATION, *PHVM_OBJECT_TYPES_INFORMATION;

    typedef struct _HVM_SYSTEM_HYPERVISOR_QUERY_INFORMATION
    {
        BOOLEAN HypervisorConnected;
        BOOLEAN HypervisorDebuggingEnabled;
        BOOLEAN HypervisorPresent;
        BOOLEAN Spare0[5];
        ULONGLONG EnabledEnlightenments;
    } HVM_SYSTEM_HYPERVISOR_QUERY_INFORMATION, *PHVM_SYSTEM_HYPERVISOR_QUERY_INFORMATION;

    typedef struct _HVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO
    {
        USHORT UniqueProcessId;
        USHORT CreatorBackTraceIndex;
        UCHAR ObjectTypeIndex;
        UCHAR HandleAttributes;
        USHORT HandleValue;
        PVOID Object;
        ULONG GrantedAccess;
    } HVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO, *PHVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO;

    typedef struct _HVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
    {
        PVOID Object;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR HandleValue;
        ULONG GrantedAccess;
        USHORT CreatorBackTraceIndex;
        USHORT ObjectTypeIndex;
        ULONG HandleAttributes;
        ULONG Reserved;
    } HVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, *PHVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

    typedef struct _HVM_SYSTEM_HANDLE_INFORMATION
    {
        ULONG NumberOfHandles;
        HVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
    } HVM_SYSTEM_HANDLE_INFORMATION, *PHVM_SYSTEM_HANDLE_INFORMATION;

    typedef struct _HVM_SYSTEM_HANDLE_INFORMATION_EX
    {
        ULONG_PTR NumberOfHandles;
        ULONG_PTR Reserved;
        HVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
    } HVM_SYSTEM_HANDLE_INFORMATION_EX, *PHVM_SYSTEM_HANDLE_INFORMATION_EX;

    const PROCESSINFOCLASS kProcessDebugPort = static_cast<PROCESSINFOCLASS>(7);
    const PROCESSINFOCLASS kProcessDebugObjectHandle = static_cast<PROCESSINFOCLASS>(0x1E);
    const PROCESSINFOCLASS kProcessDebugFlags = static_cast<PROCESSINFOCLASS>(0x1F);

    const THREADINFOCLASS kThreadHideFromDebugger = static_cast<THREADINFOCLASS>(0x11);
    const THREADINFOCLASS kThreadWow64Context = static_cast<THREADINFOCLASS>(29);

    const SYSTEM_INFORMATION_CLASS kSystemKernelDebuggerInformation =
        static_cast<SYSTEM_INFORMATION_CLASS>(0x23);
    const SYSTEM_INFORMATION_CLASS kSystemKernelDebuggerInformationEx =
        static_cast<SYSTEM_INFORMATION_CLASS>(0x95);
    const SYSTEM_INFORMATION_CLASS kSystemHypervisorInformation =
        static_cast<SYSTEM_INFORMATION_CLASS>(91);
    const SYSTEM_INFORMATION_CLASS kSystemHandleInformation =
        static_cast<SYSTEM_INFORMATION_CLASS>(16);
    const SYSTEM_INFORMATION_CLASS kSystemExtendedHandleInformation =
        static_cast<SYSTEM_INFORMATION_CLASS>(64);

    const ULONG kSysDbgGetTriageDump = 29;
    const ULONG kSysDbgGetLiveKernelDump = 37;

    const ULONG kThreadCreateFlagsHideFromDebugger = 0x00000004;
    const ULONG kObjectProtectClose = 0x00000001;
    const WCHAR kVirtualDebugObjectTypeName[] = L"Hvm_DebugObject";
    const WCHAR kDebugObjectTypeNameW[] = L"DebugObject";

    const ULONG kEthreadCrossThreadFlagsHideFromDebugger = 0x00000004;

    // PEB/HEAP 偏移来自 ntosdef.h 的 PEB 布局与 TitanHide_OllyDbg/pebhider.cpp：
    //   PEB64: ProcessHeap=0x30, NtGlobalFlag=0xbc, Heap.Flags=0x70, Heap.ForceFlags=0x74
    //   PEB32: ProcessHeap=0x18, NtGlobalFlag=0x68, Heap.Flags=0x40, Heap.ForceFlags=0x44
    const ULONG kPeb64ProcessHeapOffset = 0x30;
    const ULONG kPeb64NtGlobalFlagOffset = 0xBC;
    const ULONG kPeb32ProcessHeapOffset = 0x18;
    const ULONG kPeb32NtGlobalFlagOffset = 0x68;
    const ULONG kHeap64FlagsOffset = 0x70;
    const ULONG kHeap64ForceFlagsOffset = 0x74;
    const ULONG kHeap32FlagsOffset = 0x40;
    const ULONG kHeap32ForceFlagsOffset = 0x44;
    const ULONG kHeapGrowable = 0x00000002;
    const ULONG kPebNtGlobalFlagDebugMask = 0x70;

    const ULONG kPebBeingDebuggedOffset = 0x02;

    static_assert(
        static_cast<unsigned long>(kProcessDebugPort) ==
            hvm_debug_visibility::kProcessDebugPort,
        "ProcessDebugPort must match the shared visibility policy");
    static_assert(
        static_cast<unsigned long>(kProcessDebugObjectHandle) ==
            hvm_debug_visibility::kProcessDebugObjectHandle,
        "ProcessDebugObjectHandle must match the shared visibility policy");
    static_assert(
        static_cast<unsigned long>(kProcessDebugFlags) ==
            hvm_debug_visibility::kProcessDebugFlags,
        "ProcessDebugFlags must match the shared visibility policy");
    static_assert(
        static_cast<unsigned long>(kThreadHideFromDebugger) ==
            hvm_debug_visibility::kThreadHideFromDebugger,
        "ThreadHideFromDebugger must match the shared visibility policy");
    static_assert(
        static_cast<unsigned long>(kSystemKernelDebuggerInformation) ==
            hvm_debug_visibility::kSystemKernelDebuggerInformation,
        "SystemKernelDebuggerInformation must match the shared visibility policy");
    static_assert(
        static_cast<unsigned long>(kSystemKernelDebuggerInformationEx) ==
            hvm_debug_visibility::kSystemKernelDebuggerInformationEx,
        "SystemKernelDebuggerInformationEx must match the shared visibility policy");
    static_assert(
        static_cast<unsigned long>(kSystemHypervisorInformation) ==
            hvm_debug_visibility::kSystemHypervisorInformation,
        "SystemHypervisorInformation must match the shared visibility policy");
    static_assert(
        static_cast<unsigned long>(kSystemHandleInformation) ==
            hvm_debug_visibility::kSystemHandleInformation,
        "SystemHandleInformation must match the shared visibility policy");
    static_assert(
        static_cast<unsigned long>(kSystemExtendedHandleInformation) ==
            hvm_debug_visibility::kSystemExtendedHandleInformation,
        "SystemExtendedHandleInformation must match the shared visibility policy");

    KMUTEX g_AntiDebugNtCloseMutex;
    volatile LONG g_AntiDebugNtCloseMutexReady = 0;
    volatile LONG g_AntiDebugTraceProcessQuery = 0;
    volatile LONG g_AntiDebugTracePrepare = 0;

    BOOLEAN IsAnyVirtualDebugSessionActive()
    {
        return InterlockedCompareExchange(&g_LiveVirtualDebugObjectCount, 0, 0) > 0;
    }

    VOID DecrementCounterSaturating(volatile LONG* Counter)
    {
        for (;;)
        {
            LONG value = *Counter;
            if (value <= 0)
            {
                return;
            }
            if (InterlockedCompareExchange(Counter, value - 1, value) == value)
            {
                return;
            }
        }
    }

    // 读取 EPROCESS->DebugPort 并在调试端口锁保护下校验其对象类型。
    // 只有本驱动创建的 Hvm_DebugObject 才视为 create 路径的虚拟调试目标。
    BOOLEAN IsVirtualDebugPortObject(_EPROCESS* Process)
    {
        if (Process == NULL ||
            Hvm_DbgkDebugObjectType == NULL ||
            eprocess_offset::DebugPort == 0xFFFFFFFF)
        {
            return FALSE;
        }

        PDEBUG_OBJECT port = NULL;
        BOOLEAN referenced = FALSE;

        ExAcquireFastMutex(&DbgkpProcessDebugPortMutex);
        __try
        {
            port = *(PDEBUG_OBJECT*)((PUCHAR)Process + eprocess_offset::DebugPort);
            if (port != NULL)
            {
                referenced = NT_SUCCESS(ObReferenceObjectByPointer(
                    port,
                    0,
                    Hvm_DbgkDebugObjectType,
                    KernelMode));
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            referenced = FALSE;
        }
        ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);

        if (referenced)
        {
            ObDereferenceObject(port);
        }
        return referenced;
    }

    // 判断 Process 是否为本驱动的虚拟调试目标。
    // attach 路径：在 g_DebugProcessList 中且 DebugPort 已被置空；
    // create 路径：EPROCESS->DebugPort 仍指向本驱动的 Hvm_DebugObject。
    BOOLEAN IsVirtualDebugTargetProcess(_EPROCESS* Process, PDEBUG_PROCESS* DebugProcess)
    {
        if (DebugProcess != NULL)
        {
            *DebugProcess = NULL;
        }

        if (Process == NULL || KeGetCurrentIrql() > APC_LEVEL)
        {
            return FALSE;
        }

        // 无任何 Hvm_DebugObject 时直接放行，避免给全系统热路径增加开销。
        if (!IsAnyVirtualDebugSessionActive())
        {
            return FALSE;
        }

        if (IsVirtualDebugPortObject(Process))
        {
            return TRUE;
        }

        return IsDebugTargetProcess(Process, DebugProcess);
    }

    BOOLEAN IsCallerVirtualDebugTarget()
    {
        return IsVirtualDebugTargetProcess((_EPROCESS*)PsGetCurrentProcess(), NULL);
    }

    // 用户态常以 NtCurrentProcess()/NtCurrentThread() 伪句柄查询自身。
    // WOW64 参数可能零扩展为 0x00000000FFFFFFFF/0x00000000FFFFFFFE，
    // 因此同时接受原生 -1/-2 与零扩展形式。
    BOOLEAN IsCurrentProcessPseudoHandle(HANDLE Handle)
    {
        return hvm_debug_visibility::IsCurrentProcessPseudoHandle((ULONG_PTR)Handle);
    }

    BOOLEAN IsCurrentThreadPseudoHandle(HANDLE Handle)
    {
        return hvm_debug_visibility::IsCurrentThreadPseudoHandle((ULONG_PTR)Handle);
    }

    BOOLEAN RangesOverlap(
        const void* First,
        SIZE_T FirstSize,
        const void* Second,
        SIZE_T SecondSize)
    {
        const ULONG_PTR firstBegin = (ULONG_PTR)First;
        const ULONG_PTR firstEnd = firstBegin + FirstSize;
        const ULONG_PTR secondBegin = (ULONG_PTR)Second;
        const ULONG_PTR secondEnd = secondBegin + SecondSize;
        return firstBegin < secondEnd && secondBegin < firstEnd;
    }

    BOOLEAN IsProcessHandleVirtualDebugTarget(HANDLE ProcessHandle, KPROCESSOR_MODE PreviousMode)
    {
        if (ProcessHandle == NULL)
        {
            return FALSE;
        }

        PEPROCESS process = NULL;
        BOOLEAN referenced = FALSE;
        if (IsCurrentProcessPseudoHandle(ProcessHandle))
        {
            process = PsGetCurrentProcess();
        }
        else
        {
            const NTSTATUS status = ObReferenceObjectByHandle(
                ProcessHandle,
                0,
                *PsProcessType,
                PreviousMode,
                (PVOID*)&process,
                NULL);
            if (!NT_SUCCESS(status))
            {
                return FALSE;
            }
            referenced = TRUE;
        }

        BOOLEAN result = IsVirtualDebugTargetProcess((_EPROCESS*)process, NULL);
        if (referenced)
        {
            ObDereferenceObject(process);
        }
        return result;
    }

    BOOLEAN IsThreadHandleVirtualDebugTarget(HANDLE ThreadHandle, KPROCESSOR_MODE PreviousMode)
    {
        if (ThreadHandle == NULL)
        {
            return FALSE;
        }

        PETHREAD thread = NULL;
        BOOLEAN referenced = FALSE;
        if (IsCurrentThreadPseudoHandle(ThreadHandle))
        {
            thread = PsGetCurrentThread();
        }
        else
        {
            const NTSTATUS status = ObReferenceObjectByHandle(
                ThreadHandle,
                0,
                *PsThreadType,
                PreviousMode,
                (PVOID*)&thread,
                NULL);
            if (!NT_SUCCESS(status))
            {
                return FALSE;
            }
            referenced = TRUE;
        }

        BOOLEAN result = IsVirtualDebugTargetProcess((_EPROCESS*)PsGetThreadProcess(thread), NULL);
        if (referenced)
        {
            ObDereferenceObject(thread);
        }
        return result;
    }

    BOOLEAN IsDebugObjectTypeNameInline(PVOID ObjectInformation, ULONG ObjectInformationLength)
    {
        if (ObjectInformation == NULL ||
            ObjectInformationLength < sizeof(HVM_OBJECT_TYPE_INFORMATION) + sizeof(kVirtualDebugObjectTypeName) - sizeof(WCHAR))
        {
            return FALSE;
        }

        __try
        {
            ProbeForRead(ObjectInformation, sizeof(HVM_OBJECT_TYPE_INFORMATION), 1);
            PHVM_OBJECT_TYPE_INFORMATION typeInformation = (PHVM_OBJECT_TYPE_INFORMATION)ObjectInformation;
            if (typeInformation->TypeName.Length != sizeof(kVirtualDebugObjectTypeName) - sizeof(WCHAR))
            {
                return FALSE;
            }

            // NtQueryObject 直接把类型名写在 OBJECT_TYPE_INFORMATION 后面。
            // 不能使用 TypeName.Buffer：VMProtect 3.9.5+ 会把 ReturnLength 与它重叠。
            const WCHAR* inlineName = (const WCHAR*)(typeInformation + 1);
            ProbeForRead((PVOID)inlineName, sizeof(kVirtualDebugObjectTypeName) - sizeof(WCHAR), 1);
            return RtlCompareMemory(
                inlineName,
                kVirtualDebugObjectTypeName,
                sizeof(kVirtualDebugObjectTypeName) - sizeof(WCHAR)) ==
                sizeof(kVirtualDebugObjectTypeName) - sizeof(WCHAR);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return FALSE;
        }
    }

    VOID RewriteVirtualDebugObjectTypeName(PHVM_OBJECT_TYPE_INFORMATION TypeInformation)
    {
        // “Hvm_DebugObject” (30 bytes) 比 “DebugObject” (22 bytes) 长，
        // 原地改短不会越过原 MaximumLength 覆盖后续数据。
        const USHORT newLength = (USHORT)(sizeof(kDebugObjectTypeNameW) - sizeof(WCHAR));
        PWCHAR inlineName = (PWCHAR)(TypeInformation + 1);
        RtlCopyMemory(inlineName, kDebugObjectTypeNameW, sizeof(kDebugObjectTypeNameW) - sizeof(WCHAR));
        TypeInformation->TypeName.Length = newLength;
        TypeInformation->TypeName.Buffer = inlineName;
    }

    VOID RemoveOwnDebugObjectContribution(PHVM_OBJECT_TYPE_INFORMATION TypeInformation)
    {
        // Hvm_DebugObject 类型只由本驱动创建，调用方是虚拟调试目标时
        // 该类型的所有对象/句柄都属于调试会话，直接清零即可完整隐藏；
        // 不依赖句柄副本计数，避免 DuplicateHandle 造成的计数偏差。
        TypeInformation->TotalNumberOfObjects = 0;
        TypeInformation->TotalNumberOfHandles = 0;
    }

    BOOLEAN ReadTargetMemory(
        _EPROCESS* Process,
        PVOID RemoteAddress,
        PVOID LocalBuffer,
        SIZE_T Size)
    {
        if (Sys_MmCopyVirtualMemory == NULL || Process == NULL || RemoteAddress == NULL || LocalBuffer == NULL)
        {
            return FALSE;
        }

        BOOLEAN result = FALSE;
        SIZE_T bytes = 0;
        __try
        {
            result = NT_SUCCESS(Sys_MmCopyVirtualMemory(
                Process,
                RemoteAddress,
                (_EPROCESS*)PsGetCurrentProcess(),
                LocalBuffer,
                Size,
                KernelMode,
                &bytes)) && bytes == Size;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            result = FALSE;
        }
        return result;
    }

    BOOLEAN WriteTargetMemory(
        _EPROCESS* Process,
        PVOID RemoteAddress,
        PVOID LocalBuffer,
        SIZE_T Size)
    {
        if (Sys_MmCopyVirtualMemory == NULL || Process == NULL || RemoteAddress == NULL || LocalBuffer == NULL)
        {
            return FALSE;
        }

        BOOLEAN result = FALSE;
        SIZE_T bytes = 0;
        __try
        {
            result = NT_SUCCESS(Sys_MmCopyVirtualMemory(
                (_EPROCESS*)PsGetCurrentProcess(),
                LocalBuffer,
                Process,
                RemoteAddress,
                Size,
                KernelMode,
                &bytes)) && bytes == Size;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            result = FALSE;
        }
        return result;
    }

    VOID HidePebDebugArtifacts(_EPROCESS* Process, PVOID PebAddress, BOOLEAN IsWow64Peb)
    {
        if (Process == NULL || PebAddress == NULL)
        {
            return;
        }

        const ULONG processHeapOffset = IsWow64Peb ? kPeb32ProcessHeapOffset : kPeb64ProcessHeapOffset;
        const ULONG ntGlobalFlagOffset = IsWow64Peb ? kPeb32NtGlobalFlagOffset : kPeb64NtGlobalFlagOffset;
        const ULONG heapFlagsOffset = IsWow64Peb ? kHeap32FlagsOffset : kHeap64FlagsOffset;
        const ULONG heapForceFlagsOffset = IsWow64Peb ? kHeap32ForceFlagsOffset : kHeap64ForceFlagsOffset;

        UCHAR beingDebugged = 0;
        WriteTargetMemory(Process, (PUCHAR)PebAddress + kPebBeingDebuggedOffset, &beingDebugged, sizeof(beingDebugged));

        ULONG ntGlobalFlag = 0;
        if (ReadTargetMemory(Process, (PUCHAR)PebAddress + ntGlobalFlagOffset, &ntGlobalFlag, sizeof(ntGlobalFlag)))
        {
            ntGlobalFlag &= ~kPebNtGlobalFlagDebugMask;
            WriteTargetMemory(Process, (PUCHAR)PebAddress + ntGlobalFlagOffset, &ntGlobalFlag, sizeof(ntGlobalFlag));
        }

        ULONG_PTR processHeap = 0;
        if (!ReadTargetMemory(Process, (PUCHAR)PebAddress + processHeapOffset, &processHeap, sizeof(processHeap)))
        {
            return;
        }
        if (processHeap == 0)
        {
            return;
        }

        ULONG heapFlags = 0;
        if (ReadTargetMemory(Process, (PVOID)(processHeap + heapFlagsOffset), &heapFlags, sizeof(heapFlags)))
        {
            heapFlags &= kHeapGrowable;
            WriteTargetMemory(Process, (PVOID)(processHeap + heapFlagsOffset), &heapFlags, sizeof(heapFlags));
        }

        ULONG heapForceFlags = 0;
        WriteTargetMemory(Process, (PVOID)(processHeap + heapForceFlagsOffset), &heapForceFlags, sizeof(heapForceFlags));
    }
}

// 对外暴露的判定接口：ProtectDrxX64.cpp 及其他 hook 统一使用。
BOOLEAN HvmIsVirtualDebugTargetProcess(_EPROCESS* Process, PVOID* DebugProcess)
{
    return IsVirtualDebugTargetProcess(Process, (PDEBUG_PROCESS*)DebugProcess);
}

BOOLEAN HvmIsCallerVirtualDebugTarget()
{
    return IsCallerVirtualDebugTarget();
}

BOOLEAN HvmIsProcessHandleVirtualDebugTarget(HANDLE ProcessHandle, KPROCESSOR_MODE PreviousMode)
{
    return IsProcessHandleVirtualDebugTarget(ProcessHandle, PreviousMode);
}

BOOLEAN HvmIsThreadHandleVirtualDebugTarget(HANDLE ThreadHandle, KPROCESSOR_MODE PreviousMode)
{
    return IsThreadHandleVirtualDebugTarget(ThreadHandle, PreviousMode);
}

#define HVM_BACKUP_RETURNLENGTH() \
    ULONG hvmTempReturnLength = 0; \
    if (ARGUMENT_PRESENT(ReturnLength)) \
    { \
        ProbeForWrite(ReturnLength, sizeof(ULONG), 1); \
        hvmTempReturnLength = *ReturnLength; \
    }

#define HVM_RESTORE_RETURNLENGTH() \
    if (ARGUMENT_PRESENT(ReturnLength)) \
    { \
        *ReturnLength = hvmTempReturnLength; \
    }

// NtQueryInformationProcess：对齐 TitanHide 的 ProcessDebugFlags / ProcessDebugPort /
// ProcessDebugObjectHandle 三个查询类。隐藏判定基于 ProcessHandle 指向的进程，
// 而不是调用者 PID，这一点与 TitanHide 的 Hider::IsHidden(pid, ...) 完全一致。
NTSTATUS HvmNtQueryInformationProcess(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtQueryInformationProcess(
            ProcessHandle,
            ProcessInformationClass,
            ProcessInformation,
            ProcessInformationLength,
            ReturnLength);
    }

    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    if (previousMode == KernelMode)
    {
        return Original_NtQueryInformationProcess(
            ProcessHandle,
            ProcessInformationClass,
            ProcessInformation,
            ProcessInformationLength,
            ReturnLength);
    }

    if ((ProcessInformationClass == kProcessDebugPort ||
        ProcessInformationClass == kProcessDebugObjectHandle ||
        ProcessInformationClass == kProcessDebugFlags) &&
        InterlockedIncrement(&g_AntiDebugTraceProcessQuery) <= 2)
    {
        outFile("[AntiDebug] NtQueryInformationProcess class=0x%X callerPid=%u handle=%p",
            (ULONG)ProcessInformationClass,
            (ULONG)(ULONG_PTR)PsGetCurrentProcessId(),
            (PVOID)ProcessHandle);
    }

    if (ProcessInformation != NULL &&
        hvm_debug_visibility::IsProcessDebugObjectHandleQuery(
            hvm_debug_visibility::ProcessorMode::User,
            static_cast<unsigned long>(ProcessInformationClass),
            ProcessInformationLength) &&
        hvm_debug_visibility::ShouldHideProcessDebugObjectHandle(
            hvm_debug_visibility::ProcessorMode::User,
            static_cast<unsigned long>(ProcessInformationClass),
            ProcessInformationLength,
            IsProcessHandleVirtualDebugTarget(ProcessHandle, previousMode)))
    {
        const ULONG handleOutputWidth =
            hvm_debug_visibility::SelectedHandleOutputWidth(ProcessInformationLength);
        __try
        {
            ProbeForWrite(ProcessInformation, handleOutputWidth, handleOutputWidth);
            if (handleOutputWidth == sizeof(HANDLE))
            {
                *(PHANDLE)ProcessInformation = NULL;
            }
            else
            {
                *(PULONG)ProcessInformation = 0;
            }
            if (ARGUMENT_PRESENT(ReturnLength))
            {
                ProbeForWrite(ReturnLength, sizeof(ULONG), 1);
                if (!RangesOverlap(
                    ProcessInformation,
                    handleOutputWidth,
                    ReturnLength,
                    sizeof(ULONG)))
                {
                    *ReturnLength = handleOutputWidth;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
        return STATUS_PORT_NOT_SET;
    }

    const NTSTATUS status = Original_NtQueryInformationProcess(
        ProcessHandle,
        ProcessInformationClass,
        ProcessInformation,
        ProcessInformationLength,
        ReturnLength);

    if (NT_SUCCESS(status) && ProcessInformation != NULL)
    {
        const bool target =
            IsProcessHandleVirtualDebugTarget(ProcessHandle, previousMode);
        if (hvm_debug_visibility::ShouldUseDebugFlagsTrue(
            hvm_debug_visibility::ProcessorMode::User,
            static_cast<unsigned long>(ProcessInformationClass),
            ProcessInformationLength,
            target))
        {
            __try
            {
                HVM_BACKUP_RETURNLENGTH();
                *(PULONG)ProcessInformation = TRUE;
                HVM_RESTORE_RETURNLENGTH();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return GetExceptionCode();
            }
        }
        else if (hvm_debug_visibility::ShouldUseDebugPortZero(
            hvm_debug_visibility::ProcessorMode::User,
            static_cast<unsigned long>(ProcessInformationClass),
            ProcessInformationLength,
            target))
        {
            __try
            {
                HVM_BACKUP_RETURNLENGTH();
                if (ProcessInformationLength >= sizeof(ULONG_PTR))
                {
                    ProbeForWrite(ProcessInformation, sizeof(ULONG_PTR), sizeof(ULONG_PTR));
                    *(PULONG_PTR)ProcessInformation = 0;
                }
                else
                {
                    ProbeForWrite(ProcessInformation, sizeof(ULONG), sizeof(ULONG));
                    *(PULONG)ProcessInformation = 0;
                }
                HVM_RESTORE_RETURNLENGTH();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return GetExceptionCode();
            }
        }
    }

    return status;
}

// NtSetInformationProcess：被调试目标写 ProcessDebugFlags 时直接成功，
// 防止目标通过清除 NoDebugInherit 或设置调试相关标志改变内核状态。
NTSTATUS HvmNtSetInformationProcess(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    IN PVOID ProcessInformation,
    IN ULONG ProcessInformationLength)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtSetInformationProcess(
            ProcessHandle,
            ProcessInformationClass,
            ProcessInformation,
            ProcessInformationLength);
    }

    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    if (previousMode != KernelMode &&
        ProcessInformationClass == kProcessDebugFlags &&
        IsProcessHandleVirtualDebugTarget(ProcessHandle, previousMode))
    {
        return STATUS_SUCCESS;
    }

    return Original_NtSetInformationProcess(
        ProcessHandle,
        ProcessInformationClass,
        ProcessInformation,
        ProcessInformationLength);
}

// NtQueryObject：
//   1) ObjectTypeInformation：被调试目标拿到 Hvm_DebugObject 句柄时，
//      类型名改写为 DebugObject 并扣除本驱动贡献的对象/句柄计数；
//   2) ObjectTypesInformation：遍历内核返回的类型列表，对 Hvm_DebugObject
//      条目做同样处理，击败 NCC Group 的“枚举 DebugObject 类型计数”检测。
NTSTATUS HvmNtQueryObject(
    IN HANDLE Handle OPTIONAL,
    IN OBJECT_INFORMATION_CLASS ObjectInformationClass,
    OUT PVOID ObjectInformation OPTIONAL,
    IN ULONG ObjectInformationLength,
    OUT PULONG ReturnLength OPTIONAL)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtQueryObject(
            Handle,
            ObjectInformationClass,
            ObjectInformation,
            ObjectInformationLength,
            ReturnLength);
    }

    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    if (previousMode == KernelMode)
    {
        return Original_NtQueryObject(
            Handle,
            ObjectInformationClass,
            ObjectInformation,
            ObjectInformationLength,
            ReturnLength);
    }

    const NTSTATUS status = Original_NtQueryObject(
        Handle,
        ObjectInformationClass,
        ObjectInformation,
        ObjectInformationLength,
        ReturnLength);

    if (!NT_SUCCESS(status) ||
        ObjectInformation == NULL ||
        !IsCallerVirtualDebugTarget())
    {
        return status;
    }

    if (ObjectInformationClass == ObjectTypeInformation)
    {
        if (!IsDebugObjectTypeNameInline(ObjectInformation, ObjectInformationLength))
        {
            return status;
        }

        __try
        {
            HVM_BACKUP_RETURNLENGTH();
            ProbeForWrite(ObjectInformation, sizeof(HVM_OBJECT_TYPE_INFORMATION), 1);
            PHVM_OBJECT_TYPE_INFORMATION typeInformation = (PHVM_OBJECT_TYPE_INFORMATION)ObjectInformation;
            RewriteVirtualDebugObjectTypeName(typeInformation);
            RemoveOwnDebugObjectContribution(typeInformation);
            HVM_RESTORE_RETURNLENGTH();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
        return status;
    }

    if (ObjectInformationClass == kObjectTypesInformation)
    {
        __try
        {
            HVM_BACKUP_RETURNLENGTH();

            ULONG capacity = ObjectInformationLength;
            if (ARGUMENT_PRESENT(ReturnLength) && hvmTempReturnLength != 0 && hvmTempReturnLength < capacity)
            {
                capacity = hvmTempReturnLength;
            }

            ProbeForRead(ObjectInformation, sizeof(ULONG), 1);
            ULONG numberOfTypes = *(PULONG)ObjectInformation;
            PUCHAR entry = (PUCHAR)ObjectInformation + sizeof(ULONG);
            entry = (PUCHAR)(((ULONG_PTR)entry + sizeof(PVOID) - 1) & ~(sizeof(PVOID) - 1));

            for (ULONG index = 0; index < numberOfTypes; index++)
            {
                if ((ULONG)(entry - (PUCHAR)ObjectInformation) + sizeof(HVM_OBJECT_TYPE_INFORMATION) > capacity)
                {
                    break;
                }

                PHVM_OBJECT_TYPE_INFORMATION typeInformation = (PHVM_OBJECT_TYPE_INFORMATION)entry;
                ProbeForRead(typeInformation, sizeof(HVM_OBJECT_TYPE_INFORMATION), 1);

                PWCHAR inlineName = (PWCHAR)(typeInformation + 1);
                ULONG entryNameLength = typeInformation->TypeName.MaximumLength;
                if (entryNameLength == 0)
                {
                    entryNameLength = typeInformation->TypeName.Length;
                }

                const ULONG entrySize = (ULONG)hvm_debug_visibility::ObjectTypeEntrySize(
                    sizeof(HVM_OBJECT_TYPE_INFORMATION),
                    entryNameLength,
                    sizeof(PVOID));
                if (entrySize == 0 ||
                    entrySize > capacity - (ULONG)(entry - (PUCHAR)ObjectInformation))
                {
                    break;
                }

                PUCHAR next = entry + entrySize;
                if (typeInformation->TypeName.Length == sizeof(kVirtualDebugObjectTypeName) - sizeof(WCHAR) &&
                    (ULONG)((PUCHAR)inlineName - (PUCHAR)ObjectInformation) + typeInformation->TypeName.Length <= capacity)
                {
                    ProbeForRead(inlineName, typeInformation->TypeName.Length, 1);
                    if (RtlCompareMemory(
                            inlineName,
                            kVirtualDebugObjectTypeName,
                            sizeof(kVirtualDebugObjectTypeName) - sizeof(WCHAR)) ==
                        sizeof(kVirtualDebugObjectTypeName) - sizeof(WCHAR))
                    {
                        const ULONG remaining =
                            capacity - (ULONG)(next - (PUCHAR)ObjectInformation);
                        if (remaining != 0)
                        {
                            ProbeForRead(next, remaining, 1);
                            ProbeForWrite(entry, remaining, 1);
                            RtlMoveMemory(entry, next, remaining);
                        }

                        const ULONG newNumberOfTypes = numberOfTypes - 1;
                        ProbeForWrite(ObjectInformation, sizeof(ULONG), 1);
                        *(PULONG)ObjectInformation = newNumberOfTypes;

                        PUCHAR movedEntry = entry;
                        for (ULONG movedIndex = index; movedIndex < newNumberOfTypes; movedIndex++)
                        {
                            PHVM_OBJECT_TYPE_INFORMATION movedType =
                                (PHVM_OBJECT_TYPE_INFORMATION)movedEntry;
                            ProbeForRead(movedType, sizeof(HVM_OBJECT_TYPE_INFORMATION), 1);

                            ULONG movedNameLength = movedType->TypeName.MaximumLength;
                            if (movedNameLength == 0)
                            {
                                movedNameLength = movedType->TypeName.Length;
                            }
                            const ULONG movedSize = (ULONG)hvm_debug_visibility::ObjectTypeEntrySize(
                                sizeof(HVM_OBJECT_TYPE_INFORMATION),
                                movedNameLength,
                                sizeof(PVOID));
                            if (movedSize == 0 ||
                                movedSize > capacity - (ULONG)(movedEntry - (PUCHAR)ObjectInformation))
                            {
                                break;
                            }

                            ProbeForWrite(&movedType->TypeName.Buffer, sizeof(PVOID), sizeof(PVOID));
                            movedType->TypeName.Buffer =
                                (PWCHAR)(movedEntry + sizeof(HVM_OBJECT_TYPE_INFORMATION));
                            movedEntry += movedSize;
                        }

                        if (ARGUMENT_PRESENT(ReturnLength))
                        {
                            if (hvmTempReturnLength >= entrySize)
                            {
                                hvmTempReturnLength -= entrySize;
                            }
                            else
                            {
                                hvmTempReturnLength = 0;
                            }
                        }
                        break;
                    }
                }

                entry = next;
            }

            HVM_RESTORE_RETURNLENGTH();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }

    return status;
}

// NtQuerySystemInformation：对齐 TitanHide 的
// SystemKernelDebuggerInformation / SystemKernelDebuggerInformationEx 隐藏。
NTSTATUS HvmNtQuerySystemInformation(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength OPTIONAL)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtQuerySystemInformation(
            SystemInformationClass,
            SystemInformation,
            SystemInformationLength,
            ReturnLength);
    }

    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    const NTSTATUS status = Original_NtQuerySystemInformation(
        SystemInformationClass,
        SystemInformation,
        SystemInformationLength,
        ReturnLength);

    if (!NT_SUCCESS(status) ||
        SystemInformation == NULL ||
        previousMode == KernelMode)
    {
        return status;
    }

    const bool target = IsCallerVirtualDebugTarget();
    if (hvm_debug_visibility::ShouldHideKernelDebuggerInformation(
        hvm_debug_visibility::ProcessorMode::User,
        static_cast<unsigned long>(SystemInformationClass),
        SystemInformationLength,
        target))
    {
        __try
        {
            HVM_BACKUP_RETURNLENGTH();
            PBOOLEAN information = (PBOOLEAN)SystemInformation;
            information[0] = FALSE; // DebuggerEnabled
            information[1] = TRUE;  // DebuggerNotPresent
            HVM_RESTORE_RETURNLENGTH();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }
    else if (hvm_debug_visibility::ShouldHideKernelDebuggerInformationEx(
        hvm_debug_visibility::ProcessorMode::User,
        static_cast<unsigned long>(SystemInformationClass),
        SystemInformationLength,
        target))
    {
        __try
        {
            HVM_BACKUP_RETURNLENGTH();
            PBOOLEAN information = (PBOOLEAN)SystemInformation;
            information[0] = FALSE; // DebuggerAllowed
            information[1] = FALSE; // DebuggerEnabled
            information[2] = FALSE; // DebuggerPresent
            HVM_RESTORE_RETURNLENGTH();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }
    else if (hvm_debug_visibility::ShouldClearVirtualFirmwareFeature(
        hvm_debug_visibility::ProcessorMode::User,
        static_cast<unsigned long>(SystemInformationClass),
        SystemInformationLength,
        target))
    {
        __try
        {
            HVM_BACKUP_RETURNLENGTH();
            PULONGLONG processorFeatureBits = (PULONGLONG)SystemInformation;
            *processorFeatureBits =
                hvm_debug_visibility::ClearVirtualFirmwareFeatureBit(*processorFeatureBits);
            HVM_RESTORE_RETURNLENGTH();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }
    else if (hvm_debug_visibility::ShouldRewriteHandleObjectTypeIndex(
        hvm_debug_visibility::ProcessorMode::User,
        static_cast<unsigned long>(SystemInformationClass),
        target))
    {
        __try
        {
            if (DbgkDebugObjectType == NULL ||
                *DbgkDebugObjectType == NULL ||
                Hvm_DbgkDebugObjectType == NULL)
            {
                return status;
            }

            const UCHAR virtualObjectTypeIndex = Hvm_DbgkDebugObjectType->Index;
            const UCHAR standardObjectTypeIndex = (*DbgkDebugObjectType)->Index;
            if (virtualObjectTypeIndex == standardObjectTypeIndex)
            {
                return status;
            }

            if (SystemInformationClass == kSystemHandleInformation)
            {
                ProbeForRead(SystemInformation, sizeof(ULONG), 1);
                PHVM_SYSTEM_HANDLE_INFORMATION handleInformation =
                    (PHVM_SYSTEM_HANDLE_INFORMATION)SystemInformation;
                const ULONG_PTR headerSize = FIELD_OFFSET(HVM_SYSTEM_HANDLE_INFORMATION, Handles);
                const ULONG entrySize = sizeof(HVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO);
                const ULONG_PTR count = handleInformation->NumberOfHandles;
                if (count != 0 &&
                    headerSize <= SystemInformationLength &&
                    count <= (SystemInformationLength - headerSize) / entrySize)
                {
                    ProbeForWrite(handleInformation->Handles, count * entrySize, 1);
                    for (ULONG_PTR index = 0; index < count; index++)
                    {
                        if (handleInformation->Handles[index].ObjectTypeIndex ==
                            virtualObjectTypeIndex)
                        {
                            handleInformation->Handles[index].ObjectTypeIndex =
                                standardObjectTypeIndex;
                        }
                    }
                }
            }
            else
            {
                ProbeForRead(SystemInformation, sizeof(ULONG_PTR), 1);
                PHVM_SYSTEM_HANDLE_INFORMATION_EX handleInformation =
                    (PHVM_SYSTEM_HANDLE_INFORMATION_EX)SystemInformation;
                const ULONG_PTR headerSize = FIELD_OFFSET(HVM_SYSTEM_HANDLE_INFORMATION_EX, Handles);
                const ULONG entrySize = sizeof(HVM_SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX);
                const ULONG_PTR count = handleInformation->NumberOfHandles;
                if (count != 0 &&
                    headerSize <= SystemInformationLength &&
                    count <= (SystemInformationLength - headerSize) / entrySize)
                {
                    ProbeForWrite(handleInformation->Handles, count * entrySize, 1);
                    for (ULONG_PTR index = 0; index < count; index++)
                    {
                        if (handleInformation->Handles[index].ObjectTypeIndex ==
                            virtualObjectTypeIndex)
                        {
                            handleInformation->Handles[index].ObjectTypeIndex =
                                standardObjectTypeIndex;
                        }
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }
    else if (hvm_debug_visibility::ShouldHideHypervisorInformation(
        hvm_debug_visibility::ProcessorMode::User,
        static_cast<unsigned long>(SystemInformationClass),
        SystemInformationLength,
        sizeof(HVM_SYSTEM_HYPERVISOR_QUERY_INFORMATION),
        target))
    {
        __try
        {
            HVM_BACKUP_RETURNLENGTH();
            PHVM_SYSTEM_HYPERVISOR_QUERY_INFORMATION information =
                (PHVM_SYSTEM_HYPERVISOR_QUERY_INFORMATION)SystemInformation;
            information->HypervisorConnected = FALSE;
            information->HypervisorDebuggingEnabled = FALSE;
            information->HypervisorPresent = FALSE;
            RtlZeroMemory(information->Spare0, sizeof(information->Spare0));
            information->EnabledEnlightenments = 0;
            HVM_RESTORE_RETURNLENGTH();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }

    return status;
}

// NtSystemDebugControl：对齐 TitanHide，被调试目标除转储类命令外一律
// 返回 STATUS_DEBUGGER_INACTIVE。
NTSTATUS HvmNtSystemDebugControl(
    IN ULONG Command,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    OUT PULONG ReturnLength OPTIONAL)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtSystemDebugControl(
            Command,
            InputBuffer,
            InputBufferLength,
            OutputBuffer,
            OutputBufferLength,
            ReturnLength);
    }

    if (KeGetPreviousMode() != KernelMode &&
        Command != kSysDbgGetTriageDump &&
        Command != kSysDbgGetLiveKernelDump &&
        IsCallerVirtualDebugTarget())
    {
        return STATUS_DEBUGGER_INACTIVE;
    }

    return Original_NtSystemDebugControl(
        Command,
        InputBuffer,
        InputBufferLength,
        OutputBuffer,
        OutputBufferLength,
        ReturnLength);
}

// NtClose：被调试目标关闭内核保护句柄（OBJ_PROTECT_CLOSE）时，
// 对齐 TitanHide 返回状态码而不是让内核抛异常，避免异常路径暴露调试器。
NTSTATUS HvmNtClose(IN HANDLE Handle)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtClose(Handle);
    }

    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    if (previousMode == KernelMode ||
        KeGetCurrentIrql() > APC_LEVEL ||
        !IsCallerVirtualDebugTarget())
    {
        return Original_NtClose(Handle);
    }

    BOOLEAN auditOnClose = FALSE;
    KeWaitForSingleObject(&g_AntiDebugNtCloseMutex, Executive, KernelMode, FALSE, NULL);

    const NTSTATUS auditStatus = ObQueryObjectAuditingByHandle(Handle, &auditOnClose);
    if (auditStatus == STATUS_INVALID_HANDLE)
    {
        KeReleaseMutex(&g_AntiDebugNtCloseMutex, FALSE);
        return STATUS_INVALID_HANDLE;
    }

    NTSTATUS status = STATUS_SUCCESS;

    PVOID object = NULL;
    OBJECT_HANDLE_INFORMATION handleInformation = { 0 };
    const NTSTATUS referenceStatus = ObReferenceObjectByHandle(
        Handle,
        0,
        NULL,
        previousMode,
        &object,
        &handleInformation);
    if (object != NULL)
    {
        ObDereferenceObject(object);
    }

    if (NT_SUCCESS(referenceStatus) &&
        (handleInformation.HandleAttributes & kObjectProtectClose) != 0)
    {
        // TitanHide：返回 STATUS_HANDLE_NOT_CLOSABLE，而不是让 ObCloseHandle
        // 走异常路径；句柄保持打开状态供调用方后续自行处理。
        status = STATUS_HANDLE_NOT_CLOSABLE;
    }
    else
    {
        status = ObCloseHandle(Handle, previousMode);
    }

    KeReleaseMutex(&g_AntiDebugNtCloseMutex, FALSE);
    return status;
}

// NtDuplicateObject：被调试目标使用 DUPLICATE_CLOSE_SOURCE 复制一个
// OBJ_PROTECT_CLOSE 句柄时，清除 CLOSE_SOURCE，避免内核内部关闭源句柄时抛异常。
NTSTATUS HvmNtDuplicateObject(
    IN HANDLE SourceProcessHandle,
    IN HANDLE SourceHandle,
    IN HANDLE TargetProcessHandle,
    OUT PHANDLE TargetHandle OPTIONAL,
    IN ACCESS_MASK DesiredAccess OPTIONAL,
    IN ULONG HandleAttributes,
    IN ULONG Options)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtDuplicateObject(
            SourceProcessHandle,
            SourceHandle,
            TargetProcessHandle,
            TargetHandle,
            DesiredAccess,
            HandleAttributes,
            Options);
    }

    ULONG adjustedOptions = Options;
    const KPROCESSOR_MODE previousMode = KeGetPreviousMode();
    if (previousMode != KernelMode &&
        (Options & DUPLICATE_CLOSE_SOURCE) != 0 &&
        IsCallerVirtualDebugTarget())
    {
        PVOID object = NULL;
        OBJECT_HANDLE_INFORMATION handleInformation = { 0 };
        const NTSTATUS referenceStatus = ObReferenceObjectByHandle(
            SourceHandle,
            0,
            NULL,
            previousMode,
            &object,
            &handleInformation);
        if (NT_SUCCESS(referenceStatus))
        {
            if (object != NULL)
            {
                ObDereferenceObject(object);
            }
            if ((handleInformation.HandleAttributes & kObjectProtectClose) != 0)
            {
                adjustedOptions &= ~DUPLICATE_CLOSE_SOURCE;
            }
        }
    }

    return Original_NtDuplicateObject(
        SourceProcessHandle,
        SourceHandle,
        TargetProcessHandle,
        TargetHandle,
        DesiredAccess,
        HandleAttributes,
        adjustedOptions);
}

// 统一处理 attach/create 两条路径的目标调试痕迹：
//   1) 清除运行中线程的 ThreadHideFromDebugger（DKOM，TitanHide 同款目标）；
//   2) 清除 PEB.BeingDebugged / NtGlobalFlag / 堆调试标志（TitanHide Olly 插件同款目标）。
// 该函数在 PASSIVE_LEVEL 调用，幂等，可重复执行。
VOID PrepareVirtualDebugTargetProcess(_EPROCESS* Process)
{
    if (Process == NULL || KeGetCurrentIrql() > APC_LEVEL)
    {
        return;
    }

    if (InterlockedIncrement(&g_AntiDebugTracePrepare) <= 2)
    {
        outFile("[AntiDebug] PrepareVirtualDebugTargetProcess process=%p pid=%u",
            Process,
            (ULONG)(ULONG_PTR)PsGetProcessId((PEPROCESS)Process));
    }

    if (PsGetNextProcessThread != NULL && ethread_offset::CrossThreadFlags != 0xFFFFFFFF)
    {
        // PsGetNextProcessThread 的所有权契约（nt!PsGetNextProcessThread 反汇编可证）：
        //   - 成功时返回一个新的、已引用的线程对象；
        //   - 对传入的非 NULL Thread 参数执行一次 ObfDereferenceObjectWithTag。
        // 因此循环必须把当前对象直接交给下一次调用，不能在本函数里再手动 Dereference，
        // 否则每个线程引用计数会被多减一次，进程退出扫句柄时触发 0x18 REFERENCE_BY_POINTER。
        _ETHREAD* current = PsGetNextProcessThread((PMY_EPROCESS)Process, NULL);
        while (current != NULL)
        {
            __try
            {
                LONG* crossThreadFlags = (LONG*)((PUCHAR)current + ethread_offset::CrossThreadFlags);
                InterlockedAnd(crossThreadFlags, ~kEthreadCrossThreadFlagsHideFromDebugger);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }

            current = PsGetNextProcessThread((PMY_EPROCESS)Process, current);
        }
    }

    if (eprocess_offset::Peb == 0xFFFFFFFF)
    {
        return;
    }

    PVOID peb64 = NULL;
    __try
    {
        peb64 = *(PVOID*)((PUCHAR)Process + eprocess_offset::Peb);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        peb64 = NULL;
    }
    if (peb64 != NULL)
    {
        HidePebDebugArtifacts(Process, peb64, FALSE);
    }

    if (eprocess_offset::WoW64Process != 0xFFFFFFFF)
    {
        PVOID wow64Process = NULL;
        __try
        {
            wow64Process = *(PVOID*)((PUCHAR)Process + eprocess_offset::WoW64Process);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            wow64Process = NULL;
        }

        if (wow64Process != NULL)
        {
            // _EWOW64PROCESS { void* Peb; ... }，Peb 为 32 位 PEB 地址。
            PVOID peb32 = NULL;
            __try
            {
                peb32 = *(PVOID*)wow64Process;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                peb32 = NULL;
            }
            if (peb32 != NULL && peb32 != peb64)
            {
                HidePebDebugArtifacts(Process, peb32, TRUE);
            }
        }
    }
}

EXTERN_C
VOID Hook_NtQueryInformationProcess()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(
            Sys_NtQueryInformationProcess,
            HvmNtQueryInformationProcess,
            (PVOID*)&Original_NtQueryInformationProcess,
            "NtQueryInformationProcess",
            "NtQueryInformationProcess");
    }
}

EXTERN_C
VOID Hook_NtSetInformationProcess()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(
            Sys_NtSetInformationProcess,
            HvmNtSetInformationProcess,
            (PVOID*)&Original_NtSetInformationProcess,
            "NtSetInformationProcess",
            "NtSetInformationProcess");
    }
}

EXTERN_C
VOID Hook_NtQueryObject()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(
            Sys_NtQueryObject,
            HvmNtQueryObject,
            (PVOID*)&Original_NtQueryObject,
            "NtQueryObject",
            "NtQueryObject");
    }
}

EXTERN_C
VOID Hook_NtQuerySystemInformation()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(
            Sys_NtQuerySystemInformation,
            HvmNtQuerySystemInformation,
            (PVOID*)&Original_NtQuerySystemInformation,
            "NtQuerySystemInformation",
            "NtQuerySystemInformation");
    }
}

EXTERN_C
VOID Hook_NtSystemDebugControl()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(
            Sys_NtSystemDebugControl,
            HvmNtSystemDebugControl,
            (PVOID*)&Original_NtSystemDebugControl,
            "NtSystemDebugControl",
            "NtSystemDebugControl");
    }
}

EXTERN_C
VOID Hook_NtClose()
{
    if (!g_IsInitGlobalVariable)
    {
        return;
    }

    if (InterlockedExchange(&g_AntiDebugNtCloseMutexReady, 1) == 0)
    {
        KeInitializeMutex(&g_AntiDebugNtCloseMutex, 0);
    }

    HookEptRoutine(
        Sys_NtClose,
        HvmNtClose,
        (PVOID*)&Original_NtClose,
        "NtClose",
        "NtClose");
}

EXTERN_C
VOID Hook_NtDuplicateObject()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(
            Sys_NtDuplicateObject,
            HvmNtDuplicateObject,
            (PVOID*)&Original_NtDuplicateObject,
            "NtDuplicateObject",
            "NtDuplicateObject");
    }
}

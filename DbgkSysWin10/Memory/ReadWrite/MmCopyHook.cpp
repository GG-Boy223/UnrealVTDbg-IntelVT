#include "ReadWriteInternal.h"

NTSTATUS NewMmCopyVirtualMemory(
    _EPROCESS* FromProcess,
    PVOID FromAddress,
    _EPROCESS* ToProcess,
    PVOID ToAddress,
    size_t BufferSize,
    KPROCESSOR_MODE PreviousMode,
    size_t* NumberOfBytesCopied)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_MmCopyVirtualMemory(FromProcess, FromAddress, ToProcess, ToAddress, BufferSize, PreviousMode, NumberOfBytesCopied);
    }

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
        PreviousMode = KernelMode;
    }
    return Original_MmCopyVirtualMemory(FromProcess, FromAddress, ToProcess, ToAddress, BufferSize, PreviousMode, NumberOfBytesCopied);
}

#include "ReadWriteInternal.h"

NTSTATUS NtWriteVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _In_ PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesWritten)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtWriteVirtualMemory(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesWritten);
    }

    NTSTATUS Status = STATUS_SUCCESS;
    _EPROCESS* Process;

    KPROCESSOR_MODE PreviousMode = ExGetPreviousMode();

    if (PreviousMode != KernelMode)
    {
        __try
        {
            ProbeForRead(Buffer, BufferSize, 1);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }

    if (IsDebugger(PsGetCurrentProcess()))
    {
        if (BufferSize == 1)
        {
            if (Buffer && (*(UCHAR*)Buffer == 0xCC))
            {
                Status = ObReferenceObjectByHandle(ProcessHandle,
                    PROCESS_ALL_ACCESS,
                    *PsProcessType,
                    KernelMode,
                    (PVOID*)&Process,
                    NULL);
                if (NT_SUCCESS(Status))
                {
                    if (!exist_breakpoint(Process, BaseAddress))
                    {
                        Status = SetBreakpoint(ProcessHandle, Process, BaseAddress, Buffer, BufferSize);
                    }
                    else
                    {
                        Status = STATUS_SUCCESS;
                    }
                    if (Process)
                    {
                        ObDereferenceObject(Process);
                    }
                    return Status;
                }
                return STATUS_UNSUCCESSFUL;
            }

            if (Buffer)
            {
                Status = ObReferenceObjectByHandle(ProcessHandle,
                    PROCESS_ALL_ACCESS,
                    *PsProcessType,
                    KernelMode,
                    (PVOID*)&Process,
                    NULL);
                if (NT_SUCCESS(Status))
                {
                    if (exist_breakpoint(Process, BaseAddress))
                    {
                        Status = RemoveBreakpoint(ProcessHandle, Process, BaseAddress, Buffer, BufferSize);
                        if (Process)
                        {
                            ObDereferenceObject(Process);
                        }
                        return Status;
                    }
                    if (Process)
                    {
                        ObDereferenceObject(Process);
                    }
                }
                else
                {
                    return STATUS_UNSUCCESSFUL;
                }
            }
        }
    }
    ASSERT(Original_NtWriteVirtualMemory);
    return Original_NtWriteVirtualMemory(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesWritten);
}

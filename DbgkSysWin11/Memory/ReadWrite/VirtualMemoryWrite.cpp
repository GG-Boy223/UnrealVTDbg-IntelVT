#include "ReadWriteInternal.h"

NTSTATUS NtWriteVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _In_ PVOID Buffer, //当前进程自己的缓冲区
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesWritten)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtWriteVirtualMemory(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesWritten);
    }

    NTSTATUS Status = STATUS_SUCCESS;
    _EPROCESS* Process; //进程对象

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

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
        if (BufferSize == 1)
        {
            //设置int3断点
            if (Buffer && (*(UCHAR*)Buffer == 0xCC))
            {
                // /* Reference the process */
                Status = ObReferenceObjectByHandle(ProcessHandle,
                    PROCESS_ALL_ACCESS,
                    *PsProcessType,
                    // *PsProcessType,
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
                        // /* Then default to STATUS_SUCCESS */
                        Status = STATUS_SUCCESS;
                    }
                    if (Process)
                    {
                        ObDereferenceObject(Process); //释放进程对象
                    }
                    // /* Return status */
                    return Status;
                }
                return STATUS_UNSUCCESSFUL;
            }

            //移除int3断点
            if (Buffer)
            {
                // /* Reference the process */
                Status = ObReferenceObjectByHandle(ProcessHandle,
                    PROCESS_ALL_ACCESS,
                    *PsProcessType,
                    // *PsProcessType,
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
                            ObDereferenceObject(Process); //释放进程对象
                        }
                        // /* Return status */
                        return Status;
                    }
                    if (Process)
                    {
                        ObDereferenceObject(Process); //释放进程对象
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

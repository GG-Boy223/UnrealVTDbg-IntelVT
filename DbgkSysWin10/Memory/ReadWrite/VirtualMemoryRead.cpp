#include "ReadWriteInternal.h"

NTSTATUS MyReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead)
{
    NTSTATUS Status;
    _EPROCESS* Process = NULL;

    KAPC_STATE ApcState;
    BOOLEAN attached = FALSE;

    BYTE* tmpBuffer = NULL;
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    if (Buffer == NULL || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObReferenceObjectByHandle(ProcessHandle, PROCESS_ALL_ACCESS, *PsProcessType, KernelMode, (PVOID*)&Process, NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    tmpBuffer = allocate_pool<BYTE*>(BufferSize);
    if (tmpBuffer == NULL)
    {
        ObDereferenceObject(Process);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    attached = TRUE;

    Status = STATUS_SUCCESS;
    __try
    {
        MappedAddress = MDLMapVirtualAddress(BaseAddress, BufferSize, map_table);
        if (MappedAddress)
        {
            RtlCopyMemory(tmpBuffer, MappedAddress, BufferSize);
            UnMDLMapVirtualAddress(map_table);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Status = GetExceptionCode();
    }

    if (attached)
    {
        KiUnstackDetachProcess(&ApcState, 0);
    }

    if (NT_SUCCESS(Status))
    {
        RtlCopyMemory(Buffer, tmpBuffer, BufferSize);
        if (NumberOfBytesRead)
        {
            *NumberOfBytesRead = BufferSize;
        }
    }

    free_pool(tmpBuffer);
    ObDereferenceObject(Process);
    return Status;
}

NTSTATUS EptReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead)
{
    NTSTATUS Status;
    _EPROCESS* Process = NULL;

    BYTE* tmpBuffer = NULL;
    KAPC_STATE ApcState;
    BOOLEAN attached = FALSE;
    BOOLEAN boFound = FALSE;

    if (Buffer == NULL || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObReferenceObjectByHandle(ProcessHandle, PROCESS_ALL_ACCESS, *PsProcessType, KernelMode, (PVOID*)&Process, NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    tmpBuffer = allocate_contignous_memory<BYTE*>(BufferSize);
    if (tmpBuffer == NULL)
    {
        ObDereferenceObject(Process);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    attached = TRUE;

    Status = STATUS_SUCCESS;
    __try
    {
        if (hvgt::read_ept_fake_page_memory(BaseAddress, tmpBuffer, BufferSize))
        {
            boFound = TRUE;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Status = GetExceptionCode();
    }

    if (attached)
    {
        KiUnstackDetachProcess(&ApcState, 0);
    }

    if (NT_SUCCESS(Status) && boFound)
    {
        RtlCopyMemory(Buffer, tmpBuffer, BufferSize);
        if (NumberOfBytesRead)
        {
            *NumberOfBytesRead = BufferSize;
        }

        Status = STATUS_SUCCESS;
    }
    else if (NT_SUCCESS(Status))
    {
        Status = STATUS_UNSUCCESSFUL;
    }

    free_contignous_memory(tmpBuffer);
    ObDereferenceObject(Process);
    return Status;
}

NTSTATUS NtReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtReadVirtualMemory(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead);
    }

    NTSTATUS Status = STATUS_SUCCESS;
    _EPROCESS* Process;
    BOOLEAN boFound = FALSE;

    KPROCESSOR_MODE PreviousMode = ExGetPreviousMode();

    if (PreviousMode != KernelMode)
    {
        __try
        {
            ProbeForWrite(Buffer, BufferSize, 1);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }

    {
        if (IsDebugger(PsGetCurrentProcess()))
        {
            if (Buffer)
            {
            }

        }
        ASSERT(Original_NtReadVirtualMemory);
        Status = Original_NtReadVirtualMemory(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead);
    }

    return Status;
}

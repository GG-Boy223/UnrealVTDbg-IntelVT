#include "ReadWriteInternal.h"

NTSTATUS MyReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer, //输出缓冲区
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead)
{
    NTSTATUS Status; // eax
    _EPROCESS* Process = NULL;

    KAPC_STATE ApcState;
    BOOLEAN attached = FALSE; // 记录是否完成 Attach，确保异常路径也能 Detach。

    BYTE* tmpBuffer = NULL;
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    if (Buffer == NULL || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。
    }

    Status = ObReferenceObjectByHandle(ProcessHandle, PROCESS_ALL_ACCESS, *PsProcessType, KernelMode, (PVOID*)&Process, NULL);
    // /* Check if we failed */
    if (!NT_SUCCESS(Status))
    {
        // /* Return status */
        return Status;
    }

    tmpBuffer = allocate_pool<BYTE*>(BufferSize);
    if (tmpBuffer == NULL)
    {
        ObDereferenceObject(Process); //释放进程对象
        return STATUS_INSUFFICIENT_RESOURCES; // 显式返回内存不足，避免隐式成功。
    }

    //DbgBreakPoint();
    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    attached = TRUE;

    Status = STATUS_SUCCESS;
    __try
    {
        MappedAddress = MDLMapVirtualAddress(BaseAddress, BufferSize, map_table);
        if (MappedAddress)
        {
            //先拷贝到系统的临时缓冲区,降低直接回写风险。
            RtlCopyMemory(tmpBuffer, MappedAddress, BufferSize);
            UnMDLMapVirtualAddress(map_table);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Status = GetExceptionCode(); // 记录异常并走统一清理路径。
    }

    if (attached)
    {
        KiUnstackDetachProcess(&ApcState, 0); // 无论成功失败都对称解除附着。
    }

    // /* Now check what status we have here */
    if (NT_SUCCESS(Status))
    {
        RtlCopyMemory(Buffer, tmpBuffer, BufferSize);
        if (NumberOfBytesRead)
        {
            *NumberOfBytesRead = BufferSize;
        }
    }

    free_pool(tmpBuffer);
    ObDereferenceObject(Process); //释放进程对象
    /* Return status */
    // /* Return status */
    return Status;
}

NTSTATUS EptReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer, //输出缓冲区
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead)
{
    NTSTATUS Status; // eax
    _EPROCESS* Process = NULL;

    BYTE* tmpBuffer = NULL;
    KAPC_STATE ApcState;
    BOOLEAN attached = FALSE; // 记录是否完成 Attach，确保异常路径也能 Detach。
    BOOLEAN boFound = FALSE;

    if (Buffer == NULL || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。
    }

    Status = ObReferenceObjectByHandle(ProcessHandle, PROCESS_ALL_ACCESS, *PsProcessType, KernelMode, (PVOID*)&Process, NULL);
    // /* Check if we failed */
    if (!NT_SUCCESS(Status))
    {
        // /* Return status */
        return Status;
    }

    tmpBuffer = allocate_contignous_memory<BYTE*>(BufferSize);
    if (tmpBuffer == NULL)
    {
        ObDereferenceObject(Process); //释放进程对象
        return STATUS_INSUFFICIENT_RESOURCES; // 显式返回内存不足，避免隐式成功。
    }

    //DbgBreakPoint();
    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    attached = TRUE;

    Status = STATUS_SUCCESS;
    __try
    {
        //读取EPT内存
        if (hvgt::read_ept_fake_page_memory(BaseAddress, tmpBuffer, BufferSize))
        {
            boFound = TRUE; // 仅标记命中，回写在统一出口执行。
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Status = GetExceptionCode(); // 记录异常并走统一清理路径。
    }

    if (attached)
    {
        KiUnstackDetachProcess(&ApcState, 0); // 无论成功失败都对称解除附着。
    }

    if (NT_SUCCESS(Status) && boFound)
    {
        RtlCopyMemory(Buffer, tmpBuffer, BufferSize);
        if (NumberOfBytesRead)
        {
            *NumberOfBytesRead = BufferSize;
        }

        // /* Set success */
        Status = STATUS_SUCCESS;
    }
    else if (NT_SUCCESS(Status))
    {
        Status = STATUS_UNSUCCESSFUL; // 无异常但未读到数据，返回业务失败。
    }

    free_contignous_memory(tmpBuffer);
    ObDereferenceObject(Process); //释放进程对象
    /* Return status */
    // /* Return status */
    return Status;
}

NTSTATUS NtReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer, //输出缓冲区
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtReadVirtualMemory(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead);
    }

    NTSTATUS Status = STATUS_SUCCESS;
    _EPROCESS* Process; //进程对象
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
        //DbgBreakPoint();
        if (IsDebugger(PsGetCurrentProcess()))
        {
            //移除int3断点
            if (Buffer)
            {
            }

        }
        ASSERT(Original_NtReadVirtualMemory);
        Status = Original_NtReadVirtualMemory(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead);
    }

    // /* Return status */
    return Status;
}

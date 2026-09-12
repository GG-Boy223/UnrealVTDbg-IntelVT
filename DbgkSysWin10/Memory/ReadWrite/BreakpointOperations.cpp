#include "ReadWriteInternal.h"

NTSTATUS SetBreakpoint(HANDLE ProcessHandle, _EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize)
{
    UNREFERENCED_PARAMETER(ProcessHandle);

    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    KAPC_STATE ApcState;
    BYTE tmpBuffer[256] = { 0 };
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    if (Process == NULL || BaseAddress == NULL || Buffer == NULL || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (BufferSize > sizeof(tmpBuffer))
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    //DbgBreakPoint();
    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    RtlCopyMemory(tmpBuffer, Buffer, BufferSize);
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    //通过vt交换4KB物理页
    //设置或恢复断点
    __try
    {
        //DbgBreakPoint();
        SIZE_T RegionSize = BufferSize;
        ULONG flOldProtect;
        PVOID loc_BaseAddress = BaseAddress;
        //将页属性改为写拷贝  从而在写入值时分配新的物理页
        //KPROCESSOR_MODE Old_PreviousMode = KeSetPreviousMode(KernelMode);
        Status = ZwProtectVirtualMemory(NtCurrentProcess(), &loc_BaseAddress, &RegionSize, PAGE_EXECUTE_WRITECOPY, &flOldProtect);
        if (NT_SUCCESS(Status))
        {
            //触发写拷贝分配新的物理页
            RtlCopyMemory(BaseAddress, BaseAddress, BufferSize);

            //通过mdl将新分配的物理页面映射到虚拟内存
            MappedAddress = MDLMapVirtualAddress(BaseAddress, BufferSize, map_table);
            if (MappedAddress == NULL)
            {
                Status = STATUS_UNSUCCESSFUL;
            }
            else
            {
                map_table.VirtualAddress = BaseAddress;
                if (hvgt::set_hide_software_breakpoint(&map_table, &tmpBuffer, BufferSize))
                {
                    Status = STATUS_SUCCESS;
                    TraceBreakpoint(Process, BaseAddress, tmpBuffer);
                }
                else
                {
                    Status = STATUS_UNSUCCESSFUL;
                }
                UnMDLMapVirtualAddress(map_table);
            }

            if (NT_SUCCESS(Status))
            {
                Status = ZwProtectVirtualMemory(NtCurrentProcess(), &loc_BaseAddress, &RegionSize, flOldProtect, &flOldProtect);
            }
            else
            {
                (void)ZwProtectVirtualMemory(NtCurrentProcess(), &loc_BaseAddress, &RegionSize, flOldProtect, &flOldProtect);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        KiUnstackDetachProcess(&ApcState, 0);
        return GetExceptionCode();
    }
    KiUnstackDetachProcess(&ApcState, 0);
    return Status;
}

NTSTATUS RemoveBreakpoint(HANDLE ProcessHandle, _EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize)
{
    UNREFERENCED_PARAMETER(ProcessHandle);

    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    KAPC_STATE ApcState;
    BYTE tmpBuffer[256] = { 0 };
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    if (Process == NULL || BaseAddress == NULL || Buffer == NULL || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (BufferSize > sizeof(tmpBuffer))
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    //DbgBreakPoint();
    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    RtlCopyMemory(tmpBuffer, Buffer, BufferSize);
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    //通过vt交换4KB物理页
    //设置或恢复断点
    __try
    {
        //DbgBreakPoint();
        SIZE_T RegionSize = BufferSize;
        ULONG flOldProtect;
        PVOID loc_BaseAddress = BaseAddress;
        {
            MappedAddress = MDLMapVirtualAddress(BaseAddress, BufferSize, map_table);
            if (MappedAddress == NULL)
            {
                Status = STATUS_UNSUCCESSFUL;
            }
            else
            {
                map_table.VirtualAddress = BaseAddress;
                if (hvgt::set_hide_software_breakpoint(&map_table, &tmpBuffer, BufferSize))
                {
                    Status = STATUS_SUCCESS;
                    DeleteBreakpoint(Process, BaseAddress, tmpBuffer);
                }
                else
                {
                    Status = STATUS_UNSUCCESSFUL;
                }
                UnMDLMapVirtualAddress(map_table);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        KiUnstackDetachProcess(&ApcState, 0);
        return GetExceptionCode();
    }
    KiUnstackDetachProcess(&ApcState, 0);
    return Status;
}

NTSTATUS GetBreakpoint(_EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize)
{
    KAPC_STATE ApcState;
    BYTE tmpBuffer[256] = { 0 };
    BOOLEAN boFound = FALSE;
    NTSTATUS Status = STATUS_SUCCESS;
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    if (Process == NULL || BaseAddress == NULL || Buffer == NULL || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (BufferSize > sizeof(tmpBuffer))
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    //DbgBreakPoint();
    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    //通过vt交换4KB物理页
    //设置或恢复断点
    __try
    {
        MappedAddress = MDLMapVirtualAddress(BaseAddress, BufferSize, map_table);
        if (MappedAddress == NULL)
        {
            Status = STATUS_UNSUCCESSFUL;
        }
        else
        {
            //读取EPT内存
            if (hvgt::get_hide_software_breakpoint(BaseAddress, &tmpBuffer, BufferSize))
            {
                boFound = TRUE; // 仅标记命中，回写在统一出口执行。
            }
            else
            {
                Status = STATUS_UNSUCCESSFUL;
            }
            UnMDLMapVirtualAddress(map_table);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        KiUnstackDetachProcess(&ApcState, 0);
        return GetExceptionCode();
    }
    KiUnstackDetachProcess(&ApcState, 0);

    if (boFound)
    {
        RtlCopyMemory(Buffer, tmpBuffer, BufferSize);
        return STATUS_SUCCESS;
    }
    return Status;
}

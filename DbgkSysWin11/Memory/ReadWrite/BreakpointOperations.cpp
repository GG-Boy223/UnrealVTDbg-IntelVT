#include "ReadWriteInternal.h"

NTSTATUS SetBreakpoint(HANDLE ProcessHandle, _EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize)
{
    UNREFERENCED_PARAMETER(ProcessHandle); // 当前函数通过 Process 对象工作，不直接使用句柄。

    NTSTATUS Status = STATUS_SUCCESS;
    KAPC_STATE ApcState;
    BOOLEAN attached = FALSE; // 记录是否完成 Attach，确保异常路径也能 Detach。

    BYTE tmpBuffer[256] = { 0 }; // 本地缓存断点字节，避免直接使用外部缓冲区。
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    if (Process == NULL || BaseAddress == NULL || Buffer == NULL || BufferSize == 0)
    {
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。
    }

    if (BufferSize > sizeof(tmpBuffer))
    {
        return STATUS_INVALID_BUFFER_SIZE; // 防止固定栈缓冲区溢出。
    }

    //DbgBreakPoint();
    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    RtlCopyMemory(tmpBuffer, Buffer, BufferSize); // 拷贝用户提供的断点字节，后续仅操作本地副本。

    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    attached = TRUE;
    //通过vt交换4KB物理页
    //设置或恢复断点
    __try
    {
        //DbgBreakPoint();
        SIZE_T RegionSize = BufferSize;
        ULONG flOldProtect = 0;
        PVOID loc_BaseAddress = BaseAddress;
        //将页属性改为写拷贝  从而在写入值时分配新的物理页
        //KPROCESSOR_MODE Old_PreviousMode = KeSetPreviousMode(KernelMode);
        Status = ZwProtectVirtualMemory(NtCurrentProcess(), &loc_BaseAddress, &RegionSize, PAGE_EXECUTE_WRITECOPY, &flOldProtect); // 临时放宽保护，允许触发目标页写路径。
        if (NT_SUCCESS(Status))
        {
            RtlCopyMemory(BaseAddress, BaseAddress, BufferSize); // 保留原行为：触发写时复制路径。

            //通过mdl将新分配的物理页面映射到虚拟内存
            MappedAddress = MDLMapVirtualAddress(BaseAddress, BufferSize, map_table);
            if (MappedAddress)
            {
                map_table.VirtualAddress = BaseAddress;
                if (hvgt::set_hide_software_breakpoint(&map_table, &tmpBuffer, BufferSize))
                {
                    TraceBreakpoint(Process, BaseAddress, tmpBuffer); // 仅 hv 成功时记录断点表。
                }

                UnMDLMapVirtualAddress(map_table);
            }

            ZwProtectVirtualMemory(NtCurrentProcess(), &loc_BaseAddress, &RegionSize, flOldProtect, &flOldProtect); // 恢复原保护属性。
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

    // /* Return status */
    return Status;
}

NTSTATUS RemoveBreakpoint(HANDLE ProcessHandle, _EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize)
{
    NTSTATUS Status = STATUS_SUCCESS;
    KAPC_STATE ApcState;
    BYTE tmpBuffer[256] = { 0 }; // 本地缓存断点字节，避免直接使用外部缓冲区。
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    //DbgBreakPoint();
    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    RtlCopyMemory(tmpBuffer, Buffer, BufferSize); // 拷贝用户提供的断点字节，后续仅操作本地副本。
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
            if (MappedAddress)
            {
                map_table.VirtualAddress = BaseAddress;
                if (hvgt::set_hide_software_breakpoint(&map_table, &tmpBuffer, BufferSize))
                {
                    DeleteBreakpoint(Process, BaseAddress, tmpBuffer);
                }
                UnMDLMapVirtualAddress(map_table);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        KiUnstackDetachProcess(&ApcState, 0); // 无论成功失败都对称解除附着。
        return GetExceptionCode();
    }
    KiUnstackDetachProcess(&ApcState, 0); // 无论成功失败都对称解除附着。
    return STATUS_SUCCESS;
}

NTSTATUS GetBreakpoint(_EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize)
{
    KAPC_STATE ApcState;
    BYTE tmpBuffer[256] = { 0 }; // 本地缓存断点字节，避免直接使用外部缓冲区。
    BOOLEAN boFound = FALSE;
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    //DbgBreakPoint();
    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    //通过vt交换4KB物理页
    //设置或恢复断点
    __try
    {
        MappedAddress = MDLMapVirtualAddress(BaseAddress, BufferSize, map_table);
        if (MappedAddress)
        {
            //读取EPT内存
            if (hvgt::get_hide_software_breakpoint(BaseAddress, &tmpBuffer, BufferSize))
            {
                boFound = TRUE; // 仅标记命中，回写在统一出口执行。
            }
            UnMDLMapVirtualAddress(map_table);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        KiUnstackDetachProcess(&ApcState, 0); // 无论成功失败都对称解除附着。
        return GetExceptionCode();
    }
    KiUnstackDetachProcess(&ApcState, 0); // 无论成功失败都对称解除附着。

    if (boFound)
    {
        RtlCopyMemory(Buffer, tmpBuffer, BufferSize);
        return STATUS_SUCCESS;
    }
    return STATUS_UNSUCCESSFUL;
}

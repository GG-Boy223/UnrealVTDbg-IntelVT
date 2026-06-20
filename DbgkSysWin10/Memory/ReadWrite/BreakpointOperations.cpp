#include "ReadWriteInternal.h"

NTSTATUS SetBreakpoint(HANDLE ProcessHandle, _EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize)
{
    NTSTATUS Status = STATUS_SUCCESS;
    KAPC_STATE ApcState;
    BYTE tmpBuffer[256] = { 0 };
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    RtlCopyMemory(tmpBuffer, Buffer, BufferSize);
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    __try
    {
        SIZE_T RegionSize = BufferSize;
        ULONG flOldProtect;
        PVOID loc_BaseAddress = BaseAddress;
        Status = ZwProtectVirtualMemory(NtCurrentProcess(), &loc_BaseAddress, &RegionSize, PAGE_EXECUTE_WRITECOPY, &flOldProtect);
        if (NT_SUCCESS(Status))
        {
            RtlCopyMemory(BaseAddress, BaseAddress, BufferSize);

            MappedAddress = MDLMapVirtualAddress(BaseAddress, BufferSize, map_table);
            if (MappedAddress)
            {
                map_table.VirtualAddress = BaseAddress;
                if (hvgt::set_hide_software_breakpoint(&map_table, &tmpBuffer, BufferSize))
                {
                    TraceBreakpoint(Process, BaseAddress, tmpBuffer);
                }
                UnMDLMapVirtualAddress(map_table);
            }
            ZwProtectVirtualMemory(NtCurrentProcess(), &loc_BaseAddress, &RegionSize, flOldProtect, &flOldProtect);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        KiUnstackDetachProcess(&ApcState, 0);
        return GetExceptionCode();
    }
    KiUnstackDetachProcess(&ApcState, 0);
    return STATUS_SUCCESS;
}

NTSTATUS RemoveBreakpoint(HANDLE ProcessHandle, _EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize)
{
    NTSTATUS Status = STATUS_SUCCESS;
    KAPC_STATE ApcState;
    BYTE tmpBuffer[256] = { 0 };
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    RtlCopyMemory(tmpBuffer, Buffer, BufferSize);
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    __try
    {
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
        KiUnstackDetachProcess(&ApcState, 0);
        return GetExceptionCode();
    }
    KiUnstackDetachProcess(&ApcState, 0);
    return STATUS_SUCCESS;
}

NTSTATUS GetBreakpoint(_EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize)
{
    KAPC_STATE ApcState;
    BYTE tmpBuffer[256] = { 0 };
    BOOLEAN boFound = FALSE;
    MDL_MAP map_table = { 0 };
    PVOID MappedAddress = NULL;

    size_t ptr_Pcb = (size_t)Process + eprocess_offset::Pcb;
    KiStackAttachProcess((_KPROCESS*)ptr_Pcb, FALSE, &ApcState);
    __try
    {
        MappedAddress = MDLMapVirtualAddress(BaseAddress, BufferSize, map_table);
        if (MappedAddress)
        {
            if (hvgt::get_hide_software_breakpoint(BaseAddress, &tmpBuffer, BufferSize))
            {
                boFound = TRUE;
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
    return STATUS_UNSUCCESSFUL;
}

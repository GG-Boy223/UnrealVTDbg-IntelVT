#pragma once

#ifndef _READ_WRITE_INTERNAL_H
#define _READ_WRITE_INTERNAL_H

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
#include "../../List/MyList.h"
#include "../../ntos/inc/ntlpcapi.h"
#include "../../ntos/inc/psp.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"
#include "../../Hvm/hypervisor_gateway.h"
#include "../../ntos/inc/ke.h"
#include "../../Protect/Process/ProtectProcess.h"
#include "../ReadWrite.h"

VOID InsertBreakpointList(PBREAKPOINT_TABLE_ENTRY entry);
VOID ReleaseBreakpointList();
VOID TraceBreakpoint(_EPROCESS* Process, PVOID BaseAddress, PVOID Buffer);
PVOID MDLMapVirtualAddress(IN PVOID VirtualAddress, IN SIZE_T BufferSize, OUT MDL_MAP& map_table);
VOID UnMDLMapVirtualAddress(MDL_MAP map_table);
NTSTATUS SetBreakpoint(HANDLE ProcessHandle, _EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize);
VOID DeleteBreakpoint(_EPROCESS* Process, PVOID BaseAddress, PVOID Buffer);
NTSTATUS RemoveBreakpoint(HANDLE ProcessHandle, _EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize);
BOOLEAN exist_breakpoint(_EPROCESS* Process, PVOID BaseAddress);
NTSTATUS GetBreakpoint(_EPROCESS* Process, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize);
NTSTATUS MyReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer, //输出缓冲区
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead);
NTSTATUS EptReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer, //输出缓冲区
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead);

#endif

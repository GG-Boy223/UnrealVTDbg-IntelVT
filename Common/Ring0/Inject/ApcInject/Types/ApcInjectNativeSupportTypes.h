#pragma once

#include "ApcInjectSystemTypes.h"

typedef struct _NT_PROC_THREAD_ATTRIBUTE_ENTRY
{
    ULONG Attribute; // PROC_THREAD_ATTRIBUTE_XXX
    SIZE_T Size;
    ULONG_PTR Value;
    ULONG Unknown;
} NT_PROC_THREAD_ATTRIBUTE_ENTRY, *NT_PPROC_THREAD_ATTRIBUTE_ENTRY;

typedef struct _NT_PROC_THREAD_ATTRIBUTE_LIST
{
    ULONG Length;
    NT_PROC_THREAD_ATTRIBUTE_ENTRY Entry[1];
} NT_PROC_THREAD_ATTRIBUTE_LIST, *PNT_PROC_THREAD_ATTRIBUTE_LIST;

typedef struct _SYSTEM_SERVICE_DESCRIPTOR_TABLE
{
    PULONG_PTR ServiceTableBase;
    PULONG ServiceCounterTableBase;
    ULONG_PTR NumberOfServices;
    PUCHAR ParamTableBase;
} SYSTEM_SERVICE_DESCRIPTOR_TABLE, *PSYSTEM_SERVICE_DESCRIPTOR_TABLE;

typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
    HANDLE Section; // Not filled in
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR FullPathName[MAXIMUM_FILENAME_LENGTH];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES
{
    ULONG NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

typedef enum _WinVer
{
    WINVER_7 = 0x0610,
    WINVER_7_SP1 = 0x0611,
    WINVER_8 = 0x0620,
    WINVER_81 = 0x0630,
    WINVER_10 = 0x0A00,
    WINVER_10_RS1 = 0x0A01, // Anniversary update
    WINVER_10_RS2 = 0x0A02, // Creators update
    WINVER_10_RS3 = 0x0A03, // Fall creators update
    WINVER_10_RS4 = 0x0A04, // Spring creators update
    WINVER_10_RS5 = 0x0A05, // October 2018 update
    WINVER_10_19H1 = 0x0A06, // May 2019 update 19H1
    WINVER_10_19H2 = 0x0A07, // November 2019 update 19H2
    WINVER_10_20H1 = 0x0A08, // April 2020 update 20H1
} WinVer;

/// <summary>
/// OS-dependent stuff
/// </summary>
typedef struct _DYNAMIC_DATA
{
    WinVer ver; // OS version
    ULONG buildNo; // OS build revision
    BOOLEAN correctBuild; // OS kernel build number is correct and supported

    ULONG KExecOpt; // KPROCESS::ExecuteOptions
    ULONG Protection; // EPROCESS::Protection
    ULONG EProcessFlags2; // EPROCESS::Flags2
    ULONG ObjTable; // EPROCESS::ObjectTable
    ULONG VadRoot; // EPROCESS::VadRoot
    ULONG NtProtectIndex; // NtProtectVirtualMemory SSDT index
    ULONG NtCreateThdExIndex; // NtCreateThreadEx SSDT index
    ULONG NtTermThdIndex; // NtTerminateThread SSDT index
    ULONG PrevMode; // KTHREAD::PreviousMode
    ULONG ExitStatus; // ETHREAD::ExitStatus
    ULONG MiAllocPage; // MiAllocateDriverPage offset
    ULONG ExRemoveTable; // Ex(p)RemoveHandleTable offset

    ULONG_PTR DYN_PDE_BASE; // Win10 AU+ relocated PDE base VA
    ULONG_PTR DYN_PTE_BASE; // Win10 AU+ relocated PTE base VA
} DYNAMIC_DATA, *PDYNAMIC_DATA;

typedef struct _THREAD_BASIC_INFORMATION
{
    NTSTATUS ExitStatus;
    PVOID TebBaseAddress;
    CLIENT_ID ClientId;
    ULONG_PTR AffinityMask;
    LONG Priority;
    LONG BasePriority;
} THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;

typedef NTSTATUS(NTAPI* fnNtCreateThreadEx)
(
    OUT PHANDLE hThread,
    IN ACCESS_MASK DesiredAccess,
    IN PVOID ObjectAttributes,
    IN HANDLE ProcessHandle,
    IN PVOID lpStartAddress,
    IN PVOID lpParameter,
    IN ULONG Flags,
    IN SIZE_T StackZeroBits,
    IN SIZE_T SizeOfStackCommit,
    IN SIZE_T SizeOfStackReserve,
    OUT PVOID lpBytesBuffer
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
);

typedef VOID(NTAPI* PKNORMAL_ROUTINE)(
    PVOID NormalContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
);

typedef VOID(NTAPI* PKKERNEL_ROUTINE)(
    PRKAPC Apc,
    PKNORMAL_ROUTINE* NormalRoutine,
    PVOID* NormalContext,
    PVOID* SystemArgument1,
    PVOID* SystemArgument2
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
);

typedef VOID(NTAPI* PKRUNDOWN_ROUTINE)(PRKAPC Apc);

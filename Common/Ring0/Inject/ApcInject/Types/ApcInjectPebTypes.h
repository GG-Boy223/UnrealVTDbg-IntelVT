#pragma once

#include "ApcInjectSystemTypes.h"

typedef enum _MI_VAD_TYPE
{
    VadNone,
    VadDevicePhysicalMemory,
    VadImageMap,
    VadAwe,
    VadWriteWatch,
    VadLargePages,
    VadRotatePhysical,
    VadLargePageSection
} MI_VAD_TYPE, *PMI_VAD_TYPE;

typedef enum _MMSYSTEM_PTE_POOL_TYPE
{
    SystemPteSpace,
    NonPagedPoolExpansion,
    MaximumPtePoolTypes
} MMSYSTEM_PTE_POOL_TYPE;

typedef struct _INJECT_BUFFER
{
    UCHAR code[0x200];
    // Activation context data
    union
    {
        UNICODE_STRING path;
        UNICODE_STRING32 path32;
    };

    wchar_t buffer[488];
    PVOID module;
    ULONG complete;
    NTSTATUS status; // Last execution status
} INJECT_BUFFER, *PINJECT_BUFFER;

typedef struct _PEB32
{
    UCHAR InheritedAddressSpace;
    UCHAR ReadImageFileExecOptions;
    UCHAR BeingDebugged;
    UCHAR BitField;
    ULONG Mutant;
    ULONG ImageBaseAddress;
    ULONG Ldr;
    ULONG ProcessParameters;
    ULONG SubSystemData;
    ULONG ProcessHeap;
    ULONG FastPebLock;
    ULONG AtlThunkSListPtr;
    ULONG IFEOKey;
    ULONG CrossProcessFlags;
    ULONG UserSharedInfoPtr;
    ULONG SystemReserved;
    ULONG AtlThunkSListPtr32;
    ULONG ApiSetMap;
} PEB32, *PPEB32;

typedef struct _PEB_LDR_DATA
{
    ULONG Length;
    UCHAR Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _PEB
{
    UCHAR InheritedAddressSpace;
    UCHAR ReadImageFileExecOptions;
    UCHAR BeingDebugged;
    UCHAR BitField;
    PVOID Mutant;
    PVOID ImageBaseAddress;
    PPEB_LDR_DATA Ldr;
    PVOID ProcessParameters;
    PVOID SubSystemData;
    PVOID ProcessHeap;
    PVOID FastPebLock;
    PVOID AtlThunkSListPtr;
    PVOID IFEOKey;
    PVOID CrossProcessFlags;
    PVOID KernelCallbackTable;
    ULONG SystemReserved;
    ULONG AtlThunkSListPtr32;
    PVOID ApiSetMap;
} PEB, *PPEB;

typedef struct _PEB_LDR_DATA32
{
    ULONG Length;
    UCHAR Initialized;
    ULONG SsHandle;
    LIST_ENTRY32 InLoadOrderModuleList;
    LIST_ENTRY32 InMemoryOrderModuleList;
    LIST_ENTRY32 InInitializationOrderModuleList;
} PEB_LDR_DATA32, *PPEB_LDR_DATA32;

typedef struct _LDR_DATA_TABLE_ENTRY32
{
    LIST_ENTRY32 InLoadOrderLinks;
    LIST_ENTRY32 InMemoryOrderLinks;
    LIST_ENTRY32 InInitializationOrderLinks;
    ULONG DllBase;
    ULONG EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING32 FullDllName;
    UNICODE_STRING32 BaseDllName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
    LIST_ENTRY32 HashLinks;
    ULONG TimeDateStamp; // 0 if not bound,
} LDR_DATA_TABLE_ENTRY32, *PLDR_DATA_TABLE_ENTRY32;

typedef struct _LDR_DATA_TABLE_ENTRY
{
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
    LIST_ENTRY HashLinks;
    ULONG TimeDateStamp; // 0 if not bound,
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

/// <summary>
/// User-mode memory region
/// </summary>
typedef struct _USER_CONTEXT
{
    UCHAR code[0x1000]; // Code buffer
    // Activation context data
    union
    {
        UNICODE_STRING ustr;
        UNICODE_STRING32 ustr32;
    };
    wchar_t buffer[0x400]; // Buffer for unicode string

    union _ActCtxUnion
    {
        APC_ACTCTXW ActCtx;
        APC_ACTCTXW32 ActCtx32;
    } ActCtxData;
    HANDLE hCTX;
    ULONG hCookie;

    PVOID ptr; // Tmp data
    // Activation context data
    union
    {
        NTSTATUS status; // Last execution status
        PVOID retVal; // Function return value
        ULONG retVal32; // Function return value
    };
} USER_CONTEXT, *PUSER_CONTEXT;

/// <summary>
/// Manual map context
/// </summary>
typedef struct _MMAP_CONTEXT
{
    PEPROCESS pProcess; // Target process
    PVOID pWorkerBuf; // Worker thread code buffer
    HANDLE hWorker; // Worker thread handle
    PETHREAD pWorker; // Worker thread object
    LIST_ENTRY modules; // Manual module list
    PUSER_CONTEXT userMem; // Tmp buffer in user space
    HANDLE hSync; // APC sync handle
    PKEVENT pSync; // APC sync object
    PVOID pSetEvent; // ZwSetEvent address
    PVOID pLoadImage; // LdrLoadDll address
    BOOLEAN tlsInitialized; // Static TLS was initialized
    BOOLEAN noThreads; // No threads should be created
} MMAP_CONTEXT, *PMMAP_CONTEXT;

// Image name resolve flags
typedef enum _ResolveFlags
{
    KApiShemaOnly = 1,
    KSkipSxS = 2,
    KFullPath = 4,
} ResolveFlags;

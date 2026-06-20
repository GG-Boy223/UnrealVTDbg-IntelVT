#pragma once

#ifndef _DRIVER_H
#define _DRIVER_H

#define RING0

#include <ntifs.h>
#include <malloc.h>
#include <intrin.h>
#include <ntstrsafe.h>
#include <stdlib.h>
#include <Shared/SharedStruct.h>
#include <Shared/IOCTLs.h>
#include "../Common/Ring0/SymbolicAccess/Utils/Log.h"
#include "../Common/Ring0/SymbolicAccess/ModuleExtender/ModuleExtenderFactory.h"
#include "../Common/Ring0/Memory/AllocateMem.h"
#include "../Common/Ring0/SymbolicAccess/LogFile.h"
#include "../Common/Ring0/PE/PE_struct.h"
#include "../Common/Ring0/Inject/ApcInject/ApcInject.h"
#include "../Common/Ring0/String/StringHandler.h"
#include "ntos/inc/ntosdef.h"

typedef unsigned char BYTE;
typedef BYTE* PBYTE;
typedef UINT16 WORD;
typedef HANDLE  HWND;
typedef PIMAGE_NT_HEADERS64 PIMAGE_NT_HEADERS;
typedef ULONG WIN32_PROTECTION_MASK;
typedef PULONG PWIN32_PROTECTION_MASK;
typedef PVOID PMMVAD;

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))

#define __T(x)      L ## x
#define _T(x)       __T(x)


#define PS_CROSS_THREAD_FLAGS_TERMINATED           0x00000001UL

//
// Thread create failed
//

#define PS_CROSS_THREAD_FLAGS_DEADTHREAD           0x00000002UL

//
// Debugger isn't shown this thread
//

#define PS_CROSS_THREAD_FLAGS_HIDEFROMDBG          0x00000004UL

//
// Thread is impersonating
//

#define PS_CROSS_THREAD_FLAGS_IMPERSONATING        0x00000008UL

//
// This is a system thread
//

#define PS_CROSS_THREAD_FLAGS_SYSTEM               0x00002000UL

//
// Hard errors are disabled for this thread
//

#define PS_CROSS_THREAD_FLAGS_HARD_ERRORS_DISABLED 0x00000020UL

//
// We should break in when this thread is terminated
//

#define PS_CROSS_THREAD_FLAGS_BREAK_ON_TERMINATION 0x00000040UL

//
// This thread should skip sending its create thread message
//
#define PS_CROSS_THREAD_FLAGS_SKIP_CREATION_MSG    0x00000080UL

//
// This thread should skip sending its final thread termination message
//
#define PS_CROSS_THREAD_FLAGS_SKIP_TERMINATION_MSG 0x00000100UL

//
// Constants
//
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

/*
 * NOTE: Alignment of the pointers is not verified!
 */
#define ProbeForWriteGenericType(Ptr, Type)                                    \
    do {                                                                       \
        if ((ULONG_PTR)(Ptr) + sizeof(Type) - 1 < (ULONG_PTR)(Ptr) ||          \
            (ULONG_PTR)(Ptr) + sizeof(Type) - 1 >= (ULONG_PTR)MmUserProbeAddress) { \
            ExRaiseAccessViolation();                                          \
        }                                                                      \
        *(volatile Type *)(Ptr) = *(volatile Type *)(Ptr);                     \
    } while (0)

#define ProbeForWriteUlong(Ptr) ProbeForWriteGenericType(Ptr, ULONG)

 //
 // Cross Thread Flags
 //
#define CT_TERMINATED_BIT                       0x1
#define CT_DEAD_THREAD_BIT                      0x2
#define CT_HIDE_FROM_DEBUGGER_BIT               0x4
#define CT_ACTIVE_IMPERSONATION_INFO_BIT        0x8
#define CT_SYSTEM_THREAD_BIT                    0x10
#define CT_HARD_ERRORS_ARE_DISABLED_BIT         0x20
#define CT_BREAK_ON_TERMINATION_BIT             0x40
#define CT_SKIP_CREATION_MSG_BIT                0x80
#define CT_SKIP_TERMINATION_MSG_BIT             0x100

 //
 // Cross Thread Flag routines
 //
#define PspSetCrossThreadFlag(Thread, Flag)                 \
    InterlockedOr((PLONG)&Thread->CrossThreadFlags, Flag)
#define PspClearCrossThreadFlag(Thread, Flag)               \
    InterlockedAnd((PLONG)&Thread->CrossThreadFlags, ~Flag)

//
// Process flag routines
//
#define PspSetProcessFlag(Process, Flag) \
    InterlockedOr((PLONG)&Process->Flags, Flag)
#define PspClearProcessFlag(Process, Flag) \
    InterlockedAnd((PLONG)&Process->Flags, ~Flag)

#define PspSetProcessFlag2(Process, Flag) \
    InterlockedOr((PLONG)&Process->Flags2, Flag)

//
// Valid return values for the PORT_MESSAGE Type file
//
#define LPC_REQUEST             1
#define LPC_REPLY               2
#define LPC_DATAGRAM            3
#define LPC_LOST_REPLY          4
#define LPC_PORT_CLOSED         5
#define LPC_CLIENT_DIED         6
#define LPC_EXCEPTION           7
#define LPC_DEBUG_EVENT         8
#define LPC_ERROR_EVENT         9
#define LPC_CONNECTION_REQUEST 10

#define EFLAGS_USER_SANITIZE  0x003f4dd7L

#define MODE_MASK    1      // ntosp
#define RPL_MASK     3

// begin_wx86
//
//  GDT selectors - These defines are R0 selector numbers, which means
//                  they happen to match the byte offset relative to
//                  the base of the GDT.
//
#define KGDT_NULL       0
#define KGDT_R0_CODE    8
#define KGDT_R0_DATA    16
#define KGDT_R3_CODE    24
#define KGDT_R3_DATA    32
#define KGDT_TSS        40
#define KGDT_R0_PCR     48
#define KGDT_R3_TEB     56
#define KGDT_VDM_TILE   64
#define KGDT_LDT        72
#define KGDT_DF_TSS     80
#define KGDT_NMI_TSS    88

//
// Define flag to identify internally raised exceptions and define internal
// exception codes.
//
#define KI_EXCEPTION_INTERNAL               0x10000000
#define KI_EXCEPTION_GP_FAULT               (KI_EXCEPTION_INTERNAL | 0x1)
#define KI_EXCEPTION_INVALID_OP             (KI_EXCEPTION_INTERNAL | 0x2)
#define KI_EXCEPTION_INTEGER_DIVIDE_BY_ZERO (KI_EXCEPTION_INTERNAL | 0x3)
#define KI_EXCEPTION_ACCESS_VIOLATION       (KI_EXCEPTION_INTERNAL | 0x4)

#define PROCESS_TERMINATE         (0x0001)  // winnt
#define PROCESS_CREATE_THREAD     (0x0002)  // winnt
#define PROCESS_SET_SESSIONID     (0x0004)  // winnt
#define PROCESS_VM_OPERATION      (0x0008)  // winnt
#define PROCESS_VM_READ           (0x0010)  // winnt
#define PROCESS_VM_WRITE          (0x0020)  // winnt
// begin_ntddk begin_wdm begin_ntifs
#define PROCESS_DUP_HANDLE        (0x0040)  // winnt
// end_ntddk end_wdm end_ntifs
#define PROCESS_CREATE_PROCESS    (0x0080)  // winnt
#define PROCESS_SET_QUOTA         (0x0100)  // winnt
#define PROCESS_SET_INFORMATION   (0x0200)  // winnt
#define PROCESS_QUERY_INFORMATION (0x0400)  // winnt
#define PROCESS_SET_PORT          (0x0800)
#define PROCESS_SUSPEND_RESUME    (0x0800)  // winnt

static const LARGE_INTEGER __emptyLargeInteger = { {0, 0} };

#define ProbeForReadGenericType(Ptr, Type, Default)                            \
    (((ULONG_PTR)(Ptr) + sizeof(Type) - 1 < (ULONG_PTR)(Ptr) ||                \
     (ULONG_PTR)(Ptr) + sizeof(Type) - 1 >= (ULONG_PTR)MmUserProbeAddress) ?   \
         ExRaiseAccessViolation(), Default :                     \
         *(const volatile Type *)(Ptr))

#define ProbeForReadLargeInteger(Ptr) ProbeForReadGenericType((const LARGE_INTEGER *)(Ptr), LARGE_INTEGER, __emptyLargeInteger)

//
// Debug Event Flags
//
#define DEBUG_KILL_ON_CLOSE  (0x1)
#define DEBUG_EVENT_READ                  (0x01)
#define DEBUG_EVENT_NOWAIT                (0x02)
#define DEBUG_EVENT_INACTIVE              (0x04)
#define DEBUG_EVENT_RELEASE               (0x08)
#define DEBUG_EVENT_PROTECT_FAILED        (0x10)
#define DEBUG_EVENT_SUSPEND               (0x20)

//
// Define debug object access types. No security is present on this object.
//
#define DEBUG_READ_EVENT        (0x0001)
#define DEBUG_PROCESS_ASSIGN    (0x0002)
#define DEBUG_SET_INFORMATION   (0x0004)
#define DEBUG_QUERY_INFORMATION (0x0008)
#define DEBUG_ALL_ACCESS     (STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE|DEBUG_READ_EVENT|DEBUG_PROCESS_ASSIGN|\
                              DEBUG_SET_INFORMATION|DEBUG_QUERY_INFORMATION)

//调试对象属性
#define DEBUG_OBJECT_DELETE_PENDING (0x1)  //调试对象处于删除挂起状态
#define DEBUG_OBJECT_KILL_ON_CLOSE  (0x2)  //关闭时终止所有调试的进程

//
// These define the Debug Masks Supported
//
#define DBGK_THREAD_DEBUG                               0x01
#define DBGK_PROCESS_DEBUG                              0x02
#define DBGK_OBJECT_DEBUG                               0x04
#define DBGK_MESSAGE_DEBUG                              0x08
#define DBGK_EXCEPTION_DEBUG                            0x10

#define IS_SYSTEM_THREAD(Thread)  (((Thread)->Tcb.MiscFlags&PS_CROSS_THREAD_FLAGS_SYSTEM) != 0)

#define _SEH2_TRY __try
#define _SEH2_FINALLY __finally
#define _SEH2_EXCEPT(...) __except(__VA_ARGS__)
#define _SEH2_END
#define _SEH2_GetExceptionInformation() (GetExceptionInformation())
#define _SEH2_GetExceptionCode() (GetExceptionCode())
#define _SEH2_AbnormalTermination() (AbnormalTermination())
#define _SEH2_YIELD(STMT_) STMT_
#define _SEH2_LEAVE __leave
#define _SEH2_VOLATILE

#ifndef __RELFILE__
#define __RELFILE__ __FILE__
#endif

#define DPRINT(fmt, ...) do { \
            if (DbgPrint("(%s:%d) " fmt, __RELFILE__, __LINE__, ##__VA_ARGS__))  \
                DbgPrint("(%s:%d) DbgPrint() failed!\n", __RELFILE__, __LINE__); \
        } while (0)

#define DPRINT1(fmt, ...) do { \
        if (DbgPrint("(%s:%d) " fmt, __RELFILE__, __LINE__, ##__VA_ARGS__))  \
            DbgPrint("(%s:%d) DbgPrint() failed!\n", __RELFILE__, __LINE__); \
    } while (0)

//
// DbgKm Apis are from the kernel component (Dbgk) through a process
// debug port.
//
#define DBGKM_MSG_OVERHEAD \
    (FIELD_OFFSET(DBGKM_APIMSG, u.Exception) - sizeof(PORT_MESSAGE))

#define DBGKM_API_MSG_LENGTH(TypeSize) \
    ((sizeof(DBGKM_APIMSG) << 16) | (DBGKM_MSG_OVERHEAD + (TypeSize)))

#define DBGKM_FORMAT_API_MSG(m,Number,TypeSize)             \
    (m).h.u1.Length = DBGKM_API_MSG_LENGTH((TypeSize));     \
    (m).h.u2.ZeroInit = LPC_DEBUG_EVENT;                    \
    (m).ApiNumber = (Number)

#define SANITIZE_SEG(segCS, mode) (        \
    ((mode) == KernelMode ? \
    ((0x00000000L) | ((segCS)& 0xfffc)) : \
    ((0x00000003L) | ((segCS)& 0xffff))))

#define CHECK_FUNC_PTR(ptr) \
    if ((ptr) == nullptr) { \
        DbgPrint("Missing function pointer: %s\n", #ptr);/*触发蓝屏dump信息*/ \
    }

//#define _DBGK_DEBUG_

#ifndef _DBGK_DEBUG_
#define DBGKTRACE(x, fmt, ...)
#else
#define DBGKTRACE(x, fmt, ...) DPRINT(fmt, ##__VA_ARGS__)
#endif // !_DBGK_DEBUG_


#define ProbeForWriteHandle(Address) {                                   \
if ((Address) >= (HANDLE * const)MM_USER_PROBE_ADDRESS) {                \
*(volatile HANDLE * const)MM_USER_PROBE_ADDRESS = 0;                     \
}                                                                        \
                                                                         \
*(volatile HANDLE *)(Address) = *(volatile HANDLE *)(Address);           \
    }



// begin_nthal
//
// Define stack alignment and rounding values.
//

#define STACK_ALIGN2 (16)

#define STACK_ALIGN (15)
#define STACK_ROUND (STACK_ALIGN - 1)

#define XSTATE_STACK_ALIGN (0x40)
#define XSTATE_STACK_ROUND (XSTATE_STACK_ALIGN - 1)


#define EXCEPTION_RECORD_LENGTH                                              \
    ((sizeof(EXCEPTION_RECORD) + STACK_ROUND) & ~STACK_ROUND)


#define CONTEXT_EX_LENGTH    ((sizeof(CONTEXT_EX) + STACK_ROUND) & ~STACK_ROUND)

#define CONTEXT_LENGTH sizeof(CONTEXT)

#define STATUS_BYPASS_SINGLE_STEP ((NTSTATUS)0x66666666L)

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT pDevice;
    UNICODE_STRING ustrDeviceName;	//设备名称
    UNICODE_STRING ustrSymLinkName;	//符号链接名
    PUCHAR buffer;//缓冲区
    ULONG file_length;//模拟的文件长度，必须小于MAX_FILE_LENGTH
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;



VOID WP_OFF(); //关闭写保护
VOID WP_ON(); //开启写保护

EXTERN_C
BOOLEAN SafeCopyMemory(PVOID pDest, PVOID pSrc, ULONG dwSize);

LONG
ExSystemExceptionFilter(VOID);


/**************************** 外部导入函数 ****************************/

EXTERN_C ULONG64
__readgsqword(
    _In_ ULONG Offset
);

EXTERN_C
NTSYSAPI
PACCESS_TOKEN
NTAPI
PsReferencePrimaryToken(
    __inout PEPROCESS Process
);

EXTERN_C
NTSYSAPI
NTSTATUS
NTAPI
ZwFreeVirtualMemory(
    __in HANDLE ProcessHandle,
    __inout PVOID* BaseAddress,
    __inout PSIZE_T RegionSize,
    __in ULONG FreeType
);

//根据对象获得句柄
EXTERN_C
NTSYSAPI
NTSTATUS
NTAPI
ObOpenObjectByPointer(IN PVOID Object,
    IN ULONG HandleAttributes,
    IN PACCESS_STATE PassedAccessState,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_TYPE ObjectType,
    IN KPROCESSOR_MODE AccessMode,
    OUT PHANDLE Handle);

EXTERN_C
NTSYSAPI
NTSTATUS
NTAPI
ZwFlushInstructionCache(IN HANDLE 	ProcessHandle,
    IN PVOID 	BaseAddress,
    IN ULONG 	NumberOfBytesToFlush
);

EXTERN_C
NTSYSAPI
VOID
NTAPI
ExRaiseException(IN PEXCEPTION_RECORD ExceptionRecord);

//导入函数
EXTERN_C
NTSYSAPI
NTSTATUS
NTAPI
ObCreateObject(KPROCESSOR_MODE ProbeMode,
    POBJECT_TYPE ObjectType,
    POBJECT_ATTRIBUTES ObjectAttributes,
    KPROCESSOR_MODE OwnershipMode,
    PVOID ParseContext OPTIONAL,
    ULONG ObjectBodySize,
    ULONG PagedPoolCharge,
    ULONG NonPagedPoolCharge,
    PVOID* Object);

EXTERN_C
NTSYSAPI
NTSTATUS
NTAPI
ObInsertObject(PVOID Object,
    PACCESS_STATE PassedAccessState,
    ACCESS_MASK DesiredAccess,
    ULONG ObjectPointerBias,
    PVOID* NewObject,
    PHANDLE Handle);

EXTERN_C
NTKERNELAPI
VOID
FASTCALL ExfAcquirePushLockShared(
    __inout PEX_PUSH_LOCK PushLock
);

EXTERN_C
NTKERNELAPI
VOID
FASTCALL ExfReleasePushLockShared(
    __inout PEX_PUSH_LOCK PushLock
);

EXTERN_C
NTKERNELAPI
VOID
FASTCALL
ExfAcquirePushLockExclusive(
    __inout PEX_PUSH_LOCK PushLock
);

EXTERN_C
NTKERNELAPI
VOID
FASTCALL
ExfReleasePushLockExclusive(
    __inout PEX_PUSH_LOCK PushLock
);

EXTERN_C
NTKERNELAPI
VOID
FASTCALL
ExfTryToWakePushLock(
    __inout PEX_PUSH_LOCK PushLock
);


NTSTATUS
PsSuspendThread(
    IN _ETHREAD* Thread,
    OUT PULONG PreviousSuspendCount OPTIONAL
);

NTSTATUS
PsResumeThread(
    IN _ETHREAD* Thread,
    OUT PULONG PreviousSuspendCount OPTIONAL
);

//创建设备 符号链接等
NTSTATUS CreateDevice(IN PDRIVER_OBJECT pDriver_Object);

//删除设备
VOID _RemoveDevice(IN PDRIVER_OBJECT pDriver_Object);

NTSTATUS InitDispatchRoutin(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp);

NTSTATUS HandlerDispatchRoutin(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp);

//VOID InitFileList(PPROTECT_OBJECT_DATA pProtectObj);
//
//VOID InitWindowList(PPROTECT_OBJECT_DATA pProtectObj);
//
//初始化保护列表
VOID InitProtectList(IN PUSER_DATA userData, IN PIRP pIrp);

VOID InitDebuggerState(PDEBUGGER_STATE pDbgState);

//初始化调试器列表
VOID InitDebuggerInfo(IN PUSER_DATA userData);

VOID VerifyHookClient(IN PUSER_DATA userData, IN PIRP pIrp);

VOID InitSymbolicVariable();

VOID InitGlobalVariable(PDRIVER_OBJECT DriverObject);

//判断目标进程是否是我们自己
BOOLEAN IsSelf(PEPROCESS Process);

//判断目标进程是否是我们自己的调试器
BOOLEAN IsDebugger(PEPROCESS Process);

BOOLEAN IsProtectTargetProcess(_EPROCESS* Process);

EXTERN_C
VOID RemoveHooks();

EXTERN_C
NTSTATUS TestVMM();

EXTERN_C
NTSTATUS
ProxyDbgkOpenProcessDebugPort(
    IN PEPROCESS Process,
    IN KPROCESSOR_MODE PreviousMode,
    OUT HANDLE* pHandle
);

void CreateRemoteThread(IN PUSER_DATA userData);

//初始化保护
VOID InitProtect(IN PDRIVER_OBJECT DriverObject);

//卸载保护
VOID UnloadProtect();


EXTERN_C
ULONG_PTR SearchCode(unsigned char* code, ULONG_PTR codeLenth, unsigned char* startaddr, ULONG_PTR addrlenth);

EXTERN_C size_t __fastcall LDE(const void* lpData, unsigned int size);

EXTERN_C
PVOID DbgkCreateThread_CMP_Debugport_01();

VOID SetupHook_DbgkCreateThread_CMP_Debugport();
VOID SetupHook_PspExitThread_CMP_Debugport();

KPROCESSOR_MODE KeGetPreviousMode();

KPROCESSOR_MODE KeSetPreviousMode(KPROCESSOR_MODE PreviousMode);

VOID PrintProcessName(_EPROCESS* Process);

NTSTATUS NewObReferenceObjectByHandle(HANDLE Handle,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    PVOID* Object,
    POBJECT_HANDLE_INFORMATION HandleInformation);

NTSTATUS NewObReferenceObjectByHandleWithTag(HANDLE Handle,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    int a5,
    PVOID* Object,
    __int64 a7);

NTSTATUS NewObpReferenceObjectByHandleWithTag(HANDLE Handle,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    ULONG Tag,
    PVOID* Object,
    POBJECT_HANDLE_INFORMATION HandleInformation,
    __int64 a8);

LONG_PTR NewObfDereferenceObject(
    _In_ PVOID Object
);

LONG_PTR NewObfDereferenceObjectWithTag(
    _In_ PVOID Object,
    _In_ ULONG Tag
);

NTSTATUS NewNtOpenProcess(PHANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PCLIENT_ID ClientId);

VOID NewPspCreateUserContext(_CONTEXT* context,
    char a2,
    PVOID ApcFunc,
    PVOID pUserfunc,
    __int64 a5);


VOID NewPspCallThreadNotifyRoutines(_ETHREAD* Thread,
    BOOLEAN Create,
    BOOLEAN a3);

NTSTATUS NewPspAllocateThread(_EPROCESS* Process,
    POBJECT_ATTRIBUTES ObjectAttributes,
    unsigned __int8 a3,
    __int64 a4,
    __int64 a5,
    __int64 a6,
    PKSTART_ROUTINE StartRoutine,
    __int64 a8,
    __int64 a9,
    __int64 a10,
    __int64 a11,
    __int64 a12);

NTSTATUS NtProtectVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG flNewProtect, PULONG flOldProtect);

PMMVAD MiObtainReferencedVadEx(PVOID StartingAddress, char a2, PNTSTATUS status);

NTSTATUS MmProtectVirtualMemory(_EPROCESS* sourceProcess,
    _EPROCESS* TargetProcess,
    PVOID* BaseAddress,
    PSIZE_T RegionSize,
    WIN32_PROTECTION_MASK NewProtectWin32,
    PWIN32_PROTECTION_MASK LastProtect);

void GetProcessInfo(IN PUSER_DATA userData, IN PIRP pIrp);

void TL_BlockGameResumeThread(IN PUSER_DATA userData, IN PIRP pIrp);

VOID
NewKeStackAttachProcess(
    _Inout_ PRKPROCESS PROCESS,
    _Out_ PRKAPC_STATE ApcState
);

VOID NewKiStackAttachProcess(_KPROCESS* Process, BOOLEAN a2, _KAPC_STATE* ApcState);

NTSTATUS
NewNtTerminateProcess(
    __in_opt HANDLE ProcessHandle,
    __in NTSTATUS ExitStatus
);

NTSTATUS
NewNtSuspendThread(
    __in HANDLE ThreadHandle,
    __out_opt PULONG PreviousSuspendCount
);

NTSTATUS
NewNtResumeThread(
    __in HANDLE ThreadHandle,
    __out_opt PULONG PreviousSuspendCount
);

NTSTATUS
NewNtQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _Out_ PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength
);

USER_DATA GetUserData(PUSER_DATA userData);

extern PWCHAR PassProcessList[12];

#endif // !_DRIVER_H

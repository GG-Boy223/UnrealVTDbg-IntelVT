#pragma once

#ifndef _DBGKAPI_H
#define _DBGKAPI_H

#define DBGK_EXCEPTIONPORT_MASK           7   //异常端口

//
// NtCreateDebugObject Flags
//
#define DBGK_KILL_PROCESS_ON_EXIT         (0x1)
#define DBGK_ALL_FLAGS                    (DBGK_KILL_PROCESS_ON_EXIT)


NTSTATUS DbgkInitialize();

VOID DbgkUnInitialize();

extern volatile LONG g_DbgkActiveRuntimeCallCount;

inline BOOLEAN DbgkIsDriverRuntimeUnloading()
{
    return InterlockedCompareExchange(&g_DbgkDriverUnloading, 0, 0) != 0;
}

class DbgkRuntimeCallGuard
{
public:
    DbgkRuntimeCallGuard()
    {
        InterlockedIncrement(&g_DbgkActiveRuntimeCallCount);
    }

    ~DbgkRuntimeCallGuard()
    {
        InterlockedDecrement(&g_DbgkActiveRuntimeCallCount);
    }
};

//调试器运行后 为调试器创建调试对象
EXTERN_C
NTSTATUS NtCreateDebugObject(OUT PHANDLE DebugHandle,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_ATTRIBUTES ObjectAttributes,
	IN ULONG Flags);

NTSTATUS
NtSetInformationDebugObject(
	IN HANDLE DebugObjectHandle,
	IN DEBUGOBJECTINFOCLASS DebugObjectInformationClass,
	IN PVOID DebugInformation,
	IN ULONG DebugInformationLength,
	OUT PULONG ReturnLength OPTIONAL
);

EXTERN_C
NTSTATUS DbgkOpenProcessDebugPort(IN PEPROCESS Process,
	IN KPROCESSOR_MODE PreviousMode,
	OUT HANDLE* DebugHandle);
//
//NTSTATUS DbgkpPostFakeProcessCreateMessages(IN _EPROCESS* Process,
//	IN PDEBUG_OBJECT DebugObject,
//	OUT PMY_ETHREAD* LastThread);

NTSTATUS DbgkpSetProcessDebugObject(IN _EPROCESS* Process,
	IN PDEBUG_OBJECT DebugObject,
	IN NTSTATUS MsgStatus,
	IN _ETHREAD* LastThread);

//NTSTATUS DbgkpPostFakeThreadMessages(IN _EPROCESS* Process,
//	IN PDEBUG_OBJECT DebugObject,
//	IN PMY_ETHREAD StartThread,
//	OUT PMY_ETHREAD* FirstThread,
//	OUT PMY_ETHREAD* LastThread);
//
//NTSTATUS DbgkpPostModuleMessages(IN _EPROCESS* Process,
//	IN PMY_ETHREAD Thread,
//	IN PDEBUG_OBJECT DebugObject);
//
//VOID DbgkpFreeDebugEvent(IN PDEBUG_EVENT DebugEvent);
//
//VOID DbgkpWakeTarget(IN PDEBUG_EVENT DebugEvent);
//
//VOID DbgkpMarkProcessPeb(IN _EPROCESS* Process);
//
//HANDLE DbgkpSectionToFileHandle(IN PVOID Section);
//
NTSTATUS DbgkpQueueMessage(IN _EPROCESS* Process,
	IN _ETHREAD* Thread,
	IN PDBGKM_APIMSG Message,
	IN ULONG Flags,
	IN PDEBUG_OBJECT TargetObject OPTIONAL);
//
//NTSTATUS DbgkpSendApiMessageLpc(IN OUT PDBGKM_APIMSG Message,
//	IN PVOID Port,
//	IN BOOLEAN SuspendProcess);
//
//NTSTATUS __fastcall DbgkpSendApiMessage(_EPROCESS* Process, BOOLEAN SuspendProcess, PDBGKM_APIMSG ApiMsg);

BOOLEAN DbgkForwardException(IN PEXCEPTION_RECORD ExceptionRecord,
	IN BOOLEAN DebugPort,
	IN BOOLEAN SecondChance);

EXTERN_C
NTSTATUS NtDebugActiveProcess(IN HANDLE ProcessHandle,
	IN HANDLE DebugHandle);

EXTERN_C
NTSTATUS NtWaitForDebugEvent(IN HANDLE DebugHandle,
	IN BOOLEAN Alertable,
	IN PLARGE_INTEGER Timeout OPTIONAL,
	OUT PDBGUI_WAIT_STATE_CHANGE StateChange);

EXTERN_C
NTSTATUS
NtRemoveProcessDebug(
	IN HANDLE ProcessHandle,
	IN HANDLE DebugObjectHandle
);

EXTERN_C
VOID DbgkCreateThread(IN PETHREAD Thread);

EXTERN_C
VOID
DbgkExitThread(NTSTATUS ExitStatus);

EXTERN_C
VOID
DbgkExitProcess(NTSTATUS ExitStatus);

EXTERN_C
VOID PspExitThread(_In_ NTSTATUS ExitStatus);
//
////PVOID PsCaptureExceptionPort(_EPROCESS* Process);
//
//NTSTATUS DbgkpSendErrorMessage(PEXCEPTION_RECORD ExceptionRecord,
//	ULONG Falge, PDBGKM_APIMSG ApiMsg);
//
//VOID DbgkpDeleteErrorPort(PDBGKP_ERROR_PORT DbgErrorPort);

//VOID
//NTAPI
//DbgkpConvertKernelToUserStateChange(OUT PDBGUI_WAIT_STATE_CHANGE WaitStateChange,
//	IN PDEBUG_EVENT DebugEvent);

//VOID
//NTAPI
//DbgkpOpenHandles(IN PDBGUI_WAIT_STATE_CHANGE WaitStateChange,
//	IN _EPROCESS* Process,
//	IN _ETHREAD* Thread);

EXTERN_C
NTSTATUS
NtDebugContinue(
	IN HANDLE DebugObjectHandle,
	IN PCLIENT_ID ClientId,
	IN NTSTATUS ContinueStatus);

EXTERN_C
VOID
DbgkMapViewOfSection(IN _EPROCESS* Process,
	IN PVOID SectionObject,
	IN PVOID BaseAddress,
	IN ULONG SectionOffset,
	IN ULONG_PTR ViewSize);

//DLL卸载
EXTERN_C
VOID
DbgkUnMapViewOfSection(IN _EPROCESS* Process, IN PVOID BaseAddress);
//
//
////设置调试对象
//BOOLEAN SetDebugTargetProcess(_EPROCESS* Process, PDEBUG_OBJECT DebugObject);
//

EXTERN_C
VOID
DbgkpCloseObject(
	IN _EPROCESS* Process,
	IN PVOID Object,
	IN ACCESS_MASK GrantedAccess,
	IN ULONG_PTR SystemHandleCount
);

VOID DeleteDebugProcess(PDEBUG_OBJECT DebugObject);

POBJECT_TYPE GetDebugObjectType(UNICODE_STRING Name);

//判断目标进程是否是被调试的进程
BOOLEAN IsDebugTargetProcess(IN _EPROCESS* Process,
	OUT PDEBUG_PROCESS* DebugProcess);

BOOLEAN InsertDebuggerList(PDEBUGGER_TABLE_ENTRY Debugger);

#endif // !_DBGKAPI_H
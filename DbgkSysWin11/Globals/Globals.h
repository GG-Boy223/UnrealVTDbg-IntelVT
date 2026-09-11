#pragma once

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "../List/MyList.h"  // 包含 MY_LIST 类型定义
#include "../ntos/inc/ntdbg.h"  // 包含调试相关类型定义
#include "../ntos/inc/ntlpcapi.h"  // 包含 LPC 相关类型定义

#define TAG_PRO              'mema'

typedef struct tagPOINT
{
    LONG  x;
    LONG  y;
} POINT, * PPOINT;

typedef struct tagTHREADINFO
{
    /* 0x0000 */ struct _ETHREAD* pEThread;
// /* 0x0000 */ struct _ETHREAD* pEThread;
}THREADINFO, * PTHREADINFO;

typedef struct _THRDESKHEAD
{
    /* 0x0000 */ void* h;
    /* 0x0008 */ unsigned long cLockObj;
    /* 0x000c */ long Padding_21;
    /* 0x0010 */ struct tagTHREADINFO* pti;
    /* 0x0018 */ struct tagDESKTOP* rpdesk;
    /* 0x0020 */ unsigned char* pSelf;
} THRDESKHEAD, * PTHRDESKHEAD; /* size: 0x0028 */

typedef struct tagWND
{
    struct _THRDESKHEAD head;
}WND, * PWND;

typedef enum _SYSTEM_DLL_TYPE
{
    PsNativeSystemDll = 0,
    PsWowX86SystemDll = 1,
    PsWowArm32SystemDll = 2,
    PsWowChpeX86SystemDll = 3,
    PsChpeV2SystemDll = 4,
    PsVsmEnclaveRuntimeDll = 5,
    PsTrustedAppsRuntimeDll = 6,
    PsSystemDllTotalTypes = 7,
} SYSTEM_DLL_TYPE, * PSYSTEM_DLL_TYPE;

    //25H2 26200.8037  
//typedef enum _SYSTEM_DLL_TYPE
//{
//    PsNativeSystemDll = 0,
//    PsWowX86SystemDll = 1,
//    PsWowChpeX86SystemDll = 2,
//    PsChpeV2SystemDll = 3,
//    PsVsmEnclaveRuntimeDll = 4,
//    PsTrustedAppsRuntimeDll = 5,
//    PsSystemDllTotalTypes = 6,
//} SYSTEM_DLL_TYPE;


typedef enum _WINDOWINFOCLASS
{
    WindowProcess = 0,
    WindowRealProcess = 1,
    WindowThread = 2,
    WindowActiveWindow = 3,
    WindowFocusWindow = 4,
    WindowIsHung = 5,
    WindowClientBase = 6,
    WindowIsForegroundThread = 7,
    WindowDefaultImeWindow = 8,
    WindowDefaultInputContext = 9,
} WINDOWINFOCLASS, * PWINDOWINFOCLASS;

union PROCESSFLAGS {
    /* 0x0464 */ unsigned long Flags;
    struct /* bitfield */
    {
        /* 0x0464 */ unsigned long CreateReported : 1; /* bit position: 0 */
        /* 0x0464 */ unsigned long NoDebugInherit : 1; /* bit position: 1 */
        /* 0x0464 */ unsigned long ProcessExiting : 1; /* bit position: 2 */
        /* 0x0464 */ unsigned long ProcessDelete : 1; /* bit position: 3 */
        /* 0x0464 */ unsigned long ManageExecutableMemoryWrites : 1; /* bit position: 4 */
        /* 0x0464 */ unsigned long VmDeleted : 1; /* bit position: 5 */
        /* 0x0464 */ unsigned long OutswapEnabled : 1; /* bit position: 6 */
        /* 0x0464 */ unsigned long Outswapped : 1; /* bit position: 7 */
        /* 0x0464 */ unsigned long FailFastOnCommitFail : 1; /* bit position: 8 */
        /* 0x0464 */ unsigned long Wow64VaSpace4Gb : 1; /* bit position: 9 */
        /* 0x0464 */ unsigned long AddressSpaceInitialized : 2; /* bit position: 10 */
        /* 0x0464 */ unsigned long SetTimerResolution : 1; /* bit position: 12 */
        /* 0x0464 */ unsigned long BreakOnTermination : 1; /* bit position: 13 */
        /* 0x0464 */ unsigned long DeprioritizeViews : 1; /* bit position: 14 */
        /* 0x0464 */ unsigned long WriteWatch : 1; /* bit position: 15 */
        /* 0x0464 */ unsigned long ProcessInSession : 1; /* bit position: 16 */
        /* 0x0464 */ unsigned long OverrideAddressSpace : 1; /* bit position: 17 */
        /* 0x0464 */ unsigned long HasAddressSpace : 1; /* bit position: 18 */
        /* 0x0464 */ unsigned long LaunchPrefetched : 1; /* bit position: 19 */
        /* 0x0464 */ unsigned long Background : 1; /* bit position: 20 */
        /* 0x0464 */ unsigned long VmTopDown : 1; /* bit position: 21 */
        /* 0x0464 */ unsigned long ImageNotifyDone : 1; /* bit position: 22 */
        /* 0x0464 */ unsigned long PdeUpdateNeeded : 1; /* bit position: 23 */
        /* 0x0464 */ unsigned long VdmAllowed : 1; /* bit position: 24 */
        /* 0x0464 */ unsigned long ProcessRundown : 1; /* bit position: 25 */
        /* 0x0464 */ unsigned long ProcessInserted : 1; /* bit position: 26 */
        /* 0x0464 */ unsigned long DefaultIoPriority : 3; /* bit position: 27 */
        /* 0x0464 */ unsigned long ProcessSelfDelete : 1; /* bit position: 30 */
        /* 0x0464 */ unsigned long SetTimerResolutionLink : 1; /* bit position: 31 */
    }; /* bitfield */
}; /* size: 0x0004 */

union CROSSTHREADFLAGS {
    /* 0x0560 */ unsigned long All;
    struct /* bitfield */
    {
        /* 0x0560 */ unsigned long Terminated : 1; /* bit position: 0 */
        /* 0x0560 */ unsigned long ThreadInserted : 1; /* bit position: 1 */
        /* 0x0560 */ unsigned long HideFromDebugger : 1; /* bit position: 2 */
        /* 0x0560 */ unsigned long ActiveImpersonationInfo : 1; /* bit position: 3 */
        /* 0x0560 */ unsigned long HardErrorsAreDisabled : 1; /* bit position: 4 */
        /* 0x0560 */ unsigned long BreakOnTermination : 1; /* bit position: 5 */
        /* 0x0560 */ unsigned long SkipCreationMsg : 1; /* bit position: 6 */
        /* 0x0560 */ unsigned long SkipTerminationMsg : 1; /* bit position: 7 */
        /* 0x0560 */ unsigned long CopyTokenOnOpen : 1; /* bit position: 8 */
        /* 0x0560 */ unsigned long ThreadIoPriority : 3; /* bit position: 9 */
        /* 0x0560 */ unsigned long ThreadPagePriority : 3; /* bit position: 12 */
        /* 0x0560 */ unsigned long RundownFail : 1; /* bit position: 15 */
        /* 0x0560 */ unsigned long UmsForceQueueTermination : 1; /* bit position: 16 */
        /* 0x0560 */ unsigned long IndirectCpuSets : 1; /* bit position: 17 */
        /* 0x0560 */ unsigned long DisableDynamicCodeOptOut : 1; /* bit position: 18 */
        /* 0x0560 */ unsigned long ExplicitCaseSensitivity : 1; /* bit position: 19 */
        /* 0x0560 */ unsigned long PicoNotifyExit : 1; /* bit position: 20 */
        /* 0x0560 */ unsigned long DbgWerUserReportActive : 1; /* bit position: 21 */
        /* 0x0560 */ unsigned long ForcedSelfTrimActive : 1; /* bit position: 22 */
        /* 0x0560 */ unsigned long SamplingCoverage : 1; /* bit position: 23 */
        /* 0x0560 */ unsigned long ReservedCrossThreadFlags : 8; /* bit position: 24 */
    }; /* bitfield */
}; /* size: 0x0004 */

//Vol3B[18.2 DEBUG REGISTERS]
union Dr6 {
    uint64_t flags;
    struct
    {
        uint64_t B0 : 1; //bit0
        uint64_t B1 : 1; //bit1
        uint64_t B2 : 1; //bit2
        uint64_t B3 : 1; //bit3
        uint64_t Reserved_1 : 7; //bit10:4
        uint64_t BLD : 1; //bit11
        uint64_t Reserved_2 : 1; //bit12
        uint64_t BD : 1; //bit13
        uint64_t BS : 1; //bit14 (single step)
        uint64_t BT : 1; //bit15
        uint64_t RTM : 1; //bit16
    };
};

union EFLAGS
{
    uint32_t flags;
    struct
    {
        uint32_t CF : 1; //bit0
        uint32_t Reserved_1 : 1; //bit1
        uint32_t PF : 1; //bit2
        uint32_t Reserved_2 : 1; //bit3
        uint32_t AF : 1; //bit4
        uint32_t Reserved_3 : 1; //bit5
        uint32_t ZF : 1; //bit6
        uint32_t SF : 1; //bit7
        uint32_t TF : 1; //bit8
        uint32_t IF : 1; //bit9
        uint32_t DF : 1; //bit10
        uint32_t OF : 1; //bit11
        uint32_t IOPL : 2; //bit13:12
        uint32_t NT : 1; //bit14
        uint32_t Reserved_4 : 1; //bit15
        uint32_t RF : 1; //bit16
        uint32_t VM : 1; //bit17
        uint32_t AC : 1; //bit18
        uint32_t VIF : 1; //bit19
        uint32_t VIP : 1; //bit20
        uint32_t ID : 1; //bit21
    };
};

typedef enum _DEBUGOBJECTINFOCLASS {
    DebugObjectFlags = 1,
    MaxDebugObjectInfoClass
} DEBUGOBJECTINFOCLASS, * PDEBUGOBJECTINFOCLASS;

typedef struct _DBGKP_ERROR_PORT
{
    ULONG RefCount;
    ULONG Flags;
    HANDLE PortHandle;
} DBGKP_ERROR_PORT, * PDBGKP_ERROR_PORT; /* size: 0x0010 */

#pragma pack(push, 8)
typedef struct _Debugport_Struct
{
    unsigned __int64 id;
    size_t DebugPort;
}Debugport_Struct, * PDebugport_Struct;
#pragma pack(pop)

typedef enum _PSW32THREADCALLOUTTYPE {
    PsW32ThreadCalloutInitialize,
    PsW32ThreadCalloutExit
} PSW32THREADCALLOUTTYPE;

typedef enum _SHUTDOWN_ACTION
{
    ShutdownNoReboot,
    ShutdownReboot,
    ShutdownPowerOff
} SHUTDOWN_ACTION, * PSHUTDOWN_ACTION;

typedef struct _FILEDATA
{
    LIST_ENTRY list_entry;
    UNICODE_STRING fileName;
    UNICODE_STRING filePath;
}FILEDATA, * PFILEDATA;

typedef struct _WINDOW_DATA
{
    LIST_ENTRY list_entry;
    UNICODE_STRING WindowName;
}WINDOW_DATA, * PWINDOW_DATA;

typedef struct _PROTECTOBJ
{
    MY_LIST list_entry;    //���������б�
    FAST_MUTEX Mutex;          //������
}PROTECTOBJ, * PPROTECTOBJ;

typedef struct _DEBUGGER_TABLE
{
    MY_LIST list_entry;        //ָ��DEBUGGER_DATA����
    FAST_MUTEX Mutex;          //������
}DEBUGGER_TABLE, * PDEBUGGER_TABLE;

//
// Debug Object
//
typedef struct _DEBUG_OBJECT
{
    KEVENT EventsPresent;        //���¼��б������ʱ���õ��¼���
    FAST_MUTEX Mutex;            //������
    LIST_ENTRY EventList;        //�����¼��Ķ���
    ULONG Flags;
} DEBUG_OBJECT, * PDEBUG_OBJECT;

typedef struct _MINI_CONTEXT_WOW64
{
    BOOLEAN initializated;  //�û��ж�shadow context�Ƿ��Ѿ���ʼ��
    unsigned long Dr0;
    unsigned long Dr1;
    unsigned long Dr2;
    unsigned long Dr3;
    unsigned long Dr6;
    unsigned long Dr7;
    unsigned long EFlags;
// /* 0x0004 */ unsigned long Dr0;
// /* 0x0008 */ unsigned long Dr1;
// /* 0x000c */ unsigned long Dr2;
// /* 0x0010 */ unsigned long Dr3;
// /* 0x0014 */ unsigned long Dr6;
// /* 0x0018 */ unsigned long Dr7;
// /* 0x00c0 */ unsigned long EFlags;
}MINI_CONTEXT_WOW64, * PMINI_CONTEXT_WOW64;

typedef struct _MINI_CONTEXT
{
    BOOLEAN initializated;  //�û��ж�shadow context�Ƿ��Ѿ���ʼ��
    ULONG64 Dr0;
    ULONG64 Dr1;
    ULONG64 Dr2;
    ULONG64 Dr3;
    ULONG64 Dr6;
    ULONG64 Dr7;
    ULONG EFlags;
}MINI_CONTEXT, * PMINI_CONTEXT;

//���Խ��̶���
//调试进程对象
typedef struct _DEBUG_PROCESS
{
    LIST_ENTRY list_entry;
    FAST_MUTEX Mutex;          //������
    _EPROCESS* Process;  //���̶���
    PDEBUG_OBJECT DebugObject;  //���Զ���
    BOOLEAN DebugObjectReferenced;
    MINI_CONTEXT Context;
    MINI_CONTEXT_WOW64 wow64Context;
}DEBUG_PROCESS, * PDEBUG_PROCESS;

typedef struct _DEBUG_PROCESS_TABLE
{
    MY_LIST list_entry;    //ָ��DEBUG_PROCESS����
    FAST_MUTEX Mutex;          //������
}DEBUG_PROCESS_TABLE, * PDEBUG_PROCESS_TABLE;


typedef struct _BREAKPOINT_TABLE_ENTRY
{
    LIST_ENTRY list_entry;
    _EPROCESS* Process;  //���̶���
    PVOID BreakpointAddress;  //�ϵ��ַ
    //BYTE firstByte;  //���ֽ�����
    //BYTE originalByte  //ԭʼ�ֽ�����
//BYTE firstByte;  //首字节内容
//BYTE originalByte  //原始字节内容
}BREAKPOINT_TABLE_ENTRY, * PBREAKPOINT_TABLE_ENTRY;

typedef struct _BREAKPOINT_TABLE
{
    MY_LIST list_entry;  //ָ��BREAKPOINT_TABLE_ENTRY
    FAST_MUTEX Mutex;          //������
}BREAKPOINT_TABLE, * PBREAKPOINT_TABLE;

typedef struct _MDL_MAP
{
    PMDL mdl;
    PVOID MappedAddress;
    PVOID VirtualAddress;
}MDL_MAP, * PMDL_MAP;

//��������
//虚拟句柄表
typedef struct _VIRTUAL_HANDLE_TABLE_ENTRY
{
    size_t id;
    HANDLE handle;
    PVOID Object;
    HANDLE UniqueProcessId;
    LIST_ENTRY list_entry;
}VIRTUAL_HANDLE_TABLE_ENTRY, * PVIRTUAL_HANDLE_TABLE_ENTRY;

typedef struct _VIRTUAL_HANDLE_TABLE
{
    MY_LIST list_entry;    //ָ��DEBUG_PROCESS����
    FAST_MUTEX Mutex;          //������
}VIRTUAL_HANDLE_TABLE, * PVIRTUAL_HANDLE_TABLE;

typedef struct _DEBUGGER_TABLE_ENTRY
{
    LIST_ENTRY list_entry;
    DWORD dwPid;          //�������Ľ���id
    PTR64(TCHAR*) fileData;  //�ļ���Ϣ
    PTR64(TCHAR*) fileData2;  //�ļ���Ϣ
}DEBUGGER_TABLE_ENTRY, * PDEBUGGER_TABLE_ENTRY;


/*===============================================������===============================================*/
// /*===============================================类描述===============================================*/
class CDebuggerList : public CMyList
{
public:
    CDebuggerList()
    {

    }
    CDebuggerList(DWORD MaxCount) : CMyList(MaxCount)
    {

    }
    ~CDebuggerList()
    {
    }

private:

};

class CDebugObject : public CMyList
{
public:
    CDebugObject()
    {

    }
    CDebugObject(DWORD MaxCount) : CMyList(MaxCount)
    {

    }
    ~CDebugObject()
    {
    }

private:

};




extern PULONG_PTR PspProcessSequenceNumber;               //�������к�
extern _EPROCESS* DbgkTargetProcess;                      //����Ŀ��
extern _EPROCESS* ProtectProcessEp;                       //�������Ľ���
extern POBJECT_TYPE* DbgkDebugObjectType;                    //���Զ�������
extern POBJECT_TYPE          Hvm_DbgkDebugObjectType;                //�Խ��ĵ��Զ�������
extern PVOID                 g_DbgkOriginalCloseProcedure;
extern POBJECT_TYPE* ObTypeIndexTable;
extern FAST_MUTEX            DbgkpProcessDebugPortMutex;             //���̵��Զ˿ڻ�����
extern FAST_MUTEX            LongFlagsMutex;
extern PRKEVENT* DbgkErrorPortRegisteredEvent;
extern PDBGKP_ERROR_PORT     DbgkpErrorPort;
extern EX_PUSH_LOCK          DbgkpErrorPortLock;
extern _EPROCESS** DbgkpErrorProcess;
extern BOOLEAN               IsDbgk;
extern ULONG                 Ssdtbase;                               //SSDT��ַ
extern PVOID                 PspSystemDllBase;
extern PBOOLEAN              PsImageNotifyEnabled;
extern EPROCESS_QUOTA_BLOCK  PspDefaultQuotaBlock;
extern ULONG                 KeMaximumIncrement;
extern PHANDLE_TABLE         ObpKernelHandleTable;
extern PULONG                DbgkpMaxModuleMsgs;
extern PBOOLEAN              KdIgnoreUmExceptions;
extern PVOID                 KeUserExceptionDispatcher;              //ntdll.KiUserExceptionDispatcher
extern PULONG                KeFeatureBits;
extern PULONG_PTR            KeEnabledXStateFeatures;
extern UNICODE_STRING        PsNtDllPathName;
extern BOOLEAN               g_IsInitGlobalVariable;                 //��ʼ��ȫ�ֱ���
extern volatile LONG         g_DbgkDriverUnloading;                  //��������ж�أ��ܾ��µ�IOCTL
extern volatile LONG         g_DbgkActiveIoctlCount;                 //����ִ�е�IOCTL����
extern volatile LONG         g_DbgkActiveRuntimeCallCount;           // Active hook/debug callback count during unload drain
extern PEX_PUSH_LOCK         PspActiveProcessLock;
extern PLIST_ENTRY           PsActiveProcessHead;                    //������б�
extern LARGE_INTEGER* PspShortTime;
extern PEX_CALLBACK          PspCreateThreadNotifyRoutine;           //�߳�֪ͨ�ص�����������
extern PEX_CALLBACK          PspCreateProcessNotifyRoutine;          //����֪ͨ�ص�����������
extern PULONG                PspNotifyEnableMask;
extern PULONG                PerfGlobalGroupMask;
extern PVOID* PspSystemDlls;
extern PVOID                 g_obProcessHandle;
extern _EPROCESS* g_ProtectTargetProcess;                 //��Ҫ������Ŀ�����
extern PROTECTOBJ            g_ProtectFileObjList;                   //�������ļ������б�
extern PROTECTOBJ            g_ProtectWndObjList;                    //�����Ĵ��ڶ����б�
extern DEBUGGER_TABLE g_DebuggerList;                         //����������
extern DEBUG_PROCESS_TABLE g_DebugProcessList;                     //�����ԵĽ����б�
extern DEBUG_PROCESS_TABLE g_DebugProcessUnloadList;               // Debug process entries waiting for unload drain
extern _EPROCESS* g_SelfProcess;                          //�����Լ��Ľ���
extern BREAKPOINT_TABLE      g_BreakpointList;                       //�ϵ��б�
extern VIRTUAL_HANDLE_TABLE  g_VirtualHandleList;                    //�������б�
extern LONG                  g_TL_Game_pid;





/**************************** ����ָ�� ****************************/
// /**************************** 函数指针 ****************************/
typedef NTSTATUS(NTAPI* PFN_OBDUPLICATEOBJECT)(
    IN _EPROCESS* SourceProcess,
    IN HANDLE SourceHandle,
    IN _EPROCESS* TargetProcess OPTIONAL,
    OUT PHANDLE TargetHandle OPTIONAL,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG HandleAttributes,
    IN ULONG Options,
    IN KPROCESSOR_MODE PreviousMode);

typedef ULONG(__fastcall* PFN_KERESUMETHREAD)(_Inout_ PKTHREAD Thread);

typedef ULONG(__fastcall* PFN_KESUSPENDTHREAD)(_Inout_ PKTHREAD Thread);

typedef ULONG(__fastcall* PFN_KEFORCERESUMETHREAD)(_Inout_ PKTHREAD Thread);

typedef VOID(NTAPI* PFN_KEFREEZEALLTHREADS)(VOID);

typedef VOID(NTAPI* PFN_KETHAWALLTHREADS)(VOID);

typedef _ETHREAD* (NTAPI* PFN_PSGETNEXTPROCESSTHREAD)(IN PMY_EPROCESS Process, IN _ETHREAD* Thread);

typedef VOID(NTAPI* PFN_PSQUITNEXTPROCESSTHREAD)(IN _ETHREAD* Thread);

typedef NTSTATUS(NTAPI* PFN_MMGETFILENAMEFORADDRESS)(IN PVOID ProcessVa, OUT PUNICODE_STRING FileName);

typedef NTSTATUS(NTAPI* PFN_MMGETFILENAMEFORSECTION)(IN PVOID SectionObject, OUT POBJECT_NAME_INFORMATION* FileNameInfo);

typedef NTSTATUS(NTAPI* PFN_LPCREQUESTWAITREPLYPORTEX)(PVOID PortAddress,
    PPORT_MESSAGE RequestMessage,
    PPORT_MESSAGE ReplyMessage);

typedef VOID(NTAPI* PFN_KECONTEXTFROMKFRAMES)(IN PMY_KTRAP_FRAME TrapFrame,
    IN PMY_KEXCEPTION_FRAME ExceptionFrame,
    OUT PCONTEXT ContextFrame);

typedef VOID(NTAPI* PFN_KECONTEXTTOKFRAMES)(IN PMY_KTRAP_FRAME TrapFrame,
    IN PMY_KEXCEPTION_FRAME ExceptionFrame,
    IN PCONTEXT ContextFrame,
    IN ULONG ContextFlags,
    IN KPROCESSOR_MODE PreviousMode);

typedef BOOLEAN(NTAPI* PFN_KICHECKFORATLTHUNK)(IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT Context);

typedef VOID(NTAPI* PFN_KISEGSSTOTRAPFRAME)(IN PMY_KTRAP_FRAME TrapFrame,
    IN ULONG SegSs);

typedef VOID(NTAPI* PFN_KIESPTOTRAPFRAME)(IN PMY_KTRAP_FRAME TrapFrame,
    IN ULONG Esp);

typedef BOOLEAN(NTAPI* PFN_KIDEBUGROUTINE)(IN PMY_KTRAP_FRAME TrapFrame,
    IN PMY_KEXCEPTION_FRAME ExceptionFrame,
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord,
    IN KPROCESSOR_MODE PreviousMode,
    IN BOOLEAN SecondChance);

typedef BOOLEAN(NTAPI* PFN_RTLDISPATCHEXCEPTION)(IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord);

typedef VOID(NTAPI* PFN_KIDISPATCHEXCEPTION)(IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN KPROCESSOR_MODE PreviousMode,
    IN BOOLEAN FirstChance);

typedef VOID(__fastcall* PFN_PSCALLIMAGENOTIFYROUTINES)(IN PUNICODE_STRING FullImageName,
    IN PEPROCESS Process,
    IN PIMAGE_INFO_EX ImageInfoEx,
    IN PFILE_OBJECT FileObject);

typedef ULONG(NTAPI* PFN_OBGETPROCESSHANDLECOUNT)(IN PMY_EPROCESS Process);

typedef ULONG(NTAPI* PFN_PSGETPROCESSSESSIONID)(IN PMY_EPROCESS Process);

typedef ULONG(NTAPI* PFN_OBISLUIDDEVICEMAPSENABLED)(VOID);

typedef NTSTATUS(NTAPI* PFN_OBQUERYDEVICEMAPINFORMATION)(IN PMY_EPROCESS Process,
    IN PPROCESS_DEVICEMAP_INFORMATION DeviceMapInfo,
    IN ULONG Flags);

typedef NTSTATUS(__fastcall* PFN_SELOCATEPROCESSIMAGENAME)(IN PMY_EPROCESS Process, OUT PUNICODE_STRING* pImageFileName);

typedef VOID(NTAPI* PFN_MMGETIMAGEINFORMATION)(OUT PSECTION_IMAGE_INFORMATION ImageInformation);

typedef NTSTATUS(NTAPI* PFN_MMGETEXECUTEOPTIONS)(IN PULONG ExecuteOptions);

typedef PHANDLE_TABLE_ENTRY(NTAPI* PFN_EXMAPHANDLETOPOINTER)(IN PVOID HandleTable, IN HANDLE Handle);

typedef VOID(NTAPI* PFN_EXUNLOCKHANDLETABLEENTRY)(IN PVOID HandleTable, IN PHANDLE_TABLE_ENTRY HandleTableEntry);

typedef NTSTATUS(__fastcall* PFN_ZWALPCSENDWAITRECEIVEPORT)(HANDLE PortHandle,
    ULONG Flags,
    PPORT_MESSAGE SendMessage,
    PALPC_MESSAGE_ATTRIBUTES SendMessageAttributes,
    PPORT_MESSAGE ReceiveMessage,
    PSIZE_T BufferLength,
    PALPC_MESSAGE_ATTRIBUTES ReceiveMessageAttributes,
    PLARGE_INTEGER Timeout);

typedef NTSTATUS(__fastcall* PFN_MMGETSECTIONINFORMATION)(PSECTION SectionObject,
    ULONG Flags,
    PSECTION_IMAGE_INFORMATION ImageInfo);

typedef BOOLEAN(__fastcall* PFN_KIPREPROCESSFAULT)(IN OUT PEXCEPTION_RECORD ExceptionRecord,
    IN PMY_KTRAP_FRAME TrapFrame,
    IN OUT PCONTEXT ContextRecord,
    IN KPROCESSOR_MODE PreviousMode);

typedef NTSTATUS(__fastcall* PFN_DBGKPSTARTSYSTEMERRORHANDLER)();

typedef NTSTATUS(__fastcall* PFN_ALPCINITIALIZEMESSAGEATTRIBUTE)(ULONG AttributeFlags,
    PALPC_MESSAGE_ATTRIBUTES Buffer,
    ULONG BufferSize,
    PULONG RequiredBufferSize);

typedef PVOID(__fastcall* PFN_ALPCGETMESSAGEATTRIBUTE)(_In_ PALPC_MESSAGE_ATTRIBUTES Buffer,
    _In_ ULONG AttributeFlag);

typedef VOID(__fastcall* PFN_DBGKPREMOVEERRORPORT)(_In_ _ETHREAD* Thread,
    _In_ PDBGKP_ERROR_PORT DbgErrorPort);

typedef NTSTATUS(__fastcall* PFN_DBGKOPENPROCESSDEBUGPORT)(IN PMY_EPROCESS Process, IN KPROCESSOR_MODE PreviousMode, OUT HANDLE* pHandle);

typedef NTSTATUS(__fastcall* PFN_NTCREATEDEBUGOBJECT)(OUT PHANDLE DebugHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN ULONG Flags);

typedef NTSTATUS(__fastcall* PFN_NTDEBUGACTIVEPROCESS)(IN HANDLE ProcessHandle,
    IN HANDLE DebugHandle);

typedef NTSTATUS(__fastcall* PFN_NTWAITFORDEBUGEVENT)(IN HANDLE DebugHandle,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Timeout OPTIONAL,
    OUT PDBGUI_WAIT_STATE_CHANGE StateChange);

typedef NTSTATUS(__fastcall* PFN_RTLINITIALIZEEXTENDEDCONTEXT)(
    _In_    PVOID     pContext,
    _In_    ULONG     ContextFlags,
    _Out_   PCONTEXT_EX* ContextEx);

typedef NTSTATUS(__fastcall* PFN_RTLGETEXTENDEDCONTEXTLENGTH)(
    _In_    ULONG ContextFlags,
    _Out_   PULONG pContextLength);

typedef NTSTATUS(__fastcall* PFN_RTLPCOPYEXTENDEDCONTEXT)(_In_ BOOLEAN isCopy,
    _Inout_   PCONTEXT_EX DestContextEx,
    _In_      PCONTEXT_EX target_offset,
    _In_      ULONG ContextFlags,
    _In_      PCONTEXT_EX SrcContextEx,
    _In_      PCONTEXT_EX src_offset);

typedef NTSTATUS(__fastcall* PFN_NTSHUTDOWNSYSTEM)(SHUTDOWN_ACTION);

typedef NTSTATUS(__fastcall* PFN_OBCREATEOBJECTTYPE)(
    _In_ PUNICODE_STRING TypeName,
    _In_ POBJECT_TYPE_INITIALIZER ObjectTypeInitializer,
    _In_opt_ PSECURITY_DESCRIPTOR SecurityDescriptor,
    _Out_ POBJECT_TYPE* ObjectType);

typedef BOOLEAN(__fastcall* PFN_SEAUDITINGWITHTOKENFORSUBCATEGORY)(_In_ ULONG AuditCategory,
    _In_opt_ PACCESS_TOKEN Token);

typedef VOID(__fastcall* PFN_SEAUDITPROCESSCREATION)(_In_ PEPROCESS Process, _In_ __int64 a2);

typedef NTSTATUS(__fastcall* PFN_PSPGETJOBFROMSET)(_In_ PEJOB ParentJob,
    _In_ ULONG JobMemberLevel, _Out_ PEJOB* pJob);

typedef NTSTATUS(__fastcall* PFN_PSPBINDPROCESSSESSIONTOJOB)(_In_ PEJOB Job,
    _In_ PMY_EPROCESS Process);

typedef NTSTATUS(__fastcall* PFN_PSPADDPROCESSTOJOB)(_In_ PEJOB Job,
    _In_ PMY_EPROCESS Process);

typedef VOID(__fastcall* PFN_KICHECKFORKERNELAPCDELIVERY)();

typedef NTSTATUS(__fastcall* PFN_DBGKCOPYPROCESSDEBUGPORT)(_In_ PEPROCESS TargetProcess,
    _In_ PEPROCESS SourceProcess,
    _Inout_ PDEBUG_OBJECT DebugObject,
    _Out_ PBOOLEAN boCopyDebugPort);

typedef NTSTATUS(__fastcall* PFN_SECREATEACCESSSTATEEX)(_In_opt_ PETHREAD Thread,
    _In_ PEPROCESS Process,
    _Out_ PACCESS_STATE AccessState,
    _Out_ PVOID AuxData,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PGENERIC_MAPPING GenericMapping);

typedef NTSTATUS(__fastcall* PFN_OBINSERTOBJECTEX)(_In_ PVOID Object,
    _In_opt_ PACCESS_STATE PassedAccessState,
    _In_opt_ ACCESS_MASK DesiredAccess,
    _In_ ULONG ObjectPointerBias,
    _In_ ULONG a5,
    _Out_opt_ PVOID* NewObject,
    _Out_opt_ PHANDLE Handle);

typedef NTSTATUS(__fastcall* PFN_PSPVALIDATEJOBAFFINITYSTATE)(_In_ PEJOB Job,
    _In_ PEPROCESS Process);

typedef NTSTATUS(__fastcall* PFN_OBCHECKREFTRACEPROCESS)(_In_ PEPROCESS Process);

typedef VOID(__fastcall* PFN_SEDELETEACCESSSTATE)(_In_ PACCESS_STATE AccessState);

typedef NTSTATUS(__fastcall* PFN_PSPINSERTTHREAD)(_In_ _ETHREAD* Thread,
    _In_ PMY_EPROCESS CurrentProcess,
    _In_ PVOID InitialTeb,
    _In_ ULONG ProcessFlags,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PUCHAR Flag,
    _In_ PVOID CreateProcessContext,
    _In_ PVOID AccessState,
    _In_ PGROUP_AFFINITY GroupAffinity,
    _Out_ HANDLE* ProcessHandle,
    _In_ PCLIENT_ID ClientID);

typedef NTSTATUS(__fastcall* PFN_NTDEBUGCONTINUE)(
    IN HANDLE DebugObjectHandle,
    IN PCLIENT_ID ClientId,
    IN NTSTATUS ContinueStatus);

typedef VOID(__fastcall* PFN_DBGKMAPVIEWOFSECTION)(IN _EPROCESS* Process,
    IN PVOID SectionObject,
    IN PVOID BaseAddress,
    IN ULONG SectionOffset,
    IN ULONG_PTR ViewSize);

typedef VOID(__fastcall* PFN_DBGKUNMAPVIEWOFSECTION)(IN PMY_EPROCESS Process, IN PVOID BaseAddress);

typedef BOOLEAN(__fastcall* PFN_DBGKPSUPPRESSDBGMSG)(IN PTEB Teb);

typedef NTSTATUS(__fastcall* PFN_PSREFERENCEPROCESSFILEPOINTER)(IN PEPROCESS Process, OUT PVOID* OutFileObject);

typedef PVOID(__fastcall* PFN_OBFASTREFERENCEOBJECT)(IN PEX_FAST_REF FastRef);

typedef PVOID(__fastcall* PFN_OBFASTREFERENCEOBJECTLOCKED)(PEX_FAST_REF FastRef);

typedef PVOID(__fastcall* PFN_MIREFERENCECONTROLAREAFILE)(PCONTROL_AREA ControlArea);

typedef VOID(__fastcall* PFN_OBFASTDEREFERENCEOBJECT)(IN PEX_FAST_REF FastRef, IN PVOID Object, IN ULONG Tag);

typedef VOID(__fastcall* PFN_DBGKSENDSYSTEMDLLMESSAGES)(_In_opt_ _ETHREAD* Thread,
    _In_opt_ PDEBUG_OBJECT DebugObject,
    _In_ PDBGKM_APIMSG ApiMsg);

typedef VOID(__fastcall* PFN_DBGKCREATETHREAD)(IN _ETHREAD* Thread);

typedef VOID(__fastcall* PFN_DBGKEXITTHREAD)(IN NTSTATUS ExitStatus);

typedef VOID(__fastcall* PFN_DBGKEXITPROCESS)(IN NTSTATUS ExitStatus);

typedef HWND(__fastcall* PFN_NTUSERFINDWINDOWEX)(IN HWND hwndParent,
    IN HWND hwndChild,
    IN PUNICODE_STRING pstrClassName,
    IN PUNICODE_STRING pstrWindowName);

typedef BOOLEAN(__fastcall* PFN_DBGKFORWARDEXCEPTION)(IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOLEAN IsUseDebugPort,
    IN BOOLEAN SecondChance);

typedef VOID(__fastcall* PFN_PSSYNCHRONIZEWITHTHREADINSERTION)(IN _ETHREAD* Thread, IN _ETHREAD* CurrentThread);

typedef NTSTATUS(__fastcall* PFN_NTGETCONTEXTTHREAD)(_In_ HANDLE hThread, _Inout_ PCONTEXT ThreadContext);

typedef NTSTATUS(__fastcall* PFN_NTSETCONTEXTTHREAD)(
    __in HANDLE ThreadHandle,
    __in PCONTEXT ThreadContext  /*�˲������û����ṩ�Ļ�����*/
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef NTSTATUS(__fastcall* PFN_NTREADVIRTUALMEMORY)(_In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer, //输出缓冲区
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead);

typedef NTSTATUS(__fastcall* PFN_NTWRITEVIRTUALMEMORY)(_In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _In_ PVOID Buffer, //当前进程自己的缓冲区
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesWritten);

typedef VOID(__fastcall* PFN_ETWTRACETHREAD)(_In_ _ETHREAD* Thread,
    _In_ PVOID InitialTeb,
    _In_ BOOLEAN bFlag);

typedef PEX_CALLBACK_ROUTINE_BLOCK(__fastcall* PFN_EXREFERENCECALLBACKBLOCK)(_Inout_ PEX_CALLBACK CallBack);

typedef PEX_CALLBACK_FUNCTION(__fastcall* PFN_EXGETCALLBACKBLOCKROUTINE)(_In_ PEX_CALLBACK_ROUTINE_BLOCK CallBackBlock);

typedef VOID(__fastcall* PFN_EXDEREFERENCECALLBACKBLOCK)(_Inout_ PEX_CALLBACK CallBack, _In_ PEX_CALLBACK_ROUTINE_BLOCK CallBackBlock);

typedef BOOLEAN(__fastcall* PFN_OBREFERENCEOBJECTSAFE)(_In_ PVOID Object);

typedef VOID(__fastcall* PFN_PSPCATCHCRITICALBREAK)(_In_ PCHAR Msg, _In_ PVOID Object, _In_ PUCHAR ImageFileName);

typedef NTSTATUS(__fastcall* PFN_LPCREQUESTPORT)(_In_ PVOID PortAddress, _In_ PPORT_MESSAGE RequestMessage);

typedef NTSTATUS(__fastcall* PFN_ALPCPSENDMESSAGE)(_Inout_ PVOID a1, _In_ PLPC_CLIENT_DIED_MSG a2, _In_ __int64 a3, _In_ BOOLEAN a4);

typedef NTSTATUS(__fastcall* PFN_PSPW32THREADCALLOUT)(_In_ _ETHREAD* Thread, _In_ PSW32THREADCALLOUTTYPE CalloutType);

typedef NTSTATUS(__fastcall* PFN_PSPW32PROCESSCALLOUT)(_In_ PMY_EPROCESS Process, _In_ BOOLEAN Initialize);

typedef VOID(__fastcall* PFN_IOCANCELTHREADIO)(_In_ _ETHREAD* Thread);

typedef VOID(__fastcall* PFN_EXTIMERRUNDOWN)();

typedef VOID(__fastcall* PFN_CMNOTIFYRUNDOWN)(_In_ _ETHREAD* Thread);

typedef VOID(__fastcall* PFN_KERUNDOWNTHREAD)();

typedef NTSTATUS(__fastcall* PFN_PSPUMSUNINITTHREAD)(_In_ _ETHREAD* Thread);

typedef PLIST_ENTRY(__fastcall* PFN_KERUNDOWNQUEUEEX)(_Inout_ PKQUEUE Queue, _In_ BOOLEAN a2);

typedef NTSTATUS(__fastcall* PFN_RTLFREEUSERSTACK)(_In_ PVOID DeallocationStack);

typedef VOID(__fastcall* PFN_MMDELETETEB)(_In_ PMY_EPROCESS TargetProcess, _In_ PVOID TebBase);

typedef VOID(__fastcall* PFN_PSPEXITPROCESS)(_In_ BOOLEAN LastThreadExit, _In_ PMY_EPROCESS Process);

typedef VOID(__fastcall* PFN_SEAUDITPROCESSEXIT)(_In_ PMY_EPROCESS Process, _In_ NTSTATUS ExitStatus);

typedef VOID(__fastcall* PFN_OBKILLPROCESS)(_In_ PMY_EPROCESS Process);

typedef VOID(__fastcall* PFN_PSPEXITPROCESSFROMJOB)(_In_ PEJOB Job, _In_ PMY_EPROCESS Process);

typedef PLIST_ENTRY(__fastcall* PFN_KEFLUSHQUEUEAPC)(_Inout_ PKTHREAD Thread, _In_ KPROCESSOR_MODE ProcessorMode);

typedef VOID(__fastcall* PFN_MMCLEANPROCESSADDRESSSPACE)(_In_ PMY_EPROCESS Process);

typedef VOID(__fastcall* PFN_LPCEXITPROCESS)(_In_ PMY_EPROCESS Process);

typedef VOID(__fastcall* PFN_PSPLEGONOTIFYROUTINE)(_In_ PKTHREAD Thread);

typedef LONG(__fastcall* PFN_KESETPROCESS)(_In_ PKPROCESS Process);

typedef VOID(__fastcall* PFN_KETERMINATETHREAD)(_In_ _ETHREAD* Thread);

typedef VOID(__fastcall* PFN_PSPEXITTHREAD)(_In_ NTSTATUS ExitStatus);

typedef _EPROCESS* (__fastcall* PFN_PSGETNEXTPROCESS)(_In_ _EPROCESS* Process);

typedef NTSTATUS(__fastcall* PFN_PSTERMINATEPROCESS)(_In_ _EPROCESS* Process, _In_ NTSTATUS Status);

typedef VOID(__fastcall* PFN_DBGKPCLOSEOBJECT)(
    IN _EPROCESS* Process,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG_PTR SystemHandleCount
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef VOID(__fastcall* PFN_KESTARTTHREAD)(_In_ _ETHREAD* Thread, _In_opt_ PGROUP_AFFINITY GroupAffinity, _In_opt_ PULONG IdealProcessor);

typedef LONG_PTR(__fastcall* PFN_OBREFERENCEOBJECTEX)(_In_ PVOID Object, _In_ ULONG Count);

typedef VOID(__fastcall* PFN_PSPINITIALIZECPUQUOTAAPC)(_Out_ PVOID Apc, _In_ PKTHREAD Thread);

typedef NTSTATUS(__fastcall* PFN_PSPADJUSTTHREADSECURITYDESCRIPTOR)(_In_ _ETHREAD* Thread);

typedef VOID(__fastcall* PFN_ETWTRACEPROCESS)(_In_ PMY_EPROCESS Process, _In_ ULONG a2);

typedef NTSTATUS(__fastcall* PFN_PSPCREATEOBJECTHANDLE)(_In_ PVOID Object, _Inout_ PVOID AccessState, _In_ POBJECT_TYPE ObjectType);

typedef NTSTATUS(__fastcall* PFN_PSPTERMINATETHREADBYPOINTER)(_In_ _ETHREAD* Thread, _In_ NTSTATUS ExitStatus, _In_ BOOLEAN DirectTerminate);

typedef VOID(__fastcall* PFN_KEREADYTHREAD)(_Inout_ PKTHREAD Thread);

typedef VOID(__fastcall* PFN_MMDELETEKERNELSTACK)(_In_ PVOID PointerKernelStack, _In_ BOOLEAN LargeStack);

typedef PVOID(__fastcall* PFN_PSCAPTUREEXCEPTIONPORT)(_In_ _EPROCESS* Process);

typedef NTSTATUS(__fastcall* PFN_KERAISEUSEREXCEPTION)(_In_ NTSTATUS ExceptionCode);

typedef NTSTATUS(__fastcall* PFN_PSPCREATETHREAD)(HANDLE* ThreadHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ProcessHandle,
    _EPROCESS* arg_Process,
    __int64 CreateProcessContext,
    __int64 a7,
    __int64 a8,
    __int64 a9,
    unsigned int a10,
    PKSTART_ROUTINE StartRoutine,
    __int64 a12,
    __int64 a13);

typedef NTSTATUS(__fastcall* PFN_NTPROTECTVIRTUALMEMORY)(_In_ HANDLE ProcessHandle,
    _Inout_ PVOID* BaseAddress,
    _Inout_ PSIZE_T RegionSize,
    _In_ WIN32_PROTECTION_MASK flNewProtect,
    _Out_ PULONG flOldProtect);

typedef BOOLEAN(__fastcall* PFN_PSFREEZEPROCESS)(_EPROCESS* a1, BOOLEAN a2);

typedef void(__fastcall* PFN_PSTHAWPROCESS)(_EPROCESS* a1, __int64 a2);

typedef BOOLEAN(__fastcall* PFN_DBGKPSUSPENDPROCESS)(_EPROCESS* Process);

typedef NTSTATUS(__fastcall* PFN_DBGKPQUEUEMESSAGE)(IN _EPROCESS* Process,
    IN _ETHREAD* Thread,
    IN PDBGKM_APIMSG Message,
    IN ULONG Flags,
    IN PDEBUG_OBJECT TargetObject OPTIONAL);

typedef HANDLE(__fastcall* PFN_DBGKPSECTIONTOFILEHANDLE)(IN PVOID Section);

typedef NTSTATUS(__fastcall* PFN_DBGKPSENDAPIMESSAGE)(_EPROCESS* Process, BOOLEAN SuspendProcess, PDBGKM_APIMSG ApiMsg);

typedef NTSTATUS(__fastcall* PFN_DBGKPSENDAPIMESSAGELPC)(IN OUT PDBGKM_APIMSG Message,
    IN PVOID Port,
    IN BOOLEAN SuspendProcess);

typedef NTSTATUS(__fastcall* PFN_DBGKPSENDERRORMESSAGE)(PEXCEPTION_RECORD ExceptionRecord, ULONG Falge, PDBGKM_APIMSG ApiMsg);

typedef void(__fastcall* PFN_PSSETPROCESSFAULTINFORMATION)(_EPROCESS* Process, int* a2);

typedef NTSTATUS(__fastcall* PFN_DBGKPPOSTFAKEPROCESSCREATEMESSAGES)(IN _EPROCESS* Process,
    IN PDEBUG_OBJECT DebugObject,
    OUT _ETHREAD** LastThread);

typedef NTSTATUS(__fastcall* PFN_DBGKPSETPROCESSDEBUGOBJECT)(IN _EPROCESS* Process,
    IN PDEBUG_OBJECT DebugObject,
    IN NTSTATUS MsgStatus,
    IN _ETHREAD* LastThread);

typedef VOID(__fastcall* PFN_DBGKPWAKETARGET)(IN PDEBUG_EVENT DebugEvent);

typedef NTSTATUS(__fastcall* PFN_DBGKCLEARPROCESSDEBUGOBJECT)(
    IN _EPROCESS* Process,
    IN PDEBUG_OBJECT SourceDebugObject);

typedef NTSTATUS(__fastcall* PFN_NTREMOVEPROCESSDEBUG)(
    IN HANDLE ProcessHandle,
    IN HANDLE DebugObjectHandle);

typedef VOID(__fastcall* PFN_DBGKPMARKPROCESSPEB)(IN _EPROCESS* Process);

typedef NTSTATUS(__fastcall* PFN_DBGKPPOSTFAKETHREADMESSAGES)(IN _EPROCESS* Process,
    IN PDEBUG_OBJECT DebugObject,
    IN _ETHREAD* StartThread,
    OUT _ETHREAD** FirstThread,
    OUT _ETHREAD** LastThread);

typedef VOID(__fastcall* PFN_KISTACKATTACHPROCESS)(_KPROCESS* Process, BOOLEAN a2, _KAPC_STATE* ApcState);

typedef VOID(__fastcall* PFN_KIUNSTACKDETACHPROCESS)(_KAPC_STATE* a1, int a2);

typedef NTSTATUS(__fastcall* PFN_NTCREATETHREADEX)(unsigned __int64 a1,
    int a2,
    __int64 a3,
    ULONG_PTR a4,
    __int64 a5,
    __int64 _StartAddress,
    unsigned int a7,
    __int64 a8,
    __int64 a9,
    __int64 a10,
    __int64 a11);

typedef NTSTATUS(__fastcall* PFN_NTOPENPROCESS)(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID ClientId);

typedef VOID(__fastcall* PFN_DBGKPCONVERTKERNELTOUSERSTATECHANGE)(OUT PDBGUI_WAIT_STATE_CHANGE WaitStateChange,
    IN PDEBUG_EVENT DebugEvent);

typedef VOID(__fastcall* PFN_DBGKPOPENHANDLES)(IN PDBGUI_WAIT_STATE_CHANGE WaitStateChange,
    IN _EPROCESS* Process,
    IN _ETHREAD* Thread);

typedef VOID(__fastcall* PFN_KECOPYEXCEPTIONRECORD)(PEXCEPTION_RECORD dest, PEXCEPTION_RECORD source);

typedef NTSTATUS(__fastcall* PFN_OBREFERENCEOBJECTBYHANDLEWITHTAG)(HANDLE Handle,
    int a2,
    struct _OBJECT_TYPE* a3,
    char a4,
    int a5,
    PVOID* Object,
    __int64 a7);

typedef NTSTATUS(__fastcall* PFN_OBREFERENCEOBJECTBYHANDLE)(HANDLE Handle,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    PVOID* Object,
    POBJECT_HANDLE_INFORMATION HandleInformation);

typedef LONG_PTR(__fastcall* PFN_OBFDEREFERENCEOBJECT)(
    _In_ PVOID Object
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef LONG_PTR(__fastcall* PFN_OBFDEREFERENCEOBJECTWITHTAG)(
    _In_ PVOID Object,
    _In_ ULONG Tag
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef VOID(__fastcall* PFN_KEENTERCRITICALREGIONTHREAD)(_KTHREAD* Thread);

typedef VOID(__fastcall* PFN_KELEAVECRITICALREGIONTHREAD)(_KTHREAD* Thread);

typedef NTSTATUS(__fastcall* PFN_MMCOPYVIRTUALMEMORY)(_EPROCESS* FromProcess,
    PVOID FromAddress,
    _EPROCESS* ToProcess,
    PVOID ToAddress,
    size_t BufferSize,
    KPROCESSOR_MODE PreviousMode,
    size_t* NumberOfBytesCopied);

typedef VOID(__fastcall* PFN_PSPCREATEUSERCONTEXT)(_CONTEXT* context,
    char a2,
    PVOID ApcFunc,
    PVOID pUserfunc,
    __int64 a5);

typedef VOID(__fastcall* PFN_PSPCALLTHREADNOTIFYROUTINES)(_ETHREAD* Thread,
    BOOLEAN Create,
    BOOLEAN a3);

typedef NTSTATUS(__fastcall* PFN_PSPALLOCATETHREAD)(_EPROCESS* Process,
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

typedef NTSTATUS(__fastcall* PFN_OBPREFERENCEOBJECTBYHANDLEWITHTAG)(HANDLE Handle,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    ULONG Tag,
    PVOID* Object,
    POBJECT_HANDLE_INFORMATION HandleInformation,
    __int64 a8);

typedef PMMVAD(__fastcall* PFN_MIOBTAINREFERENCEDVADEX)(PVOID StartingAddress, char a2, PNTSTATUS status);

typedef NTSTATUS(__fastcall* PFN_MMPROTECTVIRTUALMEMORY)(_EPROCESS* sourceProcess,
    _EPROCESS* TargetProcess,
    PVOID* BaseAddress,
    PSIZE_T RegionSize,
    WIN32_PROTECTION_MASK NewProtectWin32,
    PWIN32_PROTECTION_MASK LastProtect);

typedef NTSTATUS(__fastcall* PFN_ZWGETCONTEXTTHREAD)(_In_ HANDLE hThread, _Inout_ PCONTEXT ThreadContext);

typedef NTSTATUS(__fastcall* PFN_PSPGETCONTEXTTHREADINTERNAL)(_In_ _ETHREAD* Thread,
    _Inout_ PCONTEXT ThreadContext,
    _In_ KPROCESSOR_MODE PreviousMode,
    _In_ KPROCESSOR_MODE PreviousMode2,
    _In_ char a5);

typedef VOID(__fastcall* PFN_KESTACKATTACHPROCESS)(
    _Inout_ PRKPROCESS PROCESS,
    _Out_ PRKAPC_STATE ApcState
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef VOID(__fastcall* PFN_KISTACKATTACHPROCESS)(_KPROCESS* Process, BOOLEAN a2, _KAPC_STATE* ApcState);

typedef NTSTATUS(__fastcall* PFN_NTSETINFORMATIONDEBUGOBJECT)(
    IN HANDLE DebugObjectHandle,
    IN DEBUGOBJECTINFOCLASS DebugObjectInformationClass,
    IN PVOID DebugInformation,
    IN ULONG DebugInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef NTSTATUS(__fastcall* PFN_NTTERMINATEPROCESS)(
    __in_opt HANDLE ProcessHandle,
    __in NTSTATUS ExitStatus
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef NTSTATUS(__fastcall* PFN_NTSUSPENDTHREAD)(
    __in HANDLE ThreadHandle,
    __out_opt PULONG PreviousSuspendCount
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef NTSTATUS(__fastcall* PFN_NTRESUMETHREAD)(
    __in HANDLE ThreadHandle,
    __out_opt PULONG PreviousSuspendCount
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef NTSTATUS(__fastcall* PFN_NTQUERYINFORMATIONTHREAD)(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _Out_ PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength
    // __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
    );

typedef PWND(__fastcall* PFN_VALIDATEHWND)(
    _In_ HWND hwnd);

typedef HWND(__fastcall* PFN_NTUSERWINDOWFROMPOINT)(POINT Point);

typedef PEPROCESS(__fastcall* PFN_PSGETCURRENTPROCESSBYTHREAD)(IN PETHREAD Thread);

typedef PPS_SYSTEM_DLL_INFO(__fastcall* PFN_PSQUERYSYSTEMDLLINFO)(IN int idx);

typedef SYSTEM_DLL_TYPE(__fastcall* PFN_PSWOW64GETPROCESSNTDLLTYPE)(IN PEPROCESS Process);

typedef PSECTION(__fastcall* PFN_PSPREFERENCESYSTEMDLL)(IN PPSP_SYSTEM_DLL SystemDll);

typedef PCONTROL_AREA(__fastcall* PFN_MISECTIONCONTROLAREA)(IN PSECTION Section);

typedef NTSTATUS(__fastcall* PFN_DBGKPPOSTMODULEMESSAGES)(
    IN PEPROCESS Process,
    IN PETHREAD Thread,
    IN PDEBUG_OBJECT DebugObject);

typedef PFILE_OBJECT(__fastcall* PFN_MIREFERENCECONTROLAREAFILEWITHTAG)(IN PCONTROL_AREA ControlArea, IN ULONG Tag);

typedef NTSTATUS(__fastcall* PFN_PSPINSERTPROCESS)(IN PEPROCESS TargetProcess,
    IN PEPROCESS ParentProcess,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG Flags,
    IN HANDLE DebugObjectHandle,
    IN ULONG CrossThreadFlags,
    IN int a7,
    IN PPSP_OBJECT_CREATION_STATE ObjectCreationState);

typedef NTSTATUS(__fastcall* PFN_PSPIMPLICITASSIGNPROCESSTOJOB)(IN PEJOB Job,
    IN PEPROCESS Process,
    IN ULONG Flags);

typedef NTSTATUS(__fastcall* PFN_PSPINHERITSYSCALLPROVIDER)(IN PEPROCESS TargetProcess, IN PEPROCESS ParentProcess);

typedef VOID(__fastcall* PFN_PSPUNLOCKPROCESSLISTEXCLUSIVE)(IN PKTHREAD Thread);

typedef VOID(__fastcall* PFN_SEPDELETEACCESSSTATE)(IN PACCESS_STATE PassedAccessState);

typedef VOID(__fastcall* PFN_ADDRESS)();







extern PFN_OBDUPLICATEOBJECT ObDuplicateObject;  //���ƶ���
extern PFN_KERESUMETHREAD KeResumeThread;
extern PFN_KESUSPENDTHREAD KeSuspendThread;
extern PFN_KEFORCERESUMETHREAD KeForceResumeThread;
extern PFN_KEFREEZEALLTHREADS KeFreezeAllThreads;
extern PFN_KETHAWALLTHREADS KeThawAllThreads;
extern PFN_PSGETNEXTPROCESSTHREAD PsGetNextProcessThread; //��ȡ���̵���һ���߳�
extern PFN_PSQUITNEXTPROCESSTHREAD PsQuitNextProcessThread; //�̶߳���������
extern PFN_MMGETFILENAMEFORADDRESS MmGetFileNameForAddress; //ͨ����ַ��ȡ������Ϣ
extern PFN_MMGETFILENAMEFORSECTION MmGetFileNameForSection; //ͨ���ڶ����ȡ������Ϣ
extern PFN_LPCREQUESTWAITREPLYPORTEX LpcRequestWaitReplyPortEx; //����˿�
extern PFN_KECONTEXTFROMKFRAMES KeContextFromKframes;
extern PFN_KECONTEXTTOKFRAMES KeContextToKframes;
extern PFN_KICHECKFORATLTHUNK KiCheckForAtlThunk;
extern PFN_KISEGSSTOTRAPFRAME KiSegSsToTrapFrame;
extern PFN_KIESPTOTRAPFRAME KiEspToTrapFrame;
extern PFN_KIDEBUGROUTINE KiDebugRoutine;
extern PFN_RTLDISPATCHEXCEPTION RtlDispatchException;
extern PFN_PSCALLIMAGENOTIFYROUTINES PsCallImageNotifyRoutines;  //����ӳ��ص�����
extern PFN_OBGETPROCESSHANDLECOUNT ObGetProcessHandleCount;
extern PFN_PSGETPROCESSSESSIONID PsGetProcessSessionId;
extern PFN_OBISLUIDDEVICEMAPSENABLED ObIsLUIDDeviceMapsEnabled;
extern PFN_OBQUERYDEVICEMAPINFORMATION ObQueryDeviceMapInformation;
extern PFN_MMGETIMAGEINFORMATION MmGetImageInformation;
extern PFN_MMGETEXECUTEOPTIONS MmGetExecuteOptions;
extern PFN_EXMAPHANDLETOPOINTER ExMapHandleToPointer;
extern PFN_EXUNLOCKHANDLETABLEENTRY ExUnlockHandleTableEntry;
extern PFN_ZWALPCSENDWAITRECEIVEPORT ZwAlpcSendWaitReceivePort;
extern PFN_MMGETSECTIONINFORMATION MmGetSectionInformation;
extern PFN_KIPREPROCESSFAULT KiPreprocessFault;
extern PFN_DBGKPSTARTSYSTEMERRORHANDLER DbgkpStartSystemErrorHandler;
extern PFN_ALPCINITIALIZEMESSAGEATTRIBUTE AlpcInitializeMessageAttribute;
extern PFN_ALPCGETMESSAGEATTRIBUTE AlpcGetMessageAttribute;
extern PFN_DBGKPREMOVEERRORPORT DbgkpRemoveErrorPort;
extern PFN_DBGKOPENPROCESSDEBUGPORT Sys_DbgkOpenProcessDebugPort;
extern PFN_NTCREATEDEBUGOBJECT Sys_NtCreateDebugObject;
extern PFN_NTDEBUGACTIVEPROCESS Sys_NtDebugActiveProcess;
extern PFN_NTWAITFORDEBUGEVENT Sys_NtWaitForDebugEvent;
extern PFN_KIDISPATCHEXCEPTION Sys_KiDispatchException;
extern PFN_KIDISPATCHEXCEPTION Original_KiDispatchException;
extern PFN_RTLINITIALIZEEXTENDEDCONTEXT RtlInitializeExtendedContext;
extern PFN_RTLGETEXTENDEDCONTEXTLENGTH RtlGetExtendedContextLength;
extern PFN_RTLPCOPYEXTENDEDCONTEXT RtlpCopyExtendedContext;
extern PFN_NTSHUTDOWNSYSTEM Sys_NtShutdownSystem;
extern PFN_OBCREATEOBJECTTYPE ObCreateObjectType;
extern PFN_SEAUDITINGWITHTOKENFORSUBCATEGORY SeAuditingWithTokenForSubcategory;
extern PFN_SEAUDITPROCESSCREATION SeAuditProcessCreation;
extern PFN_PSPGETJOBFROMSET PspGetJobFromSet;
extern PFN_PSPBINDPROCESSSESSIONTOJOB PspBindProcessSessionToJob;
extern PFN_PSPADDPROCESSTOJOB PspAddProcessToJob;
extern PFN_KICHECKFORKERNELAPCDELIVERY KiCheckForKernelApcDelivery;
extern PFN_DBGKCOPYPROCESSDEBUGPORT DbgkCopyProcessDebugPort;
extern PFN_SECREATEACCESSSTATEEX SeCreateAccessStateEx;
extern PFN_OBINSERTOBJECTEX ObInsertObjectEx;
extern PFN_PSPVALIDATEJOBAFFINITYSTATE PspValidateJobAffinityState;
extern PFN_OBCHECKREFTRACEPROCESS ObCheckRefTraceProcess;
extern PFN_SEDELETEACCESSSTATE SeDeleteAccessState;
extern PFN_PSPINSERTPROCESS Sys_PspInsertProcess;
extern PFN_PSPINSERTTHREAD Sys_PspInsertThread;
extern PFN_NTDEBUGCONTINUE Sys_NtDebugContinue;
extern PFN_DBGKMAPVIEWOFSECTION Sys_DbgkMapViewOfSection;
extern PFN_DBGKUNMAPVIEWOFSECTION Sys_DbgkUnMapViewOfSection;
extern PFN_DBGKPSUPPRESSDBGMSG DbgkpSuppressDbgMsg;
extern PFN_PSREFERENCEPROCESSFILEPOINTER PsReferenceProcessFilePointer;
extern PFN_OBFASTREFERENCEOBJECT ObFastReferenceObject;
extern PFN_OBFASTREFERENCEOBJECTLOCKED ObFastReferenceObjectLocked;
extern PFN_MIREFERENCECONTROLAREAFILE MiReferenceControlAreaFile;
extern PFN_OBFASTDEREFERENCEOBJECT ObFastDereferenceObject;
extern PFN_DBGKSENDSYSTEMDLLMESSAGES DbgkSendSystemDllMessages;
extern PFN_DBGKCREATETHREAD Sys_DbgkCreateThread;
extern PFN_DBGKCREATETHREAD Original_DbgkCreateThread;
extern PFN_DBGKEXITTHREAD Sys_DbgkExitThread;
extern PFN_DBGKEXITPROCESS Sys_DbgkExitProcess;
extern PFN_NTUSERFINDWINDOWEX Original_NtUserFindWindowEx;
extern PFN_NTUSERFINDWINDOWEX Sys_NtUserFindWindowEx;
extern PFN_DBGKFORWARDEXCEPTION Sys_DbgkForwardException;
extern PFN_PSSYNCHRONIZEWITHTHREADINSERTION PsSynchronizeWithThreadInsertion;
extern PFN_NTGETCONTEXTTHREAD Sys_NtGetContextThread;
extern PFN_NTGETCONTEXTTHREAD Original_NtGetContextThread;
extern PFN_NTSETCONTEXTTHREAD Sys_NtSetContextThread;
extern PFN_NTSETCONTEXTTHREAD Original_NtSetContextThread;
extern PFN_NTREADVIRTUALMEMORY Sys_NtReadVirtualMemory;
extern PFN_NTREADVIRTUALMEMORY Original_NtReadVirtualMemory;
extern PFN_NTWRITEVIRTUALMEMORY Sys_NtWriteVirtualMemory;
extern PFN_NTWRITEVIRTUALMEMORY Original_NtWriteVirtualMemory;
extern PFN_ETWTRACETHREAD EtwTraceThread;
extern PFN_EXREFERENCECALLBACKBLOCK ExReferenceCallBackBlock;
extern PFN_EXGETCALLBACKBLOCKROUTINE ExGetCallBackBlockRoutine;
extern PFN_EXDEREFERENCECALLBACKBLOCK ExDereferenceCallBackBlock;
//extern PFN_OBREFERENCEOBJECTSAFE ObReferenceObjectSafe;
extern PFN_PSPCATCHCRITICALBREAK PspCatchCriticalBreak;
extern PFN_LPCREQUESTPORT LpcRequestPort;
extern PFN_ALPCPSENDMESSAGE AlpcpSendMessage;
extern PFN_PSPW32THREADCALLOUT* PspW32ThreadCallout;
extern PFN_PSPW32PROCESSCALLOUT* PspW32ProcessCallout;
extern PFN_IOCANCELTHREADIO IoCancelThreadIo;
extern PFN_EXTIMERRUNDOWN ExTimerRundown;
extern PFN_CMNOTIFYRUNDOWN CmNotifyRunDown;
extern PFN_KERUNDOWNTHREAD KeRundownThread;
extern PFN_PSPUMSUNINITTHREAD PspUmsUnInitThread;
extern PFN_KERUNDOWNQUEUEEX KeRundownQueueEx;
extern PFN_RTLFREEUSERSTACK RtlFreeUserStack;
extern PFN_MMDELETETEB MmDeleteTeb;
extern PFN_PSPEXITPROCESS PspExitProcess;
extern PFN_SEAUDITPROCESSEXIT SeAuditProcessExit;
extern PFN_OBKILLPROCESS ObKillProcess;
extern PFN_PSPEXITPROCESSFROMJOB PspExitProcessFromJob;
extern PFN_KEFLUSHQUEUEAPC KeFlushQueueApc;
extern PFN_MMCLEANPROCESSADDRESSSPACE MmCleanProcessAddressSpace;
extern PFN_LPCEXITPROCESS LpcExitProcess;
extern PFN_PSPLEGONOTIFYROUTINE* PspLegoNotifyRoutine;
extern PFN_KESETPROCESS KeSetProcess;
extern PFN_KETERMINATETHREAD KeTerminateThread;
extern PFN_PSPEXITTHREAD Sys_PspExitThread;
extern PFN_PSPEXITTHREAD Original_PspExitThread;
extern PFN_PSGETNEXTPROCESS PsGetNextProcess;
extern PFN_PSTERMINATEPROCESS PsTerminateProcess;
extern PFN_DBGKPCLOSEOBJECT Sys_DbgkpCloseObject;
extern PFN_KESTARTTHREAD KeStartThread;
extern PFN_OBREFERENCEOBJECTEX ObReferenceObjectEx;
extern PFN_PSPINITIALIZECPUQUOTAAPC PspInitializeCpuQuotaApc;
extern PFN_PSPADJUSTTHREADSECURITYDESCRIPTOR PspAdjustThreadSecurityDescriptor;
extern PFN_ETWTRACEPROCESS EtwTraceProcess;
extern PFN_PSPCREATEOBJECTHANDLE PspCreateObjectHandle;
extern PFN_PSPTERMINATETHREADBYPOINTER PspTerminateThreadByPointer;
extern PFN_KEREADYTHREAD KeReadyThread;
extern PFN_MMDELETEKERNELSTACK MmDeleteKernelStack;
extern PFN_PSCAPTUREEXCEPTIONPORT PsCaptureExceptionPort;
extern PFN_KERAISEUSEREXCEPTION KeRaiseUserException;
extern PFN_PSPCREATETHREAD Sys_PspCreateThread;
extern PFN_PSPCREATETHREAD Original_PspCreateThread;
extern PFN_NTCREATETHREADEX Sys_NtCreateThreadEx;
extern PFN_NTCREATETHREADEX Original_NtCreateThreadEx;
extern PFN_NTPROTECTVIRTUALMEMORY ZwProtectVirtualMemory;
extern PFN_NTPROTECTVIRTUALMEMORY Sys_NtProtectVirtualMemory;
extern PFN_NTPROTECTVIRTUALMEMORY Original_NtProtectVirtualMemory;
extern PFN_PSFREEZEPROCESS PsFreezeProcess;
extern PFN_PSTHAWPROCESS PsThawProcess;
extern PFN_DBGKPSUSPENDPROCESS DbgkpSuspendProcess;
extern PFN_DBGKPQUEUEMESSAGE Sys_DbgkpQueueMessage;
extern PFN_DBGKPSECTIONTOFILEHANDLE DbgkpSectionToFileHandle;
extern PFN_DBGKPSENDAPIMESSAGE DbgkpSendApiMessage;
extern PFN_DBGKPSENDAPIMESSAGELPC DbgkpSendApiMessageLpc;
extern PFN_DBGKPSENDERRORMESSAGE DbgkpSendErrorMessage;
extern PFN_PSSETPROCESSFAULTINFORMATION PsSetProcessFaultInformation;
extern PFN_DBGKPPOSTFAKEPROCESSCREATEMESSAGES DbgkpPostFakeProcessCreateMessages;
//extern PFN_DBGKPSETPROCESSDEBUGOBJECT DbgkpSetProcessDebugObject;
extern PFN_DBGKPWAKETARGET DbgkpWakeTarget;
extern PFN_DBGKCLEARPROCESSDEBUGOBJECT DbgkClearProcessDebugObject;
extern PFN_NTREMOVEPROCESSDEBUG Sys_NtRemoveProcessDebug;
extern PFN_DBGKPMARKPROCESSPEB DbgkpMarkProcessPeb;
extern PFN_DBGKPPOSTFAKETHREADMESSAGES DbgkpPostFakeThreadMessages;
extern PFN_KISTACKATTACHPROCESS KiStackAttachProcess;
extern PFN_KIUNSTACKDETACHPROCESS KiUnstackDetachProcess;
extern PFN_NTOPENPROCESS Sys_NtOpenProcess;
extern PFN_NTOPENPROCESS Original_NtOpenProcess;
extern PFN_DBGKPCONVERTKERNELTOUSERSTATECHANGE DbgkpConvertKernelToUserStateChange;
extern PFN_DBGKPOPENHANDLES DbgkpOpenHandles;
extern PFN_KECOPYEXCEPTIONRECORD KeCopyExceptionRecord;
extern PFN_OBREFERENCEOBJECTBYHANDLEWITHTAG Sys_ObReferenceObjectByHandleWithTag;
extern PFN_OBREFERENCEOBJECTBYHANDLEWITHTAG Original_ObReferenceObjectByHandleWithTag;
extern PFN_OBREFERENCEOBJECTBYHANDLE Sys_ObReferenceObjectByHandle;
extern PFN_OBREFERENCEOBJECTBYHANDLE Original_ObReferenceObjectByHandle;
extern PFN_OBFDEREFERENCEOBJECTWITHTAG Sys_ObfDereferenceObjectWithTag;
extern PFN_OBFDEREFERENCEOBJECTWITHTAG Original_ObfDereferenceObjectWithTag;
extern PFN_OBFDEREFERENCEOBJECT Sys_ObfDereferenceObject;
extern PFN_OBFDEREFERENCEOBJECT Original_ObfDereferenceObject;
extern PFN_KEENTERCRITICALREGIONTHREAD KeEnterCriticalRegionThread;
extern PFN_KELEAVECRITICALREGIONTHREAD KeLeaveCriticalRegionThread;
extern PFN_MMCOPYVIRTUALMEMORY Sys_MmCopyVirtualMemory;
extern PFN_MMCOPYVIRTUALMEMORY Original_MmCopyVirtualMemory;
extern PFN_PSPCREATEUSERCONTEXT Sys_PspCreateUserContext;
extern PFN_PSPCREATEUSERCONTEXT Original_PspCreateUserContext;
extern PFN_PSPCALLTHREADNOTIFYROUTINES Sys_PspCallThreadNotifyRoutines;
extern PFN_PSPCALLTHREADNOTIFYROUTINES Original_PspCallThreadNotifyRoutines;
extern PFN_PSPALLOCATETHREAD Sys_PspAllocateThread;
extern PFN_PSPALLOCATETHREAD Original_PspAllocateThread;
extern PFN_OBPREFERENCEOBJECTBYHANDLEWITHTAG Sys_ObpReferenceObjectByHandleWithTag;
extern PFN_OBPREFERENCEOBJECTBYHANDLEWITHTAG Original_ObpReferenceObjectByHandleWithTag;
extern PFN_MIOBTAINREFERENCEDVADEX Sys_MiObtainReferencedVadEx;
extern PFN_MIOBTAINREFERENCEDVADEX Original_MiObtainReferencedVadEx;
extern PFN_MMPROTECTVIRTUALMEMORY Sys_MmProtectVirtualMemory;
extern PFN_MMPROTECTVIRTUALMEMORY Original_MmProtectVirtualMemory;
extern PFN_ZWGETCONTEXTTHREAD ZwGetContextThread;
extern PFN_PSPGETCONTEXTTHREADINTERNAL PspGetContextThreadInternal;
extern PFN_KESTACKATTACHPROCESS Sys_KeStackAttachProcess;
extern PFN_KESTACKATTACHPROCESS Original_KeStackAttachProcess;
extern PFN_KISTACKATTACHPROCESS Sys_KiStackAttachProcess;
extern PFN_KISTACKATTACHPROCESS Original_KiStackAttachProcess;
extern PFN_NTSETINFORMATIONDEBUGOBJECT Sys_NtSetInformationDebugObject;
extern PFN_NTTERMINATEPROCESS Sys_NtTerminateProcess;
extern PFN_NTTERMINATEPROCESS Original_NtTerminateProcess;
extern PFN_NTSUSPENDTHREAD Sys_NtSuspendThread;
extern PFN_NTSUSPENDTHREAD Original_NtSuspendThread;
extern PFN_NTRESUMETHREAD Sys_NtResumeThread;
extern PFN_NTRESUMETHREAD Original_NtResumeThread;
extern PFN_NTQUERYINFORMATIONTHREAD Sys_NtQueryInformationThread;
extern PFN_NTQUERYINFORMATIONTHREAD Original_NtQueryInformationThread;
extern PFN_VALIDATEHWND Sys_ValidateHwnd;
extern PFN_VALIDATEHWND Original_ValidateHwnd;
extern PFN_NTUSERWINDOWFROMPOINT Sys_NtUserWindowFromPoint;
extern PFN_NTUSERWINDOWFROMPOINT Original_NtUserWindowFromPoint;
extern PFN_PSGETCURRENTPROCESSBYTHREAD PsGetCurrentProcessByThread;
extern PFN_PSQUERYSYSTEMDLLINFO PsQuerySystemDllInfo;
extern PFN_PSWOW64GETPROCESSNTDLLTYPE PsWow64GetProcessNtdllType;
extern PFN_PSPREFERENCESYSTEMDLL PspReferenceSystemDll;
extern PFN_MISECTIONCONTROLAREA MiSectionControlArea;
extern PFN_DBGKPPOSTMODULEMESSAGES DbgkpPostModuleMessages;
extern PFN_MIREFERENCECONTROLAREAFILEWITHTAG MiReferenceControlAreaFileWithTag;
extern PFN_PSPIMPLICITASSIGNPROCESSTOJOB PspImplicitAssignProcessToJob;
extern PFN_PSPINHERITSYSCALLPROVIDER PspInheritSyscallProvider;
extern PFN_PSPUNLOCKPROCESSLISTEXCLUSIVE PspUnlockProcessListExclusive;
extern PFN_SEPDELETEACCESSSTATE SepDeleteAccessState;





//ȫ�ֱ���
//全局变量
extern PVOID PspLoaderInitRoutine;
extern unsigned __int64 game_cr3;


namespace eprocess_offset
{
    extern size_t Pcb;
    extern size_t DebugPort;
    extern size_t ImageFileName;
    extern size_t WoW64Process;
    extern size_t RundownProtect;
    extern size_t ExitTime;
    extern size_t Flags;
    extern size_t SectionBaseAddress;
    extern size_t SectionObject;
    extern size_t Machine;
    extern size_t ObjectTable;
    extern size_t UniqueProcessId;
    extern size_t Job;
    extern size_t ActiveProcessLinks;
    extern size_t SequenceNumber;
    extern size_t ProcessLock;
}

namespace kprocess_offset
{
    extern size_t DirectoryTableBase;
}

namespace ethread_offset
{
    extern size_t Tcb;
    extern size_t CrossThreadFlags;
    extern size_t Cid;
    extern size_t RundownProtect;
    extern size_t Win32StartAddress;
    extern size_t SameThreadPassiveFlags;
}

namespace kthread_offset
{
    extern size_t ApcState;
    extern size_t PreviousMode;
    extern size_t Teb;
    extern size_t Process;
    extern size_t KernelApcDisable;
    extern size_t MiscFlags;
    extern size_t TrapFrame;
    extern size_t SuspendCount;
    extern size_t SpecialApcDisable;
}

namespace kapc_state_offset
{
    extern size_t Process;
}

namespace image_nt_headers64_offset
{
    extern size_t Signature;
    extern size_t FileHeader;
    extern size_t OptionalHeader;
}

namespace image_file_header_offset
{
    extern size_t PointerToSymbolTable;
    extern size_t NumberOfSymbols;
}

namespace ewow64process_offset
{
    extern size_t Machine;
}

namespace mmvad_offset
{
    extern size_t Core; //_MMVAD_SHORT Core;
}

namespace mmvad_short_offset
{
    extern size_t LongFlags;
}

namespace handle_table_offset
{
    extern size_t UniqueProcessId;
}

namespace object_type_offset
{
    extern size_t TypeInfo;
}

namespace object_type_initializer_offset
{
    extern size_t GenericMapping;
}



//VOID
//FORCEINLINE
//KeEnterCriticalRegionThread(IN PKTHREAD Thread)
//{
//    size_t ptr_KernelApcDisable = (size_t)Thread + kthread_offset::KernelApcDisable;
//    *(short*)ptr_KernelApcDisable -= 1;
//    //Thread->KernelApcDisable -= 1;
//}

//VOID
//FORCEINLINE
//KeLeaveCriticalRegionThread(IN PKTHREAD Thread)
//{
//    size_t ptr_KernelApcDisable = (size_t)Thread + kthread_offset::KernelApcDisable;
//
//    if ((*(short*)ptr_KernelApcDisable += 1) == 0)
//    {
//        size_t ptr_ApcState = (size_t)Thread + kthread_offset::ApcState;
//        ptr_ApcState +
//    }
//
//    if ((Thread->KernelApcDisable += 1) == 0)
//    {
//        if (Thread->ApcState.ApcListHead[KernelMode].Flink != &Thread->ApcState.ApcListHead[KernelMode])
//        {
//            if (Thread->SpecialApcDisable == 0)
//            {
//                KiCheckForKernelApcDelivery();
//            }
//        }
//    }
//}

//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//pass:
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
KeEnterGuardedRegionThread(IN PKTHREAD Thread)
{
    //���´�����������Thread->SpecialApcDisable -= 1;
    //以下代码描述的是Thread->SpecialApcDisable -= 1;
    size_t ptr_SpecialApcDisable = (size_t)Thread + kthread_offset::SpecialApcDisable;
    *(short*)ptr_SpecialApcDisable -= 1;
}

//pass:
//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
KeLeaveGuardedRegionThread(IN PKTHREAD Thread)
{
    //以下代码描述的是Thread->SpecialApcDisable -= 1;
    size_t ptr_SpecialApcDisable = (size_t)Thread + kthread_offset::SpecialApcDisable;
    if ((*(short*)ptr_SpecialApcDisable += 1) == 0)
    {
        PKAPC_STATE ApcState = (PKAPC_STATE)((size_t)Thread + kthread_offset::ApcState);
        if (ApcState->ApcListHead[KernelMode].Flink != &ApcState->ApcListHead[KernelMode])
        {
            KiCheckForKernelApcDelivery();
        }
    }
    //if ((Thread->SpecialApcDisable += 1) == 0)
    //{
    //    if (Thread->ApcState.ApcListHead[KernelMode].Flink != &Thread->ApcState.ApcListHead[KernelMode])
    //    {
    //        KiCheckForKernelApcDelivery();
    //    }
    //}
}

//VOID
//FORCEINLINE
//ExAcquirePushLockExclusive(IN PEX_PUSH_LOCK PushLock)
//{
//    if (InterlockedBitTestAndSet64((LONG64*)&PushLock->Value, EX_PUSH_LOCK_LOCK_V))
//    {
//        ExfAcquirePushLockExclusive((PULONG_PTR)PushLock);
//    }
//}

//VOID
//FORCEINLINE
//ExReleasePushLockExclusive(IN PEX_PUSH_LOCK PushLock)
//{
//    EX_PUSH_LOCK OldValue;
//
//    OldValue.Value = InterlockedExchangeAdd64((PLONG64)&PushLock->Value, -(LONG64)EX_PUSH_LOCK_LOCK);
//
//    if (!OldValue.Waiting || OldValue.Waking) {
//        return;
//    }
//
//    ExfTryToWakePushLock((PULONG_PTR)PushLock);
//}





//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//pass:
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
PspLockProcessShared(IN PMY_EPROCESS Process,  //���̹�����
    IN PMY_ETHREAD CurrentThread)
{
    KeEnterCriticalRegionThread(&CurrentThread->Tcb);
    ExAcquirePushLockShared((PULONG_PTR)&Process->ProcessLock);
}

//pass:
//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
PspUnlockProcessShared(IN PMY_EPROCESS Process,
    IN PMY_ETHREAD CurrentThread)
{
    ExReleasePushLockShared((PULONG_PTR)&Process->ProcessLock);
    KeLeaveCriticalRegionThread(&CurrentThread->Tcb);
}

//pass:
//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
PspLockDbgkpErrorPortShared(IN _ETHREAD* CurrentThread)
{
    KeEnterCriticalRegionThread(&CurrentThread->Tcb);
    ExAcquirePushLockShared((PULONG_PTR)&DbgkpErrorPortLock);
}

//pass:
//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
PspUnlockDbgkpErrorPortShared(IN PMY_ETHREAD CurrentThread)
{
    ExReleasePushLockShared((PULONG_PTR)&DbgkpErrorPortLock);
    KeLeaveCriticalRegionThread(&CurrentThread->Tcb);
}

//pass:
//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
PspLockProcessExclusive(IN PMY_EPROCESS Process,   //���̶�ռ��
    IN PMY_ETHREAD CurrentThread)
{
    KeEnterCriticalRegionThread(&CurrentThread->Tcb);
    ExAcquirePushLockExclusive((PULONG_PTR)&Process->ProcessLock);
}

//pass:
//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
PspUnlockProcessExclusive(IN PEPROCESS Process,
    IN PETHREAD CurrentThread)
{
    size_t ptr_ProcessLock = (size_t)Process + eprocess_offset::ProcessLock;
    size_t kthread_base = (size_t)CurrentThread + ethread_offset::Tcb;
    ExReleasePushLockExclusive((PULONG_PTR)ptr_ProcessLock);
    KeLeaveCriticalRegionThread((PKTHREAD)kthread_base);
}

//pass:
//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
PspLockProcessListExclusive(IN PETHREAD CurrentThread)  //��ס������б�
{
    size_t kthread_base = (size_t)CurrentThread + ethread_offset::Tcb;
    KeEnterGuardedRegionThread((PKTHREAD)kthread_base);
    ExAcquirePushLockExclusive((PULONG_PTR)PspActiveProcessLock);
}

//pass:
//VOID
//NewKeStackAttachProcess(
//    _Inout_ PRKPROCESS PROCESS,
//    _Out_ PRKAPC_STATE ApcState
//)
//{
//    DbgkRuntimeCallGuard runtimeGuard;
//    if (DbgkIsDriverRuntimeUnloading())
//    {
//        Original_KeStackAttachProcess(PROCESS, ApcState);
//        return;
//    }
//
//    if (IsDebugger((PEPROCESS)PROCESS))
//        //获取当前调用进程
//        WCHAR SubStr[256] = { 0 };
//        UNICODE_STRING ImageFileName, PassImage;
//        NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
//        if (NT_SUCCESS(Status))
//        {
//            RtlInitUnicodeString(&ImageFileName, SubStr);
//            for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
//            {
//                RtlInitUnicodeString(&PassImage, PassProcessList[i]);
//                if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
//                {
//                    //发现是白名单进程就放行
//                    goto pass;
//                }
//            }
//        }
//        ApcState->Process = (PKPROCESS)1;
//    Original_KeStackAttachProcess(PROCESS, ApcState);
//}
VOID
FORCEINLINE
PspUnlockProcessList(IN PETHREAD CurrentThread)
{
    size_t kthread_base = (size_t)CurrentThread + ethread_offset::Tcb;
    ExReleasePushLockExclusive((PULONG_PTR)PspActiveProcessLock);
    KeLeaveGuardedRegionThread((PKTHREAD)kthread_base);
}

//VOID
//FORCEINLINE
//PspLockSystemDllShared(IN _ETHREAD* CurrentThread,   //ϵͳdll������
//    IN PSP_SYSTEM_DLL SystemDll)
//{
//    KeEnterCriticalRegionThread(&CurrentThread->Tcb);
//    ExAcquirePushLockShared((PULONG_PTR)&SystemDll.DllLock);
//}

//VOID
//FORCEINLINE
//PspUnlockSystemDllShared(IN PMY_ETHREAD CurrentThread,
//    IN PSP_SYSTEM_DLL SystemDll)
//{
//    ExReleasePushLockShared((PULONG_PTR)&SystemDll.DllLock);
//    KeLeaveCriticalRegionThread(&CurrentThread->Tcb);
//}

#endif // !_GLOBALS_H

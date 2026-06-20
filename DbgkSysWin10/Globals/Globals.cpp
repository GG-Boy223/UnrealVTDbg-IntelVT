#include "Driver.h"
#include "../ntos/inc/mmtypes.h"
#include "../ntos/inc/ntdbg.h"
#include "../ntos/inc/ketypes.h"
#include "../ntos/inc/extypes.h"
#include "../ntos/inc/ntosdef.h"
#include "../ntos/inc/amd64.h"
#include "../ntos/inc/mi.h"
#include "../ntos/inc/pstypes.h"
#include "../ntos/inc/obtypes.h"
#include "../ntos/inc/peb_teb.h"
#include "../List/MyList.h"
#include "../ntos/inc/ntlpcapi.h"
#include "../ntos/inc/psp.h"
#include "Globals.h"
#include "../DbgkApi/DbgkApi.h"


PULONG_PTR PspProcessSequenceNumber;               //进程序列号
_EPROCESS* DbgkTargetProcess;                      //调试目标
_EPROCESS* ProtectProcessEp;                       //被保护的进程
POBJECT_TYPE* DbgkDebugObjectType;                    //调试对象类型
POBJECT_TYPE          Hvm_DbgkDebugObjectType;                //自建的调试对象类型
PVOID                 g_DbgkOriginalCloseProcedure;
POBJECT_TYPE* ObTypeIndexTable;
FAST_MUTEX            DbgkpProcessDebugPortMutex;             //进程调试端口互斥锁
FAST_MUTEX            LongFlagsMutex;
PRKEVENT* DbgkErrorPortRegisteredEvent;
PDBGKP_ERROR_PORT     DbgkpErrorPort;
EX_PUSH_LOCK          DbgkpErrorPortLock;
_EPROCESS** DbgkpErrorProcess;
BOOLEAN               IsDbgk;                                 //是否调试
ULONG                 Ssdtbase;                               //SSDT基址
PVOID                 PspSystemDllBase;
PBOOLEAN              PsImageNotifyEnabled;
EPROCESS_QUOTA_BLOCK  PspDefaultQuotaBlock;
ULONG                 KeMaximumIncrement;
PHANDLE_TABLE         ObpKernelHandleTable;
PULONG                DbgkpMaxModuleMsgs;


ULONG                 DbgkpTraceLevel;

BOOLEAN               KdEnteredDebugger;
BOOLEAN               KdBreakAfterSymbolLoad;
BOOLEAN               KdPitchDebugger;
PBOOLEAN              KdIgnoreUmExceptions;

//
// KeUserExceptionDispatcher - This is the address of the user mode exception
//      dispatch code. This address is looked up in NTDLL.DLL during system
//      initialization.
//
PVOID                 KeUserExceptionDispatcher;  //ntdll.KiUserExceptionDispatcher

//
// KeFeatureBits - Architectural specific processor features present
// on all processors.
//

PULONG                KeFeatureBits;
PULONG_PTR            KeEnabledXStateFeatures;

UNICODE_STRING PsNtDllPathName = RTL_CONSTANT_STRING(L"\\SystemRoot\\System32\\ntdll.dll");

BOOLEAN               g_IsInitGlobalVariable;                 //初始化全局变量
volatile LONG         g_DbgkDriverUnloading;                  //驱动正在卸载，拒绝新的IOCTL
volatile LONG         g_DbgkActiveIoctlCount;                 //正在执行的IOCTL数量
volatile LONG         g_DbgkActiveRuntimeCallCount;           // Active hook/debug callback count during unload drain
PEX_PUSH_LOCK         PspActiveProcessLock;                   //活动进程列表锁
PLIST_ENTRY           PsActiveProcessHead;                    //活动进程列表
LARGE_INTEGER* PspShortTime;
PEX_CALLBACK          PspCreateThreadNotifyRoutine;           //线程通知回调函数的数组
PEX_CALLBACK          PspCreateProcessNotifyRoutine;          //进程通知回调函数的数组
PULONG                PspNotifyEnableMask;
PULONG                PerfGlobalGroupMask;
PVOID* PspSystemDlls;
PVOID                 g_obProcessHandle;
_EPROCESS* g_ProtectTargetProcess;                 //需要保护的目标进程
PROTECTOBJ            g_ProtectFileObjList;                   //保护的文件对象列表
PROTECTOBJ            g_ProtectWndObjList;                    //保护的窗口对象列表
DEBUGGER_TABLE        g_DebuggerList;                         //调试器对象
DEBUG_PROCESS_TABLE   g_DebugProcessList;                     //被调试的进程列表
DEBUG_PROCESS_TABLE   g_DebugProcessUnloadList;               // Debug process entries waiting for unload drain
_EPROCESS*            g_SelfProcess;                          //我们自己的进程
BREAKPOINT_TABLE      g_BreakpointList;                       //断点列表
VIRTUAL_HANDLE_TABLE  g_VirtualHandleList;                    //虚拟句柄列表
LONG                  g_TL_Game_pid;








/**************************** 函数指针 ****************************/

PFN_OBDUPLICATEOBJECT ObDuplicateObject;  //复制对象
PFN_KERESUMETHREAD KeResumeThread;
PFN_KESUSPENDTHREAD KeSuspendThread;
PFN_KEFORCERESUMETHREAD KeForceResumeThread;
PFN_KEFREEZEALLTHREADS KeFreezeAllThreads;
PFN_KETHAWALLTHREADS KeThawAllThreads;
PFN_PSGETNEXTPROCESSTHREAD PsGetNextProcessThread; //获取进程的下一个线程
PFN_PSQUITNEXTPROCESSTHREAD PsQuitNextProcessThread; //线程对象解除引用
PFN_MMGETFILENAMEFORADDRESS MmGetFileNameForAddress; //通过地址获取名称信息
PFN_MMGETFILENAMEFORSECTION MmGetFileNameForSection; //通过节对象获取名称信息
PFN_LPCREQUESTWAITREPLYPORTEX LpcRequestWaitReplyPortEx; //请求端口
PFN_KECONTEXTFROMKFRAMES KeContextFromKframes;
PFN_KECONTEXTTOKFRAMES KeContextToKframes;
PFN_KICHECKFORATLTHUNK KiCheckForAtlThunk;
PFN_KISEGSSTOTRAPFRAME KiSegSsToTrapFrame;
PFN_KIESPTOTRAPFRAME KiEspToTrapFrame;
PFN_KIDEBUGROUTINE KiDebugRoutine;
PFN_RTLDISPATCHEXCEPTION RtlDispatchException;
PFN_PSCALLIMAGENOTIFYROUTINES PsCallImageNotifyRoutines;  //调用映像回调例程
PFN_OBGETPROCESSHANDLECOUNT ObGetProcessHandleCount;
PFN_PSGETPROCESSSESSIONID PsGetProcessSessionId;
PFN_OBISLUIDDEVICEMAPSENABLED ObIsLUIDDeviceMapsEnabled;
PFN_OBQUERYDEVICEMAPINFORMATION ObQueryDeviceMapInformation;
PFN_MMGETIMAGEINFORMATION MmGetImageInformation;
PFN_MMGETEXECUTEOPTIONS MmGetExecuteOptions;
PFN_EXMAPHANDLETOPOINTER ExMapHandleToPointer;
PFN_EXUNLOCKHANDLETABLEENTRY ExUnlockHandleTableEntry;
PFN_ZWALPCSENDWAITRECEIVEPORT ZwAlpcSendWaitReceivePort;
PFN_MMGETSECTIONINFORMATION MmGetSectionInformation;
PFN_KIPREPROCESSFAULT KiPreprocessFault;
PFN_DBGKPSTARTSYSTEMERRORHANDLER DbgkpStartSystemErrorHandler;
PFN_ALPCINITIALIZEMESSAGEATTRIBUTE AlpcInitializeMessageAttribute;
PFN_ALPCGETMESSAGEATTRIBUTE AlpcGetMessageAttribute;
PFN_DBGKPREMOVEERRORPORT DbgkpRemoveErrorPort;
PFN_DBGKOPENPROCESSDEBUGPORT Sys_DbgkOpenProcessDebugPort;
PFN_NTCREATEDEBUGOBJECT Sys_NtCreateDebugObject;
PFN_NTDEBUGACTIVEPROCESS Sys_NtDebugActiveProcess;
PFN_NTWAITFORDEBUGEVENT Sys_NtWaitForDebugEvent;
PFN_KIDISPATCHEXCEPTION Sys_KiDispatchException;
PFN_KIDISPATCHEXCEPTION Original_KiDispatchException;
PFN_RTLINITIALIZEEXTENDEDCONTEXT RtlInitializeExtendedContext;
PFN_RTLGETEXTENDEDCONTEXTLENGTH RtlGetExtendedContextLength;
PFN_RTLPCOPYEXTENDEDCONTEXT RtlpCopyExtendedContext;
PFN_NTSHUTDOWNSYSTEM Sys_NtShutdownSystem;
PFN_OBCREATEOBJECTTYPE ObCreateObjectType;
PFN_SEAUDITINGWITHTOKENFORSUBCATEGORY SeAuditingWithTokenForSubcategory;
PFN_SEAUDITPROCESSCREATION SeAuditProcessCreation;
PFN_PSPGETJOBFROMSET PspGetJobFromSet;
PFN_PSPBINDPROCESSSESSIONTOJOB PspBindProcessSessionToJob;
PFN_PSPADDPROCESSTOJOB PspAddProcessToJob;
PFN_KICHECKFORKERNELAPCDELIVERY KiCheckForKernelApcDelivery;
PFN_DBGKCOPYPROCESSDEBUGPORT DbgkCopyProcessDebugPort;
PFN_SECREATEACCESSSTATEEX SeCreateAccessStateEx;
PFN_OBINSERTOBJECTEX ObInsertObjectEx;
PFN_PSPVALIDATEJOBAFFINITYSTATE PspValidateJobAffinityState;
PFN_OBCHECKREFTRACEPROCESS ObCheckRefTraceProcess;
PFN_SEDELETEACCESSSTATE SeDeleteAccessState;
PFN_PSPINSERTPROCESS Sys_PspInsertProcess;
PFN_PSPINSERTTHREAD Sys_PspInsertThread;
PFN_NTDEBUGCONTINUE Sys_NtDebugContinue;
PFN_DBGKMAPVIEWOFSECTION Sys_DbgkMapViewOfSection;
PFN_DBGKUNMAPVIEWOFSECTION Sys_DbgkUnMapViewOfSection;
PFN_DBGKPSUPPRESSDBGMSG DbgkpSuppressDbgMsg;
PFN_PSREFERENCEPROCESSFILEPOINTER PsReferenceProcessFilePointer;
PFN_OBFASTREFERENCEOBJECT ObFastReferenceObject;
PFN_OBFASTREFERENCEOBJECTLOCKED ObFastReferenceObjectLocked;
PFN_MIREFERENCECONTROLAREAFILE MiReferenceControlAreaFile;
PFN_OBFASTDEREFERENCEOBJECT ObFastDereferenceObject;
PFN_DBGKSENDSYSTEMDLLMESSAGES DbgkSendSystemDllMessages;
PFN_DBGKCREATETHREAD Sys_DbgkCreateThread;
PFN_DBGKCREATETHREAD Original_DbgkCreateThread;
PFN_DBGKEXITTHREAD Sys_DbgkExitThread;
PFN_DBGKEXITPROCESS Sys_DbgkExitProcess;
PFN_NTUSERFINDWINDOWEX Original_NtUserFindWindowEx;
PFN_NTUSERFINDWINDOWEX Sys_NtUserFindWindowEx;
PFN_DBGKFORWARDEXCEPTION Sys_DbgkForwardException;
PFN_PSSYNCHRONIZEWITHTHREADINSERTION PsSynchronizeWithThreadInsertion;
PFN_NTGETCONTEXTTHREAD Sys_NtGetContextThread;
PFN_NTGETCONTEXTTHREAD Original_NtGetContextThread;
PFN_NTSETCONTEXTTHREAD Sys_NtSetContextThread;
PFN_NTSETCONTEXTTHREAD Original_NtSetContextThread;
PFN_NTREADVIRTUALMEMORY Sys_NtReadVirtualMemory;
PFN_NTREADVIRTUALMEMORY Original_NtReadVirtualMemory;
PFN_NTWRITEVIRTUALMEMORY Sys_NtWriteVirtualMemory;
PFN_NTWRITEVIRTUALMEMORY Original_NtWriteVirtualMemory;
PFN_ETWTRACETHREAD EtwTraceThread;
PFN_EXREFERENCECALLBACKBLOCK ExReferenceCallBackBlock;
PFN_EXGETCALLBACKBLOCKROUTINE ExGetCallBackBlockRoutine;
PFN_EXDEREFERENCECALLBACKBLOCK ExDereferenceCallBackBlock;
//PFN_OBREFERENCEOBJECTSAFE ObReferenceObjectSafe;
PFN_PSPCATCHCRITICALBREAK PspCatchCriticalBreak;
PFN_LPCREQUESTPORT LpcRequestPort;
PFN_ALPCPSENDMESSAGE AlpcpSendMessage;
PFN_PSPW32THREADCALLOUT* PspW32ThreadCallout;
PFN_PSPW32PROCESSCALLOUT* PspW32ProcessCallout;
PFN_IOCANCELTHREADIO IoCancelThreadIo;
PFN_EXTIMERRUNDOWN ExTimerRundown;
PFN_CMNOTIFYRUNDOWN CmNotifyRunDown;
PFN_KERUNDOWNTHREAD KeRundownThread;
PFN_PSPUMSUNINITTHREAD PspUmsUnInitThread;
PFN_KERUNDOWNQUEUEEX KeRundownQueueEx;
PFN_RTLFREEUSERSTACK RtlFreeUserStack;
PFN_MMDELETETEB MmDeleteTeb;
PFN_PSPEXITPROCESS PspExitProcess;
PFN_SEAUDITPROCESSEXIT SeAuditProcessExit;
PFN_OBKILLPROCESS ObKillProcess;
PFN_PSPEXITPROCESSFROMJOB PspExitProcessFromJob;
PFN_KEFLUSHQUEUEAPC KeFlushQueueApc;
PFN_MMCLEANPROCESSADDRESSSPACE MmCleanProcessAddressSpace;
PFN_LPCEXITPROCESS LpcExitProcess;
PFN_PSPLEGONOTIFYROUTINE* PspLegoNotifyRoutine;
PFN_KESETPROCESS KeSetProcess;
PFN_KETERMINATETHREAD KeTerminateThread;
PFN_PSPEXITTHREAD Sys_PspExitThread;
PFN_PSPEXITTHREAD Original_PspExitThread;
PFN_PSGETNEXTPROCESS PsGetNextProcess;
PFN_PSTERMINATEPROCESS PsTerminateProcess;
PFN_DBGKPCLOSEOBJECT Sys_DbgkpCloseObject;
PFN_KESTARTTHREAD KeStartThread;
PFN_OBREFERENCEOBJECTEX ObReferenceObjectEx;
PFN_PSPINITIALIZECPUQUOTAAPC PspInitializeCpuQuotaApc;
PFN_PSPADJUSTTHREADSECURITYDESCRIPTOR PspAdjustThreadSecurityDescriptor;
PFN_ETWTRACEPROCESS EtwTraceProcess;
PFN_PSPCREATEOBJECTHANDLE PspCreateObjectHandle;
PFN_PSPTERMINATETHREADBYPOINTER PspTerminateThreadByPointer;
PFN_KEREADYTHREAD KeReadyThread;
PFN_MMDELETEKERNELSTACK MmDeleteKernelStack;
PFN_PSCAPTUREEXCEPTIONPORT PsCaptureExceptionPort;
PFN_KERAISEUSEREXCEPTION KeRaiseUserException;
PFN_PSPCREATETHREAD Sys_PspCreateThread;
PFN_PSPCREATETHREAD Original_PspCreateThread;
PFN_NTCREATETHREADEX Sys_NtCreateThreadEx;
PFN_NTCREATETHREADEX Original_NtCreateThreadEx;
PFN_NTPROTECTVIRTUALMEMORY ZwProtectVirtualMemory;
PFN_NTPROTECTVIRTUALMEMORY Sys_NtProtectVirtualMemory;
PFN_NTPROTECTVIRTUALMEMORY Original_NtProtectVirtualMemory;
PFN_PSFREEZEPROCESS PsFreezeProcess;
PFN_PSTHAWPROCESS PsThawProcess;
PFN_DBGKPSUSPENDPROCESS DbgkpSuspendProcess;
PFN_DBGKPQUEUEMESSAGE Sys_DbgkpQueueMessage;
PFN_DBGKPSECTIONTOFILEHANDLE DbgkpSectionToFileHandle;
PFN_DBGKPSENDAPIMESSAGE DbgkpSendApiMessage;
PFN_DBGKPSENDAPIMESSAGELPC DbgkpSendApiMessageLpc;
PFN_DBGKPSENDERRORMESSAGE DbgkpSendErrorMessage;
PFN_PSSETPROCESSFAULTINFORMATION PsSetProcessFaultInformation;
PFN_DBGKPPOSTFAKEPROCESSCREATEMESSAGES DbgkpPostFakeProcessCreateMessages;
//PFN_DBGKPSETPROCESSDEBUGOBJECT DbgkpSetProcessDebugObject;
PFN_DBGKPWAKETARGET DbgkpWakeTarget;
PFN_DBGKCLEARPROCESSDEBUGOBJECT DbgkClearProcessDebugObject;
PFN_NTREMOVEPROCESSDEBUG Sys_NtRemoveProcessDebug;
PFN_DBGKPMARKPROCESSPEB DbgkpMarkProcessPeb;
PFN_DBGKPPOSTFAKETHREADMESSAGES DbgkpPostFakeThreadMessages;
PFN_KISTACKATTACHPROCESS KiStackAttachProcess;
PFN_KIUNSTACKDETACHPROCESS KiUnstackDetachProcess;
PFN_NTOPENPROCESS Sys_NtOpenProcess;
PFN_NTOPENPROCESS Original_NtOpenProcess;
PFN_DBGKPCONVERTKERNELTOUSERSTATECHANGE DbgkpConvertKernelToUserStateChange;
PFN_DBGKPOPENHANDLES DbgkpOpenHandles;
PFN_KECOPYEXCEPTIONRECORD KeCopyExceptionRecord;
PFN_OBREFERENCEOBJECTBYHANDLEWITHTAG Sys_ObReferenceObjectByHandleWithTag;
PFN_OBREFERENCEOBJECTBYHANDLEWITHTAG Original_ObReferenceObjectByHandleWithTag;
PFN_OBREFERENCEOBJECTBYHANDLE Sys_ObReferenceObjectByHandle;
PFN_OBREFERENCEOBJECTBYHANDLE Original_ObReferenceObjectByHandle;
PFN_OBFDEREFERENCEOBJECTWITHTAG Sys_ObfDereferenceObjectWithTag;
PFN_OBFDEREFERENCEOBJECTWITHTAG Original_ObfDereferenceObjectWithTag;
PFN_OBFDEREFERENCEOBJECT Sys_ObfDereferenceObject;
PFN_OBFDEREFERENCEOBJECT Original_ObfDereferenceObject;
PFN_KEENTERCRITICALREGIONTHREAD KeEnterCriticalRegionThread;
PFN_KELEAVECRITICALREGIONTHREAD KeLeaveCriticalRegionThread;
PFN_MMCOPYVIRTUALMEMORY Sys_MmCopyVirtualMemory;
PFN_MMCOPYVIRTUALMEMORY Original_MmCopyVirtualMemory;
PFN_PSPCREATEUSERCONTEXT Sys_PspCreateUserContext;
PFN_PSPCREATEUSERCONTEXT Original_PspCreateUserContext;
PFN_PSPCALLTHREADNOTIFYROUTINES Sys_PspCallThreadNotifyRoutines;
PFN_PSPCALLTHREADNOTIFYROUTINES Original_PspCallThreadNotifyRoutines;
PFN_PSPALLOCATETHREAD Sys_PspAllocateThread;
PFN_PSPALLOCATETHREAD Original_PspAllocateThread;
PFN_OBPREFERENCEOBJECTBYHANDLEWITHTAG Sys_ObpReferenceObjectByHandleWithTag;
PFN_OBPREFERENCEOBJECTBYHANDLEWITHTAG Original_ObpReferenceObjectByHandleWithTag;
PFN_MIOBTAINREFERENCEDVADEX Sys_MiObtainReferencedVadEx;
PFN_MIOBTAINREFERENCEDVADEX Original_MiObtainReferencedVadEx;
PFN_MMPROTECTVIRTUALMEMORY Sys_MmProtectVirtualMemory;
PFN_MMPROTECTVIRTUALMEMORY Original_MmProtectVirtualMemory;
PFN_ZWGETCONTEXTTHREAD ZwGetContextThread;
PFN_PSPGETCONTEXTTHREADINTERNAL PspGetContextThreadInternal;
PFN_KESTACKATTACHPROCESS Sys_KeStackAttachProcess;
PFN_KESTACKATTACHPROCESS Original_KeStackAttachProcess;
PFN_KISTACKATTACHPROCESS Sys_KiStackAttachProcess;
PFN_KISTACKATTACHPROCESS Original_KiStackAttachProcess;
PFN_NTSETINFORMATIONDEBUGOBJECT Sys_NtSetInformationDebugObject;
PFN_NTTERMINATEPROCESS Sys_NtTerminateProcess;
PFN_NTTERMINATEPROCESS Original_NtTerminateProcess;
PFN_NTSUSPENDTHREAD Sys_NtSuspendThread;
PFN_NTSUSPENDTHREAD Original_NtSuspendThread;
PFN_NTRESUMETHREAD Sys_NtResumeThread;
PFN_NTRESUMETHREAD Original_NtResumeThread;
PFN_NTQUERYINFORMATIONTHREAD Sys_NtQueryInformationThread;
PFN_NTQUERYINFORMATIONTHREAD Original_NtQueryInformationThread;
PFN_VALIDATEHWND Sys_ValidateHwnd;
PFN_VALIDATEHWND Original_ValidateHwnd;
PFN_NTUSERWINDOWFROMPOINT Sys_NtUserWindowFromPoint;
PFN_NTUSERWINDOWFROMPOINT Original_NtUserWindowFromPoint;
PFN_PSGETCURRENTPROCESSBYTHREAD PsGetCurrentProcessByThread;
PFN_PSQUERYSYSTEMDLLINFO PsQuerySystemDllInfo;
PFN_PSWOW64GETPROCESSNTDLLTYPE PsWow64GetProcessNtdllType;
PFN_PSPREFERENCESYSTEMDLL PspReferenceSystemDll;
PFN_MISECTIONCONTROLAREA MiSectionControlArea;
PFN_DBGKPPOSTMODULEMESSAGES DbgkpPostModuleMessages;
PFN_PSPIMPLICITASSIGNPROCESSTOJOB PspImplicitAssignProcessToJob;
PFN_PSPUNLOCKPROCESSLISTEXCLUSIVE PspUnlockProcessListExclusive;
PFN_SEPDELETEACCESSSTATE SepDeleteAccessState;




//全局变量
PVOID PspLoaderInitRoutine;
unsigned __int64 game_cr3;

namespace eprocess_offset
{
    size_t Pcb;
    size_t DebugPort;
    size_t ImageFileName;
    size_t WoW64Process;
    size_t RundownProtect;
    size_t ExitTime;
    size_t Flags;
    size_t SectionBaseAddress;
    size_t SectionObject;
    size_t Machine;
    size_t ObjectTable;
    size_t UniqueProcessId;
    size_t Job;
    size_t ActiveProcessLinks;
    size_t SequenceNumber;
    size_t ProcessLock;
}

namespace kprocess_offset
{
    size_t DirectoryTableBase;
}

namespace ethread_offset
{
    size_t Tcb;
    size_t CrossThreadFlags;
    size_t Cid;
    size_t RundownProtect;
    size_t Win32StartAddress;
    size_t SameThreadPassiveFlags;
}

namespace kthread_offset
{
    size_t ApcState;
    size_t PreviousMode;
    size_t Teb;
    size_t Process;
    size_t KernelApcDisable;
    size_t MiscFlags;
    size_t TrapFrame;
    size_t SuspendCount;
    size_t SpecialApcDisable;
}

namespace kapc_state_offset
{
    size_t Process;
}

namespace image_nt_headers64_offset
{
    size_t Signature;
    size_t FileHeader;
    size_t OptionalHeader;
}

namespace image_file_header_offset
{
    size_t PointerToSymbolTable;
    size_t NumberOfSymbols;
}

namespace ewow64process_offset
{
    size_t Machine;
}

namespace mmvad_offset
{
    size_t Core; //_MMVAD_SHORT Core;
}

namespace mmvad_short_offset
{
    size_t LongFlags;
}

namespace handle_table_offset
{
    size_t UniqueProcessId;
}

namespace object_type_offset
{
    size_t TypeInfo;
}

namespace object_type_initializer_offset
{
    size_t GenericMapping;
}

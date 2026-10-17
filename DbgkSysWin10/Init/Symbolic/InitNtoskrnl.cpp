#include "../../Driver.h"
#include "../../ntos/inc/extypes.h"
#include "../../ntos/inc/ketypes.h"
#include "../../ntos/inc/ntosdef.h"
#include "../../ntos/inc/amd64.h"
#include "../../ntos/inc/mi.h"
#include "../../ntos/inc/pstypes.h"
#include "../../ntos/inc/obtypes.h"
#include "../../ntos/inc/mmtypes.h"
#include "../../ntos/inc/ntdbg.h"
#include "../../ntos/inc/peb_teb.h"
#include "../../List/MyList.h"
#include "../../ntos/inc/ntlpcapi.h"
#include "../../ntos/inc/psp.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"
#include "../../Log/log.h"
#include "../../Hooks/EptHook/EptHook.h"
#include "InitSymbolic.h"
#include "../../Encrypt/Blowfish/Blowfish.h"
#include "../../Hvm/vmcall_reason.h"
#include "../../Hvm/hypervisor_gateway.h"

BOOLEAN InitNtoskrnlSymbolsTable()
{
    //当前线程必须是gui线程才能访问win32k的内存
    //DbgBreakPoint();
    symbolic_access::ModuleExtenderFactory extenderFactory{};
    const auto& moduleExtender = extenderFactory.Create(L"ntoskrnl.exe");
    if (!moduleExtender.has_value())
    {
        outLog("ntoskrnl.exe 符号初始化失败..");
        return FALSE;
    }

    //内核的全局变量
    PspLoaderInitRoutine = moduleExtender->GetPointer<PVOID>("PspLoaderInitRoutine");
    outFile("Symbol PspLoaderInitRoutine = 0x%p", (PVOID)PspLoaderInitRoutine);
    DbgkDebugObjectType = moduleExtender->GetPointer<POBJECT_TYPE>("DbgkDebugObjectType");
    outFile("Symbol DbgkDebugObjectType = 0x%p", (PVOID)DbgkDebugObjectType);
    ObTypeIndexTable = moduleExtender->GetPointer<POBJECT_TYPE>("ObTypeIndexTable");
    outFile("Symbol ObTypeIndexTable = 0x%p", (PVOID)ObTypeIndexTable);
    PspNotifyEnableMask = (PULONG)moduleExtender->GetPointer<PULONG>("PspNotifyEnableMask");
    outFile("Symbol PspNotifyEnableMask = 0x%p", (PVOID)PspNotifyEnableMask);
    PerfGlobalGroupMask = (PULONG)moduleExtender->GetPointer<PULONG>("PerfGlobalGroupMask");
    outFile("Symbol PerfGlobalGroupMask = 0x%p", (PVOID)PerfGlobalGroupMask);
    PspActiveProcessLock = (PEX_PUSH_LOCK)moduleExtender->GetPointer<PEX_PUSH_LOCK>("PspActiveProcessLock");
    outFile("Symbol PspActiveProcessLock = 0x%p", (PVOID)PspActiveProcessLock);
    PspProcessSequenceNumber = (PULONG_PTR)moduleExtender->GetPointer<PULONG_PTR>("PspProcessSequenceNumber");
    outFile("Symbol PspProcessSequenceNumber = 0x%p", (PVOID)PspProcessSequenceNumber);
    PsActiveProcessHead = (PLIST_ENTRY)moduleExtender->GetPointer<PLIST_ENTRY>("PsActiveProcessHead");
    outFile("Symbol PsActiveProcessHead = 0x%p", (PVOID)PsActiveProcessHead);

    //内核函数指针
    PsFreezeProcess = (PFN_PSFREEZEPROCESS)moduleExtender->GetPointer<PFN_PSFREEZEPROCESS>("PsFreezeProcess");
    outFile("Symbol PsFreezeProcess = 0x%p", (PVOID)PsFreezeProcess);
    PsThawProcess = (PFN_PSTHAWPROCESS)moduleExtender->GetPointer<PFN_PSTHAWPROCESS>("PsThawProcess");
    outFile("Symbol PsThawProcess = 0x%p", (PVOID)PsThawProcess);
    Sys_NtCreateDebugObject = (PFN_NTCREATEDEBUGOBJECT)moduleExtender->GetPointer<PFN_NTCREATEDEBUGOBJECT>("NtCreateDebugObject");
    outFile("Symbol Sys_NtCreateDebugObject = 0x%p", (PVOID)Sys_NtCreateDebugObject);
    DbgkpSuspendProcess = (PFN_DBGKPSUSPENDPROCESS)moduleExtender->GetPointer<PFN_DBGKPSUSPENDPROCESS>("DbgkpSuspendProcess");
    outFile("Symbol DbgkpSuspendProcess = 0x%p", (PVOID)DbgkpSuspendProcess);
    Sys_DbgkCreateThread = (PFN_DBGKCREATETHREAD)moduleExtender->GetPointer<PFN_DBGKCREATETHREAD>("DbgkCreateThread");
    outFile("Symbol Sys_DbgkCreateThread = 0x%p", (PVOID)Sys_DbgkCreateThread);
    Sys_DbgkpQueueMessage = (PFN_DBGKPQUEUEMESSAGE)moduleExtender->GetPointer<PFN_DBGKPQUEUEMESSAGE>("DbgkpQueueMessage");
    outFile("Symbol Sys_DbgkpQueueMessage = 0x%p", (PVOID)Sys_DbgkpQueueMessage);
    DbgkpSectionToFileHandle = (PFN_DBGKPSECTIONTOFILEHANDLE)moduleExtender->GetPointer<PFN_DBGKPSECTIONTOFILEHANDLE>("DbgkpSectionToFileHandle");
    outFile("Symbol DbgkpSectionToFileHandle = 0x%p", (PVOID)DbgkpSectionToFileHandle);
    DbgkpSendApiMessage = (PFN_DBGKPSENDAPIMESSAGE)moduleExtender->GetPointer<PFN_DBGKPSENDAPIMESSAGE>("DbgkpSendApiMessage");
    outFile("Symbol DbgkpSendApiMessage = 0x%p", (PVOID)DbgkpSendApiMessage);
    Sys_DbgkMapViewOfSection = (PFN_DBGKMAPVIEWOFSECTION)moduleExtender->GetPointer<PFN_DBGKMAPVIEWOFSECTION>("DbgkMapViewOfSection");
    outFile("Symbol Sys_DbgkMapViewOfSection = 0x%p", (PVOID)Sys_DbgkMapViewOfSection);
    Sys_DbgkUnMapViewOfSection = (PFN_DBGKUNMAPVIEWOFSECTION)moduleExtender->GetPointer<PFN_DBGKUNMAPVIEWOFSECTION>("DbgkUnMapViewOfSection");
    outFile("Symbol Sys_DbgkUnMapViewOfSection = 0x%p", (PVOID)Sys_DbgkUnMapViewOfSection);
    DbgkpSuppressDbgMsg = (PFN_DBGKPSUPPRESSDBGMSG)moduleExtender->GetPointer<PFN_DBGKPSUPPRESSDBGMSG>("DbgkpSuppressDbgMsg");
    outFile("Symbol DbgkpSuppressDbgMsg = 0x%p", (PVOID)DbgkpSuppressDbgMsg);
    Sys_DbgkExitThread = (PFN_DBGKEXITTHREAD)moduleExtender->GetPointer<PFN_DBGKEXITTHREAD>("DbgkExitThread");
    outFile("Symbol Sys_DbgkExitThread = 0x%p", (PVOID)Sys_DbgkExitThread);
    PsSetProcessFaultInformation = (PFN_PSSETPROCESSFAULTINFORMATION)moduleExtender->GetPointer<PFN_PSSETPROCESSFAULTINFORMATION>("PsSetProcessFaultInformation");
    outFile("Symbol PsSetProcessFaultInformation = 0x%p", (PVOID)PsSetProcessFaultInformation);
    PsCaptureExceptionPort = (PFN_PSCAPTUREEXCEPTIONPORT)moduleExtender->GetPointer<PFN_PSCAPTUREEXCEPTIONPORT>("PsCaptureExceptionPort");
    outFile("Symbol PsCaptureExceptionPort = 0x%p", (PVOID)PsCaptureExceptionPort);
    DbgkpSendApiMessageLpc = (PFN_DBGKPSENDAPIMESSAGELPC)moduleExtender->GetPointer<PFN_DBGKPSENDAPIMESSAGELPC>("DbgkpSendApiMessageLpc");
    outFile("Symbol DbgkpSendApiMessageLpc = 0x%p", (PVOID)DbgkpSendApiMessageLpc);
    DbgkpSendErrorMessage = (PFN_DBGKPSENDERRORMESSAGE)moduleExtender->GetPointer<PFN_DBGKPSENDERRORMESSAGE>("DbgkpSendErrorMessage");
    outFile("Symbol DbgkpSendErrorMessage = 0x%p", (PVOID)DbgkpSendErrorMessage);
    Sys_DbgkForwardException = (PFN_DBGKFORWARDEXCEPTION)moduleExtender->GetPointer<PFN_DBGKFORWARDEXCEPTION>("DbgkForwardException");
    outFile("Symbol Sys_DbgkForwardException = 0x%p", (PVOID)Sys_DbgkForwardException);
    //DbgkpSetProcessDebugObject = (PFN_DBGKPSETPROCESSDEBUGOBJECT)moduleExtender->GetPointer<PFN_DBGKPSETPROCESSDEBUGOBJECT>("DbgkpSetProcessDebugObject");
    DbgkpPostFakeProcessCreateMessages = (PFN_DBGKPPOSTFAKEPROCESSCREATEMESSAGES)moduleExtender->GetPointer<PFN_DBGKPPOSTFAKEPROCESSCREATEMESSAGES>("DbgkpPostFakeProcessCreateMessages");
    outFile("Symbol DbgkpPostFakeProcessCreateMessages = 0x%p", (PVOID)DbgkpPostFakeProcessCreateMessages);
    Sys_NtDebugActiveProcess = (PFN_NTDEBUGACTIVEPROCESS)moduleExtender->GetPointer<PFN_NTDEBUGACTIVEPROCESS>("NtDebugActiveProcess");
    outFile("Symbol Sys_NtDebugActiveProcess = 0x%p", (PVOID)Sys_NtDebugActiveProcess);
    Sys_DbgkExitProcess = (PFN_DBGKEXITPROCESS)moduleExtender->GetPointer<PFN_DBGKEXITPROCESS>("DbgkExitProcess");
    outFile("Symbol Sys_DbgkExitProcess = 0x%p", (PVOID)Sys_DbgkExitProcess);
    Sys_PspExitThread = (PFN_PSPEXITTHREAD)moduleExtender->GetPointer<PFN_PSPEXITTHREAD>("PspExitThread");
    outFile("Symbol Sys_PspExitThread = 0x%p", (PVOID)Sys_PspExitThread);
    DbgkpWakeTarget = (PFN_DBGKPWAKETARGET)moduleExtender->GetPointer<PFN_DBGKPWAKETARGET>("DbgkpWakeTarget");
    outFile("Symbol DbgkpWakeTarget = 0x%p", (PVOID)DbgkpWakeTarget);
    Sys_NtDebugContinue = (PFN_NTDEBUGCONTINUE)moduleExtender->GetPointer<PFN_NTDEBUGCONTINUE>("NtDebugContinue");
    outFile("Symbol Sys_NtDebugContinue = 0x%p", (PVOID)Sys_NtDebugContinue);
    Sys_NtWaitForDebugEvent = (PFN_NTWAITFORDEBUGEVENT)moduleExtender->GetPointer<PFN_NTWAITFORDEBUGEVENT>("NtWaitForDebugEvent");
    outFile("Symbol Sys_NtWaitForDebugEvent = 0x%p", (PVOID)Sys_NtWaitForDebugEvent);
    ObDuplicateObject = (PFN_OBDUPLICATEOBJECT)moduleExtender->GetPointer<PFN_OBDUPLICATEOBJECT>("ObDuplicateObject");
    outFile("Symbol ObDuplicateObject = 0x%p", (PVOID)ObDuplicateObject);
    DbgkClearProcessDebugObject = (PFN_DBGKCLEARPROCESSDEBUGOBJECT)moduleExtender->GetPointer<PFN_DBGKCLEARPROCESSDEBUGOBJECT>("DbgkClearProcessDebugObject");
    outFile("Symbol DbgkClearProcessDebugObject = 0x%p", (PVOID)DbgkClearProcessDebugObject);
    Sys_NtRemoveProcessDebug = (PFN_NTREMOVEPROCESSDEBUG)moduleExtender->GetPointer<PFN_NTREMOVEPROCESSDEBUG>("NtRemoveProcessDebug");
    outFile("Symbol Sys_NtRemoveProcessDebug = 0x%p", (PVOID)Sys_NtRemoveProcessDebug);
    PsGetNextProcess = (PFN_PSGETNEXTPROCESS)moduleExtender->GetPointer<PFN_PSGETNEXTPROCESS>("PsGetNextProcess");
    outFile("Symbol PsGetNextProcess = 0x%p", (PVOID)PsGetNextProcess);
    DbgkpMarkProcessPeb = (PFN_DBGKPMARKPROCESSPEB)moduleExtender->GetPointer<PFN_DBGKPMARKPROCESSPEB>("DbgkpMarkProcessPeb");
    outFile("Symbol DbgkpMarkProcessPeb = 0x%p", (PVOID)DbgkpMarkProcessPeb);
    PsTerminateProcess = (PFN_PSTERMINATEPROCESS)moduleExtender->GetPointer<PFN_PSTERMINATEPROCESS>("PsTerminateProcess");
    outFile("Symbol PsTerminateProcess = 0x%p", (PVOID)PsTerminateProcess);
    ObCreateObjectType = (PFN_OBCREATEOBJECTTYPE)moduleExtender->GetPointer<PFN_OBCREATEOBJECTTYPE>("ObCreateObjectType");
    outFile("Symbol ObCreateObjectType = 0x%p", (PVOID)ObCreateObjectType);
    PsGetNextProcessThread = (PFN_PSGETNEXTPROCESSTHREAD)moduleExtender->GetPointer<PFN_PSGETNEXTPROCESSTHREAD>("PsGetNextProcessThread");
    outFile("Symbol PsGetNextProcessThread = 0x%p", (PVOID)PsGetNextProcessThread);
    DbgkpPostFakeThreadMessages = (PFN_DBGKPPOSTFAKETHREADMESSAGES)moduleExtender->GetPointer<PFN_DBGKPPOSTFAKETHREADMESSAGES>("DbgkpPostFakeThreadMessages");
    outFile("Symbol DbgkpPostFakeThreadMessages = 0x%p", (PVOID)DbgkpPostFakeThreadMessages);
    KiStackAttachProcess = (PFN_KISTACKATTACHPROCESS)moduleExtender->GetPointer<PFN_KISTACKATTACHPROCESS>("KiStackAttachProcess");
    outFile("Symbol KiStackAttachProcess = 0x%p", (PVOID)KiStackAttachProcess);
    KiUnstackDetachProcess = (PFN_KIUNSTACKDETACHPROCESS)moduleExtender->GetPointer<PFN_KIUNSTACKDETACHPROCESS>("KiUnstackDetachProcess");
    outFile("Symbol KiUnstackDetachProcess = 0x%p", (PVOID)KiUnstackDetachProcess);
    Sys_NtReadVirtualMemory = (PFN_NTREADVIRTUALMEMORY)moduleExtender->GetPointer<PFN_NTREADVIRTUALMEMORY>("NtReadVirtualMemory");
    outFile("Symbol Sys_NtReadVirtualMemory = 0x%p", (PVOID)Sys_NtReadVirtualMemory);
    Sys_NtWriteVirtualMemory = (PFN_NTWRITEVIRTUALMEMORY)moduleExtender->GetPointer<PFN_NTWRITEVIRTUALMEMORY>("NtWriteVirtualMemory");
    outFile("Symbol Sys_NtWriteVirtualMemory = 0x%p", (PVOID)Sys_NtWriteVirtualMemory);
    ZwProtectVirtualMemory = (PFN_NTPROTECTVIRTUALMEMORY)moduleExtender->GetPointer<PFN_NTPROTECTVIRTUALMEMORY>("ZwProtectVirtualMemory");
    outFile("Symbol ZwProtectVirtualMemory = 0x%p", (PVOID)ZwProtectVirtualMemory);
    Sys_NtProtectVirtualMemory = (PFN_NTPROTECTVIRTUALMEMORY)moduleExtender->GetPointer<PFN_NTPROTECTVIRTUALMEMORY>("NtProtectVirtualMemory");
    outFile("Symbol Sys_NtProtectVirtualMemory = 0x%p", (PVOID)Sys_NtProtectVirtualMemory);
    Sys_PspCreateThread = (PFN_PSPCREATETHREAD)moduleExtender->GetPointer<PFN_PSPCREATETHREAD>("PspCreateThread");
    outFile("Symbol Sys_PspCreateThread = 0x%p", (PVOID)Sys_PspCreateThread);
    Sys_NtCreateThreadEx = (PFN_NTCREATETHREADEX)moduleExtender->GetPointer<PFN_NTCREATETHREADEX>("NtCreateThreadEx");
    outFile("Symbol Sys_NtCreateThreadEx = 0x%p", (PVOID)Sys_NtCreateThreadEx);
    Sys_NtOpenProcess = (PFN_NTOPENPROCESS)moduleExtender->GetPointer<PFN_NTOPENPROCESS>("NtOpenProcess");
    outFile("Symbol Sys_NtOpenProcess = 0x%p", (PVOID)Sys_NtOpenProcess);
    DbgkpConvertKernelToUserStateChange = (PFN_DBGKPCONVERTKERNELTOUSERSTATECHANGE)moduleExtender->GetPointer<PFN_DBGKPCONVERTKERNELTOUSERSTATECHANGE>("DbgkpConvertKernelToUserStateChange");
    outFile("Symbol DbgkpConvertKernelToUserStateChange = 0x%p", (PVOID)DbgkpConvertKernelToUserStateChange);
    DbgkpOpenHandles = (PFN_DBGKPOPENHANDLES)moduleExtender->GetPointer<PFN_DBGKPOPENHANDLES>("DbgkpOpenHandles");
    outFile("Symbol DbgkpOpenHandles = 0x%p", (PVOID)DbgkpOpenHandles);
    KeCopyExceptionRecord = (PFN_KECOPYEXCEPTIONRECORD)moduleExtender->GetPointer<PFN_KECOPYEXCEPTIONRECORD>("KeCopyExceptionRecord");
    outFile("Symbol KeCopyExceptionRecord = 0x%p", (PVOID)KeCopyExceptionRecord);
    Sys_ObReferenceObjectByHandleWithTag = (PFN_OBREFERENCEOBJECTBYHANDLEWITHTAG)moduleExtender->GetPointer<PFN_OBREFERENCEOBJECTBYHANDLEWITHTAG>("ObReferenceObjectByHandleWithTag");
    outFile("Symbol Sys_ObReferenceObjectByHandleWithTag = 0x%p", (PVOID)Sys_ObReferenceObjectByHandleWithTag);
    Sys_ObReferenceObjectByHandle = (PFN_OBREFERENCEOBJECTBYHANDLE)moduleExtender->GetPointer<PFN_OBREFERENCEOBJECTBYHANDLE>("ObReferenceObjectByHandle");
    outFile("Symbol Sys_ObReferenceObjectByHandle = 0x%p", (PVOID)Sys_ObReferenceObjectByHandle);
    Sys_ObfDereferenceObjectWithTag = (PFN_OBFDEREFERENCEOBJECTWITHTAG)moduleExtender->GetPointer<PFN_OBFDEREFERENCEOBJECTWITHTAG>("ObfDereferenceObjectWithTag");
    outFile("Symbol Sys_ObfDereferenceObjectWithTag = 0x%p", (PVOID)Sys_ObfDereferenceObjectWithTag);
    Sys_ObfDereferenceObject = (PFN_OBFDEREFERENCEOBJECT)moduleExtender->GetPointer<PFN_OBFDEREFERENCEOBJECT>("ObfDereferenceObject");
    outFile("Symbol Sys_ObfDereferenceObject = 0x%p", (PVOID)Sys_ObfDereferenceObject);
    KiCheckForKernelApcDelivery = (PFN_KICHECKFORKERNELAPCDELIVERY)moduleExtender->GetPointer<PFN_KICHECKFORKERNELAPCDELIVERY>("KiCheckForKernelApcDelivery");
    outFile("Symbol KiCheckForKernelApcDelivery = 0x%p", (PVOID)KiCheckForKernelApcDelivery);
    KeEnterCriticalRegionThread = (PFN_KEENTERCRITICALREGIONTHREAD)moduleExtender->GetPointer<PFN_KEENTERCRITICALREGIONTHREAD>("KeEnterCriticalRegionThread");
    outFile("Symbol KeEnterCriticalRegionThread = 0x%p", (PVOID)KeEnterCriticalRegionThread);
    KeLeaveCriticalRegionThread = (PFN_KELEAVECRITICALREGIONTHREAD)moduleExtender->GetPointer<PFN_KELEAVECRITICALREGIONTHREAD>("KeLeaveCriticalRegionThread");
    outFile("Symbol KeLeaveCriticalRegionThread = 0x%p", (PVOID)KeLeaveCriticalRegionThread);
    Sys_MmCopyVirtualMemory = (PFN_MMCOPYVIRTUALMEMORY)moduleExtender->GetPointer<PFN_MMCOPYVIRTUALMEMORY>("MmCopyVirtualMemory");
    outFile("Symbol Sys_MmCopyVirtualMemory = 0x%p", (PVOID)Sys_MmCopyVirtualMemory);
    Sys_PspCreateUserContext = (PFN_PSPCREATEUSERCONTEXT)moduleExtender->GetPointer<PFN_PSPCREATEUSERCONTEXT>("PspCreateUserContext");
    outFile("Symbol Sys_PspCreateUserContext = 0x%p", (PVOID)Sys_PspCreateUserContext);
    Sys_PspCallThreadNotifyRoutines = (PFN_PSPCALLTHREADNOTIFYROUTINES)moduleExtender->GetPointer<PFN_PSPCALLTHREADNOTIFYROUTINES>("PspCallThreadNotifyRoutines");
    outFile("Symbol Sys_PspCallThreadNotifyRoutines = 0x%p", (PVOID)Sys_PspCallThreadNotifyRoutines);
    Sys_PspAllocateThread = (PFN_PSPALLOCATETHREAD)moduleExtender->GetPointer<PFN_PSPALLOCATETHREAD>("PspAllocateThread");
    outFile("Symbol Sys_PspAllocateThread = 0x%p", (PVOID)Sys_PspAllocateThread);
    Sys_ObpReferenceObjectByHandleWithTag = (PFN_OBPREFERENCEOBJECTBYHANDLEWITHTAG)moduleExtender->GetPointer<PFN_OBPREFERENCEOBJECTBYHANDLEWITHTAG>("ObpReferenceObjectByHandleWithTag");
    outFile("Symbol Sys_ObpReferenceObjectByHandleWithTag = 0x%p", (PVOID)Sys_ObpReferenceObjectByHandleWithTag);
    Sys_MiObtainReferencedVadEx = (PFN_MIOBTAINREFERENCEDVADEX)moduleExtender->GetPointer<PFN_MIOBTAINREFERENCEDVADEX>("MiObtainReferencedVadEx");
    outFile("Symbol Sys_MiObtainReferencedVadEx = 0x%p", (PVOID)Sys_MiObtainReferencedVadEx);
    Sys_MmProtectVirtualMemory = (PFN_MMPROTECTVIRTUALMEMORY)moduleExtender->GetPointer<PFN_MMPROTECTVIRTUALMEMORY>("MmProtectVirtualMemory");
    outFile("Symbol Sys_MmProtectVirtualMemory = 0x%p", (PVOID)Sys_MmProtectVirtualMemory);
    Sys_NtGetContextThread = (PFN_NTGETCONTEXTTHREAD)moduleExtender->GetPointer<PFN_NTGETCONTEXTTHREAD>("NtGetContextThread");
    outFile("Symbol Sys_NtGetContextThread = 0x%p", (PVOID)Sys_NtGetContextThread);
    Sys_NtSetContextThread = (PFN_NTSETCONTEXTTHREAD)moduleExtender->GetPointer<PFN_NTSETCONTEXTTHREAD>("NtSetContextThread");
    outFile("Symbol Sys_NtSetContextThread = 0x%p", (PVOID)Sys_NtSetContextThread);
    Sys_NtQueryInformationProcess = (PFN_NTQUERYINFORMATIONPROCESS)moduleExtender->GetPointer<PFN_NTQUERYINFORMATIONPROCESS>("NtQueryInformationProcess");
    outFile("Symbol Sys_NtQueryInformationProcess = 0x%p", (PVOID)Sys_NtQueryInformationProcess);
    Sys_NtSetInformationProcess = (PFN_NTSETINFORMATIONPROCESS)moduleExtender->GetPointer<PFN_NTSETINFORMATIONPROCESS>("NtSetInformationProcess");
    outFile("Symbol Sys_NtSetInformationProcess = 0x%p", (PVOID)Sys_NtSetInformationProcess);
    Sys_NtQueryObject = (PFN_NTQUERYOBJECT)moduleExtender->GetPointer<PFN_NTQUERYOBJECT>("NtQueryObject");
    outFile("Symbol Sys_NtQueryObject = 0x%p", (PVOID)Sys_NtQueryObject);
    Sys_NtQuerySystemInformation = (PFN_NTQUERYSYSTEMINFORMATION)moduleExtender->GetPointer<PFN_NTQUERYSYSTEMINFORMATION>("NtQuerySystemInformation");
    outFile("Symbol Sys_NtQuerySystemInformation = 0x%p", (PVOID)Sys_NtQuerySystemInformation);
    Sys_NtSystemDebugControl = (PFN_NTSYSTEMDEBUGCONTROL)moduleExtender->GetPointer<PFN_NTSYSTEMDEBUGCONTROL>("NtSystemDebugControl");
    outFile("Symbol Sys_NtSystemDebugControl = 0x%p", (PVOID)Sys_NtSystemDebugControl);
    Sys_NtClose = (PFN_NTCLOSE)moduleExtender->GetPointer<PFN_NTCLOSE>("NtClose");
    outFile("Symbol Sys_NtClose = 0x%p", (PVOID)Sys_NtClose);
    Sys_NtDuplicateObject = (PFN_NTDUPLICATEOBJECT)moduleExtender->GetPointer<PFN_NTDUPLICATEOBJECT>("NtDuplicateObject");
    outFile("Symbol Sys_NtDuplicateObject = 0x%p", (PVOID)Sys_NtDuplicateObject);
    Sys_NtSetInformationThread = (PFN_NTSETINFORMATIONTHREAD)moduleExtender->GetPointer<PFN_NTSETINFORMATIONTHREAD>("NtSetInformationThread");
    outFile("Symbol Sys_NtSetInformationThread = 0x%p", (PVOID)Sys_NtSetInformationThread);
    Sys_ObReferenceObjectByPointer = (PFN_OBREFERENCEOBJECTBYPOINTER)moduleExtender->GetPointer<PFN_OBREFERENCEOBJECTBYPOINTER>("ObReferenceObjectByPointer");
    outFile("Symbol Sys_ObReferenceObjectByPointer = 0x%p", (PVOID)Sys_ObReferenceObjectByPointer);
    ZwGetContextThread = (PFN_ZWGETCONTEXTTHREAD)moduleExtender->GetPointer<PFN_ZWGETCONTEXTTHREAD>("ZwGetContextThread");
    outFile("Symbol ZwGetContextThread = 0x%p", (PVOID)ZwGetContextThread);
    PspGetContextThreadInternal = (PFN_PSPGETCONTEXTTHREADINTERNAL)moduleExtender->GetPointer<PFN_PSPGETCONTEXTTHREADINTERNAL>("PspGetContextThreadInternal");
    outFile("Symbol PspGetContextThreadInternal = 0x%p", (PVOID)PspGetContextThreadInternal);
    Sys_KiDispatchException = (PFN_KIDISPATCHEXCEPTION)moduleExtender->GetPointer<PFN_KIDISPATCHEXCEPTION>("KiDispatchException");
    outFile("Symbol Sys_KiDispatchException = 0x%p", (PVOID)Sys_KiDispatchException);
    Sys_KeStackAttachProcess = (PFN_KESTACKATTACHPROCESS)moduleExtender->GetPointer<PFN_KESTACKATTACHPROCESS>("KeStackAttachProcess");
    outFile("Symbol Sys_KeStackAttachProcess = 0x%p", (PVOID)Sys_KeStackAttachProcess);
    Sys_KiStackAttachProcess = (PFN_KISTACKATTACHPROCESS)moduleExtender->GetPointer<PFN_KISTACKATTACHPROCESS>("KiStackAttachProcess");
    outFile("Symbol Sys_KiStackAttachProcess = 0x%p", (PVOID)Sys_KiStackAttachProcess);
    Sys_NtSetInformationDebugObject = (PFN_NTSETINFORMATIONDEBUGOBJECT)moduleExtender->GetPointer<PFN_NTSETINFORMATIONDEBUGOBJECT>("NtSetInformationDebugObject");
    outFile("Symbol Sys_NtSetInformationDebugObject = 0x%p", (PVOID)Sys_NtSetInformationDebugObject);
    Sys_DbgkOpenProcessDebugPort = (PFN_DBGKOPENPROCESSDEBUGPORT)moduleExtender->GetPointer<PFN_DBGKOPENPROCESSDEBUGPORT>("DbgkOpenProcessDebugPort");
    outFile("Symbol Sys_DbgkOpenProcessDebugPort = 0x%p", (PVOID)Sys_DbgkOpenProcessDebugPort);
    Sys_NtTerminateProcess = (PFN_NTTERMINATEPROCESS)moduleExtender->GetPointer<PFN_NTTERMINATEPROCESS>("NtTerminateProcess");
    outFile("Symbol Sys_NtTerminateProcess = 0x%p", (PVOID)Sys_NtTerminateProcess);
    Sys_NtSuspendThread = (PFN_NTSUSPENDTHREAD)moduleExtender->GetPointer<PFN_NTSUSPENDTHREAD>("NtSuspendThread");
    outFile("Symbol Sys_NtSuspendThread = 0x%p", (PVOID)Sys_NtSuspendThread);
    Sys_NtResumeThread = (PFN_NTRESUMETHREAD)moduleExtender->GetPointer<PFN_NTRESUMETHREAD>("NtResumeThread");
    outFile("Symbol Sys_NtResumeThread = 0x%p", (PVOID)Sys_NtResumeThread);
    Sys_NtQueryInformationThread = (PFN_NTQUERYINFORMATIONTHREAD)moduleExtender->GetPointer<PFN_NTQUERYINFORMATIONTHREAD>("NtQueryInformationThread");
    outFile("Symbol Sys_NtQueryInformationThread = 0x%p", (PVOID)Sys_NtQueryInformationThread);
    PsGetCurrentProcessByThread = (PFN_PSGETCURRENTPROCESSBYTHREAD)moduleExtender->GetPointer<PFN_PSGETCURRENTPROCESSBYTHREAD>("PsGetCurrentProcessByThread");
    outFile("Symbol PsGetCurrentProcessByThread = 0x%p", (PVOID)PsGetCurrentProcessByThread);
    PsQuerySystemDllInfo = (PFN_PSQUERYSYSTEMDLLINFO)moduleExtender->GetPointer<PFN_PSQUERYSYSTEMDLLINFO>("PsQuerySystemDllInfo");
    outFile("Symbol PsQuerySystemDllInfo = 0x%p", (PVOID)PsQuerySystemDllInfo);
    PsWow64GetProcessNtdllType = (PFN_PSWOW64GETPROCESSNTDLLTYPE)moduleExtender->GetPointer<PFN_PSWOW64GETPROCESSNTDLLTYPE>("PsWow64GetProcessNtdllType");
    outFile("Symbol PsWow64GetProcessNtdllType = 0x%p", (PVOID)PsWow64GetProcessNtdllType);
    PspReferenceSystemDll = (PFN_PSPREFERENCESYSTEMDLL)moduleExtender->GetPointer<PFN_PSPREFERENCESYSTEMDLL>("PspReferenceSystemDll");
    outFile("Symbol PspReferenceSystemDll = 0x%p", (PVOID)PspReferenceSystemDll);
    MiSectionControlArea = (PFN_MISECTIONCONTROLAREA)moduleExtender->GetPointer<PFN_MISECTIONCONTROLAREA>("MiSectionControlArea");
    outFile("Symbol MiSectionControlArea = 0x%p", (PVOID)MiSectionControlArea);
    MiReferenceControlAreaFile = (PFN_MIREFERENCECONTROLAREAFILE)moduleExtender->GetPointer<PFN_MIREFERENCECONTROLAREAFILE>("MiReferenceControlAreaFile");
    outFile("Symbol MiReferenceControlAreaFile = 0x%p", (PVOID)MiReferenceControlAreaFile);
    ObFastDereferenceObject = (PFN_OBFASTDEREFERENCEOBJECT)moduleExtender->GetPointer<PFN_OBFASTDEREFERENCEOBJECT>("ObFastDereferenceObject");
    outFile("Symbol ObFastDereferenceObject = 0x%p", (PVOID)ObFastDereferenceObject);
    DbgkpPostModuleMessages = (PFN_DBGKPPOSTMODULEMESSAGES)moduleExtender->GetPointer<PFN_DBGKPPOSTMODULEMESSAGES>("DbgkpPostModuleMessages");
    outFile("Symbol DbgkpPostModuleMessages = 0x%p", (PVOID)DbgkpPostModuleMessages);
    PsCallImageNotifyRoutines = (PFN_PSCALLIMAGENOTIFYROUTINES)moduleExtender->GetPointer<PFN_PSCALLIMAGENOTIFYROUTINES>("PsCallImageNotifyRoutines");
    outFile("Symbol PsCallImageNotifyRoutines = 0x%p", (PVOID)PsCallImageNotifyRoutines);
    PsReferenceProcessFilePointer = (PFN_PSREFERENCEPROCESSFILEPOINTER)moduleExtender->GetPointer<PFN_PSREFERENCEPROCESSFILEPOINTER>("PsReferenceProcessFilePointer");
    outFile("Symbol PsReferenceProcessFilePointer = 0x%p", (PVOID)PsReferenceProcessFilePointer);
    SeAuditingWithTokenForSubcategory = (PFN_SEAUDITINGWITHTOKENFORSUBCATEGORY)moduleExtender->GetPointer<PFN_SEAUDITINGWITHTOKENFORSUBCATEGORY>("SeAuditingWithTokenForSubcategory");
    outFile("Symbol SeAuditingWithTokenForSubcategory = 0x%p", (PVOID)SeAuditingWithTokenForSubcategory);
    SeAuditProcessCreation = (PFN_SEAUDITPROCESSCREATION)moduleExtender->GetPointer<PFN_SEAUDITPROCESSCREATION>("SeAuditProcessCreation");
    outFile("Symbol SeAuditProcessCreation = 0x%p", (PVOID)SeAuditProcessCreation);
    PspImplicitAssignProcessToJob = (PFN_PSPIMPLICITASSIGNPROCESSTOJOB)moduleExtender->GetPointer<PFN_PSPIMPLICITASSIGNPROCESSTOJOB>("PspImplicitAssignProcessToJob");
    outFile("Symbol PspImplicitAssignProcessToJob = 0x%p", (PVOID)PspImplicitAssignProcessToJob);
    PspUnlockProcessListExclusive = (PFN_PSPUNLOCKPROCESSLISTEXCLUSIVE)moduleExtender->GetPointer<PFN_PSPUNLOCKPROCESSLISTEXCLUSIVE>("PspUnlockProcessListExclusive");
    outFile("Symbol PspUnlockProcessListExclusive = 0x%p", (PVOID)PspUnlockProcessListExclusive);
    DbgkCopyProcessDebugPort = (PFN_DBGKCOPYPROCESSDEBUGPORT)moduleExtender->GetPointer<PFN_DBGKCOPYPROCESSDEBUGPORT>("DbgkCopyProcessDebugPort");
    outFile("Symbol DbgkCopyProcessDebugPort = 0x%p", (PVOID)DbgkCopyProcessDebugPort);
    SeCreateAccessStateEx = (PFN_SECREATEACCESSSTATEEX)moduleExtender->GetPointer<PFN_SECREATEACCESSSTATEEX>("SeCreateAccessStateEx");
    outFile("Symbol SeCreateAccessStateEx = 0x%p", (PVOID)SeCreateAccessStateEx);
    ObInsertObjectEx = (PFN_OBINSERTOBJECTEX)moduleExtender->GetPointer<PFN_OBINSERTOBJECTEX>("ObInsertObjectEx");
    outFile("Symbol ObInsertObjectEx = 0x%p", (PVOID)ObInsertObjectEx);
    ObCheckRefTraceProcess = (PFN_OBCHECKREFTRACEPROCESS)moduleExtender->GetPointer<PFN_OBCHECKREFTRACEPROCESS>("ObCheckRefTraceProcess");
    outFile("Symbol ObCheckRefTraceProcess = 0x%p", (PVOID)ObCheckRefTraceProcess);
    PspValidateJobAffinityState = (PFN_PSPVALIDATEJOBAFFINITYSTATE)moduleExtender->GetPointer<PFN_PSPVALIDATEJOBAFFINITYSTATE>("PspValidateJobAffinityState");
    outFile("Symbol PspValidateJobAffinityState = 0x%p", (PVOID)PspValidateJobAffinityState);
    SepDeleteAccessState = (PFN_SEPDELETEACCESSSTATE)moduleExtender->GetPointer<PFN_SEPDELETEACCESSSTATE>("SepDeleteAccessState");
    outFile("Symbol SepDeleteAccessState = 0x%p", (PVOID)SepDeleteAccessState);
    Sys_PspInsertProcess = (PFN_PSPINSERTPROCESS)moduleExtender->GetPointer<PFN_PSPINSERTPROCESS>("PspInsertProcess");
    outFile("Symbol Sys_PspInsertProcess = 0x%p", (PVOID)Sys_PspInsertProcess);
    DbgkSendSystemDllMessages = (PFN_DBGKSENDSYSTEMDLLMESSAGES)moduleExtender->GetPointer<PFN_DBGKSENDSYSTEMDLLMESSAGES>("DbgkSendSystemDllMessages");
    outFile("Symbol DbgkSendSystemDllMessages = 0x%p", (PVOID)DbgkSendSystemDllMessages);


    eprocess_offset::Pcb = moduleExtender->GetOffset("_EPROCESS", "Pcb").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::Pcb = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::Pcb);
    eprocess_offset::DebugPort = moduleExtender->GetOffset("_EPROCESS", "DebugPort").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::DebugPort = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::DebugPort);
    eprocess_offset::Peb = moduleExtender->GetOffset("_EPROCESS", "Peb").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::Peb = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::Peb);
    eprocess_offset::ImageFileName = moduleExtender->GetOffset("_EPROCESS", "ImageFileName").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::ImageFileName = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::ImageFileName);
    eprocess_offset::WoW64Process = moduleExtender->GetOffset("_EPROCESS", "WoW64Process").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::WoW64Process = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::WoW64Process);
    eprocess_offset::RundownProtect = moduleExtender->GetOffset("_EPROCESS", "RundownProtect").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::RundownProtect = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::RundownProtect);
    eprocess_offset::ExitTime = moduleExtender->GetOffset("_EPROCESS", "ExitTime").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::ExitTime = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::ExitTime);
    eprocess_offset::Flags = moduleExtender->GetOffset("_EPROCESS", "Flags").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::Flags = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::Flags);
    eprocess_offset::SectionBaseAddress = moduleExtender->GetOffset("_EPROCESS", "SectionBaseAddress").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::SectionBaseAddress = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::SectionBaseAddress);
    eprocess_offset::SectionObject = moduleExtender->GetOffset("_EPROCESS", "SectionObject").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::SectionObject = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::SectionObject);
    eprocess_offset::Machine = moduleExtender->GetOffset("_EPROCESS", "Machine").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::Machine = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::Machine);
    eprocess_offset::ObjectTable = moduleExtender->GetOffset("_EPROCESS", "ObjectTable").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::ObjectTable = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::ObjectTable);
    eprocess_offset::UniqueProcessId = moduleExtender->GetOffset("_EPROCESS", "UniqueProcessId").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::UniqueProcessId = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::UniqueProcessId);
    eprocess_offset::Job = moduleExtender->GetOffset("_EPROCESS", "Job").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::Job = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::Job);
    eprocess_offset::ActiveProcessLinks = moduleExtender->GetOffset("_EPROCESS", "ActiveProcessLinks").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::ActiveProcessLinks = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::ActiveProcessLinks);
    eprocess_offset::SequenceNumber = moduleExtender->GetOffset("_EPROCESS", "SequenceNumber").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::SequenceNumber = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::SequenceNumber);
    eprocess_offset::ProcessLock = moduleExtender->GetOffset("_EPROCESS", "ProcessLock").value_or(0xFFFFFFFF);
    outFile("Offset eprocess_offset::ProcessLock = 0x%p", (PVOID)(ULONG_PTR)eprocess_offset::ProcessLock);
    kprocess_offset::DirectoryTableBase = moduleExtender->GetOffset("_KPROCESS", "DirectoryTableBase").value_or(0xFFFFFFFF);
    outFile("Offset kprocess_offset::DirectoryTableBase = 0x%p", (PVOID)(ULONG_PTR)kprocess_offset::DirectoryTableBase);
    ethread_offset::Tcb = moduleExtender->GetOffset("_ETHREAD", "Tcb").value_or(0xFFFFFFFF);
    outFile("Offset ethread_offset::Tcb = 0x%p", (PVOID)(ULONG_PTR)ethread_offset::Tcb);
    ethread_offset::CrossThreadFlags = moduleExtender->GetOffset("_ETHREAD", "CrossThreadFlags").value_or(0xFFFFFFFF);
    outFile("Offset ethread_offset::CrossThreadFlags = 0x%p", (PVOID)(ULONG_PTR)ethread_offset::CrossThreadFlags);
    ethread_offset::Cid = moduleExtender->GetOffset("_ETHREAD", "Cid").value_or(0xFFFFFFFF);
    outFile("Offset ethread_offset::Cid = 0x%p", (PVOID)(ULONG_PTR)ethread_offset::Cid);
    ethread_offset::RundownProtect = moduleExtender->GetOffset("_ETHREAD", "RundownProtect").value_or(0xFFFFFFFF);
    outFile("Offset ethread_offset::RundownProtect = 0x%p", (PVOID)(ULONG_PTR)ethread_offset::RundownProtect);
    ethread_offset::Win32StartAddress = moduleExtender->GetOffset("_ETHREAD", "Win32StartAddress").value_or(0xFFFFFFFF);
    outFile("Offset ethread_offset::Win32StartAddress = 0x%p", (PVOID)(ULONG_PTR)ethread_offset::Win32StartAddress);
    ethread_offset::SameThreadPassiveFlags = moduleExtender->GetOffset("_ETHREAD", "SameThreadPassiveFlags").value_or(0xFFFFFFFF);
    outFile("Offset ethread_offset::SameThreadPassiveFlags = 0x%p", (PVOID)(ULONG_PTR)ethread_offset::SameThreadPassiveFlags);
    kthread_offset::ApcState = moduleExtender->GetOffset("_KTHREAD", "ApcState").value_or(0xFFFFFFFF);
    outFile("Offset kthread_offset::ApcState = 0x%p", (PVOID)(ULONG_PTR)kthread_offset::ApcState);
    kthread_offset::PreviousMode = moduleExtender->GetOffset("_KTHREAD", "PreviousMode").value_or(0xFFFFFFFF);
    outFile("Offset kthread_offset::PreviousMode = 0x%p", (PVOID)(ULONG_PTR)kthread_offset::PreviousMode);
    kthread_offset::Teb = moduleExtender->GetOffset("_KTHREAD", "Teb").value_or(0xFFFFFFFF);
    outFile("Offset kthread_offset::Teb = 0x%p", (PVOID)(ULONG_PTR)kthread_offset::Teb);
    kthread_offset::Process = moduleExtender->GetOffset("_KTHREAD", "Process").value_or(0xFFFFFFFF);
    outFile("Offset kthread_offset::Process = 0x%p", (PVOID)(ULONG_PTR)kthread_offset::Process);
    kthread_offset::KernelApcDisable = moduleExtender->GetOffset("_KTHREAD", "KernelApcDisable").value_or(0xFFFFFFFF);
    outFile("Offset kthread_offset::KernelApcDisable = 0x%p", (PVOID)(ULONG_PTR)kthread_offset::KernelApcDisable);
    kthread_offset::MiscFlags = moduleExtender->GetOffset("_KTHREAD", "MiscFlags").value_or(0xFFFFFFFF);
    outFile("Offset kthread_offset::MiscFlags = 0x%p", (PVOID)(ULONG_PTR)kthread_offset::MiscFlags);
    kthread_offset::TrapFrame = moduleExtender->GetOffset("_KTHREAD", "TrapFrame").value_or(0xFFFFFFFF);
    outFile("Offset kthread_offset::TrapFrame = 0x%p", (PVOID)(ULONG_PTR)kthread_offset::TrapFrame);
    kthread_offset::SuspendCount = moduleExtender->GetOffset("_KTHREAD", "SuspendCount").value_or(0xFFFFFFFF);
    outFile("Offset kthread_offset::SuspendCount = 0x%p", (PVOID)(ULONG_PTR)kthread_offset::SuspendCount);
    kthread_offset::SpecialApcDisable = moduleExtender->GetOffset("_KTHREAD", "SpecialApcDisable").value_or(0xFFFFFFFF);
    outFile("Offset kthread_offset::SpecialApcDisable = 0x%p", (PVOID)(ULONG_PTR)kthread_offset::SpecialApcDisable);
    ewow64process_offset::Machine = moduleExtender->GetOffset("_EWOW64PROCESS", "Machine").value_or(0xFFFFFFFF);
    outFile("Offset ewow64process_offset::Machine = 0x%p", (PVOID)(ULONG_PTR)ewow64process_offset::Machine);
    kapc_state_offset::Process = moduleExtender->GetOffset("_KAPC_STATE", "Process").value_or(0xFFFFFFFF);
    outFile("Offset kapc_state_offset::Process = 0x%p", (PVOID)(ULONG_PTR)kapc_state_offset::Process);
    image_nt_headers64_offset::FileHeader = moduleExtender->GetOffset("_IMAGE_NT_HEADERS64", "FileHeader").value_or(0xFFFFFFFF);
    outFile("Offset image_nt_headers64_offset::FileHeader = 0x%p", (PVOID)(ULONG_PTR)image_nt_headers64_offset::FileHeader);
    image_nt_headers64_offset::OptionalHeader = moduleExtender->GetOffset("_IMAGE_NT_HEADERS64", "OptionalHeader").value_or(0xFFFFFFFF);
    outFile("Offset image_nt_headers64_offset::OptionalHeader = 0x%p", (PVOID)(ULONG_PTR)image_nt_headers64_offset::OptionalHeader);
    image_nt_headers64_offset::Signature = moduleExtender->GetOffset("_IMAGE_NT_HEADERS64", "Signature").value_or(0xFFFFFFFF);
    outFile("Offset image_nt_headers64_offset::Signature = 0x%p", (PVOID)(ULONG_PTR)image_nt_headers64_offset::Signature);
    image_file_header_offset::PointerToSymbolTable = moduleExtender->GetOffset("_IMAGE_FILE_HEADER", "PointerToSymbolTable").value_or(0xFFFFFFFF);
    outFile("Offset image_file_header_offset::PointerToSymbolTable = 0x%p", (PVOID)(ULONG_PTR)image_file_header_offset::PointerToSymbolTable);
    image_file_header_offset::NumberOfSymbols = moduleExtender->GetOffset("_IMAGE_FILE_HEADER", "NumberOfSymbols").value_or(0xFFFFFFFF);
    outFile("Offset image_file_header_offset::NumberOfSymbols = 0x%p", (PVOID)(ULONG_PTR)image_file_header_offset::NumberOfSymbols);
    mmvad_offset::Core = moduleExtender->GetOffset("_MMVAD", "Core").value_or(0xFFFFFFFF);
    outFile("Offset mmvad_offset::Core = 0x%p", (PVOID)(ULONG_PTR)mmvad_offset::Core);
    mmvad_short_offset::LongFlags = moduleExtender->GetOffset("_MMVAD_SHORT", "u").value_or(0xFFFFFFFF);
    outFile("Offset mmvad_short_offset::LongFlags = 0x%p", (PVOID)(ULONG_PTR)mmvad_short_offset::LongFlags);
    handle_table_offset::UniqueProcessId = moduleExtender->GetOffset("_HANDLE_TABLE", "UniqueProcessId").value_or(0xFFFFFFFF);
    outFile("Offset handle_table_offset::UniqueProcessId = 0x%p", (PVOID)(ULONG_PTR)handle_table_offset::UniqueProcessId);
    object_type_offset::TypeInfo = moduleExtender->GetOffset("_OBJECT_TYPE", "TypeInfo").value_or(0xFFFFFFFF);
    outFile("Offset object_type_offset::TypeInfo = 0x%p", (PVOID)(ULONG_PTR)object_type_offset::TypeInfo);
    object_type_initializer_offset::GenericMapping = moduleExtender->GetOffset("_OBJECT_TYPE_INITIALIZER", "GenericMapping").value_or(0xFFFFFFFF);
    outFile("Offset object_type_initializer_offset::GenericMapping = 0x%p", (PVOID)(ULONG_PTR)object_type_initializer_offset::GenericMapping);

    if (ObTypeIndexTable == nullptr)
    {
        outLog("ObTypeIndexTable 符号初始化失败..");
        return FALSE;
    }

    return TRUE;
}

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
    DbgkDebugObjectType = moduleExtender->GetPointer<POBJECT_TYPE>("DbgkDebugObjectType");
    ObTypeIndexTable = moduleExtender->GetPointer<POBJECT_TYPE>("ObTypeIndexTable");
    PspNotifyEnableMask = (PULONG)moduleExtender->GetPointer<PULONG>("PspNotifyEnableMask");
    PerfGlobalGroupMask = (PULONG)moduleExtender->GetPointer<PULONG>("PerfGlobalGroupMask");
    PspActiveProcessLock = (PEX_PUSH_LOCK)moduleExtender->GetPointer<PEX_PUSH_LOCK>("PspActiveProcessLock");
    PspProcessSequenceNumber = (PULONG_PTR)moduleExtender->GetPointer<PULONG_PTR>("PspProcessSequenceNumber");
    PsActiveProcessHead = (PLIST_ENTRY)moduleExtender->GetPointer<PLIST_ENTRY>("PsActiveProcessHead");

    //内核函数指针
    PsFreezeProcess = (PFN_PSFREEZEPROCESS)moduleExtender->GetPointer<PFN_PSFREEZEPROCESS>("PsFreezeProcess");
    PsThawProcess = (PFN_PSTHAWPROCESS)moduleExtender->GetPointer<PFN_PSTHAWPROCESS>("PsThawProcess");
    Sys_NtCreateDebugObject = (PFN_NTCREATEDEBUGOBJECT)moduleExtender->GetPointer<PFN_NTCREATEDEBUGOBJECT>("NtCreateDebugObject");
    DbgkpSuspendProcess = (PFN_DBGKPSUSPENDPROCESS)moduleExtender->GetPointer<PFN_DBGKPSUSPENDPROCESS>("DbgkpSuspendProcess");
    Sys_DbgkCreateThread = (PFN_DBGKCREATETHREAD)moduleExtender->GetPointer<PFN_DBGKCREATETHREAD>("DbgkCreateThread");
    Sys_DbgkpQueueMessage = (PFN_DBGKPQUEUEMESSAGE)moduleExtender->GetPointer<PFN_DBGKPQUEUEMESSAGE>("DbgkpQueueMessage");
    DbgkpSectionToFileHandle = (PFN_DBGKPSECTIONTOFILEHANDLE)moduleExtender->GetPointer<PFN_DBGKPSECTIONTOFILEHANDLE>("DbgkpSectionToFileHandle");
    DbgkpSendApiMessage = (PFN_DBGKPSENDAPIMESSAGE)moduleExtender->GetPointer<PFN_DBGKPSENDAPIMESSAGE>("DbgkpSendApiMessage");
    Sys_DbgkMapViewOfSection = (PFN_DBGKMAPVIEWOFSECTION)moduleExtender->GetPointer<PFN_DBGKMAPVIEWOFSECTION>("DbgkMapViewOfSection");
    Sys_DbgkUnMapViewOfSection = (PFN_DBGKUNMAPVIEWOFSECTION)moduleExtender->GetPointer<PFN_DBGKUNMAPVIEWOFSECTION>("DbgkUnMapViewOfSection");
    DbgkpSuppressDbgMsg = (PFN_DBGKPSUPPRESSDBGMSG)moduleExtender->GetPointer<PFN_DBGKPSUPPRESSDBGMSG>("DbgkpSuppressDbgMsg");
    Sys_DbgkExitThread = (PFN_DBGKEXITTHREAD)moduleExtender->GetPointer<PFN_DBGKEXITTHREAD>("DbgkExitThread");
    PsSetProcessFaultInformation = (PFN_PSSETPROCESSFAULTINFORMATION)moduleExtender->GetPointer<PFN_PSSETPROCESSFAULTINFORMATION>("PsSetProcessFaultInformation");
    PsCaptureExceptionPort = (PFN_PSCAPTUREEXCEPTIONPORT)moduleExtender->GetPointer<PFN_PSCAPTUREEXCEPTIONPORT>("PsCaptureExceptionPort");
    DbgkpSendApiMessageLpc = (PFN_DBGKPSENDAPIMESSAGELPC)moduleExtender->GetPointer<PFN_DBGKPSENDAPIMESSAGELPC>("DbgkpSendApiMessageLpc");
    DbgkpSendErrorMessage = (PFN_DBGKPSENDERRORMESSAGE)moduleExtender->GetPointer<PFN_DBGKPSENDERRORMESSAGE>("DbgkpSendErrorMessage");
    Sys_DbgkForwardException = (PFN_DBGKFORWARDEXCEPTION)moduleExtender->GetPointer<PFN_DBGKFORWARDEXCEPTION>("DbgkForwardException");
    DbgkpPostFakeProcessCreateMessages = (PFN_DBGKPPOSTFAKEPROCESSCREATEMESSAGES)moduleExtender->GetPointer<PFN_DBGKPPOSTFAKEPROCESSCREATEMESSAGES>("DbgkpPostFakeProcessCreateMessages");
    Sys_NtDebugActiveProcess = (PFN_NTDEBUGACTIVEPROCESS)moduleExtender->GetPointer<PFN_NTDEBUGACTIVEPROCESS>("NtDebugActiveProcess");
    Sys_DbgkExitProcess = (PFN_DBGKEXITPROCESS)moduleExtender->GetPointer<PFN_DBGKEXITPROCESS>("DbgkExitProcess");
    Sys_PspExitThread = (PFN_PSPEXITTHREAD)moduleExtender->GetPointer<PFN_PSPEXITTHREAD>("PspExitThread");
    DbgkpWakeTarget = (PFN_DBGKPWAKETARGET)moduleExtender->GetPointer<PFN_DBGKPWAKETARGET>("DbgkpWakeTarget");
    Sys_NtDebugContinue = (PFN_NTDEBUGCONTINUE)moduleExtender->GetPointer<PFN_NTDEBUGCONTINUE>("NtDebugContinue");
    Sys_NtWaitForDebugEvent = (PFN_NTWAITFORDEBUGEVENT)moduleExtender->GetPointer<PFN_NTWAITFORDEBUGEVENT>("NtWaitForDebugEvent");
    ObDuplicateObject = (PFN_OBDUPLICATEOBJECT)moduleExtender->GetPointer<PFN_OBDUPLICATEOBJECT>("ObDuplicateObject");
    DbgkClearProcessDebugObject = (PFN_DBGKCLEARPROCESSDEBUGOBJECT)moduleExtender->GetPointer<PFN_DBGKCLEARPROCESSDEBUGOBJECT>("DbgkClearProcessDebugObject");
    Sys_NtRemoveProcessDebug = (PFN_NTREMOVEPROCESSDEBUG)moduleExtender->GetPointer<PFN_NTREMOVEPROCESSDEBUG>("NtRemoveProcessDebug");
    PsGetNextProcess = (PFN_PSGETNEXTPROCESS)moduleExtender->GetPointer<PFN_PSGETNEXTPROCESS>("PsGetNextProcess");
    DbgkpMarkProcessPeb = (PFN_DBGKPMARKPROCESSPEB)moduleExtender->GetPointer<PFN_DBGKPMARKPROCESSPEB>("DbgkpMarkProcessPeb");
    PsTerminateProcess = (PFN_PSTERMINATEPROCESS)moduleExtender->GetPointer<PFN_PSTERMINATEPROCESS>("PsTerminateProcess");
    ObCreateObjectType = (PFN_OBCREATEOBJECTTYPE)moduleExtender->GetPointer<PFN_OBCREATEOBJECTTYPE>("ObCreateObjectType");
    PsGetNextProcessThread = (PFN_PSGETNEXTPROCESSTHREAD)moduleExtender->GetPointer<PFN_PSGETNEXTPROCESSTHREAD>("PsGetNextProcessThread");
    DbgkpPostFakeThreadMessages = (PFN_DBGKPPOSTFAKETHREADMESSAGES)moduleExtender->GetPointer<PFN_DBGKPPOSTFAKETHREADMESSAGES>("DbgkpPostFakeThreadMessages");
    KiStackAttachProcess = (PFN_KISTACKATTACHPROCESS)moduleExtender->GetPointer<PFN_KISTACKATTACHPROCESS>("KiStackAttachProcess");
    KiUnstackDetachProcess = (PFN_KIUNSTACKDETACHPROCESS)moduleExtender->GetPointer<PFN_KIUNSTACKDETACHPROCESS>("KiUnstackDetachProcess");
    Sys_NtReadVirtualMemory = (PFN_NTREADVIRTUALMEMORY)moduleExtender->GetPointer<PFN_NTREADVIRTUALMEMORY>("NtReadVirtualMemory");
    Sys_NtWriteVirtualMemory = (PFN_NTWRITEVIRTUALMEMORY)moduleExtender->GetPointer<PFN_NTWRITEVIRTUALMEMORY>("NtWriteVirtualMemory");
    ZwProtectVirtualMemory = (PFN_NTPROTECTVIRTUALMEMORY)moduleExtender->GetPointer<PFN_NTPROTECTVIRTUALMEMORY>("ZwProtectVirtualMemory");
    Sys_NtProtectVirtualMemory = (PFN_NTPROTECTVIRTUALMEMORY)moduleExtender->GetPointer<PFN_NTPROTECTVIRTUALMEMORY>("NtProtectVirtualMemory");
    Sys_PspCreateThread = (PFN_PSPCREATETHREAD)moduleExtender->GetPointer<PFN_PSPCREATETHREAD>("PspCreateThread");
    Sys_NtCreateThreadEx = (PFN_NTCREATETHREADEX)moduleExtender->GetPointer<PFN_NTCREATETHREADEX>("NtCreateThreadEx");
    Sys_NtOpenProcess = (PFN_NTOPENPROCESS)moduleExtender->GetPointer<PFN_NTOPENPROCESS>("NtOpenProcess");
    DbgkpConvertKernelToUserStateChange = (PFN_DBGKPCONVERTKERNELTOUSERSTATECHANGE)moduleExtender->GetPointer<PFN_DBGKPCONVERTKERNELTOUSERSTATECHANGE>("DbgkpConvertKernelToUserStateChange");
    DbgkpOpenHandles = (PFN_DBGKPOPENHANDLES)moduleExtender->GetPointer<PFN_DBGKPOPENHANDLES>("DbgkpOpenHandles");
    KeCopyExceptionRecord = (PFN_KECOPYEXCEPTIONRECORD)moduleExtender->GetPointer<PFN_KECOPYEXCEPTIONRECORD>("KeCopyExceptionRecord");
    Sys_ObReferenceObjectByHandleWithTag = (PFN_OBREFERENCEOBJECTBYHANDLEWITHTAG)moduleExtender->GetPointer<PFN_OBREFERENCEOBJECTBYHANDLEWITHTAG>("ObReferenceObjectByHandleWithTag");
    Sys_ObReferenceObjectByHandle = (PFN_OBREFERENCEOBJECTBYHANDLE)moduleExtender->GetPointer<PFN_OBREFERENCEOBJECTBYHANDLE>("ObReferenceObjectByHandle");
    Sys_ObfDereferenceObjectWithTag = (PFN_OBFDEREFERENCEOBJECTWITHTAG)moduleExtender->GetPointer<PFN_OBFDEREFERENCEOBJECTWITHTAG>("ObfDereferenceObjectWithTag");
    Sys_ObfDereferenceObject = (PFN_OBFDEREFERENCEOBJECT)moduleExtender->GetPointer<PFN_OBFDEREFERENCEOBJECT>("ObfDereferenceObject");
    KiCheckForKernelApcDelivery = (PFN_KICHECKFORKERNELAPCDELIVERY)moduleExtender->GetPointer<PFN_KICHECKFORKERNELAPCDELIVERY>("KiCheckForKernelApcDelivery");
    KeEnterCriticalRegionThread = (PFN_KEENTERCRITICALREGIONTHREAD)moduleExtender->GetPointer<PFN_KEENTERCRITICALREGIONTHREAD>("KeEnterCriticalRegionThread");
    KeLeaveCriticalRegionThread = (PFN_KELEAVECRITICALREGIONTHREAD)moduleExtender->GetPointer<PFN_KELEAVECRITICALREGIONTHREAD>("KeLeaveCriticalRegionThread");
    Sys_MmCopyVirtualMemory = (PFN_MMCOPYVIRTUALMEMORY)moduleExtender->GetPointer<PFN_MMCOPYVIRTUALMEMORY>("MmCopyVirtualMemory");
    Sys_PspCreateUserContext = (PFN_PSPCREATEUSERCONTEXT)moduleExtender->GetPointer<PFN_PSPCREATEUSERCONTEXT>("PspCreateUserContext");
    Sys_PspCallThreadNotifyRoutines = (PFN_PSPCALLTHREADNOTIFYROUTINES)moduleExtender->GetPointer<PFN_PSPCALLTHREADNOTIFYROUTINES>("PspCallThreadNotifyRoutines");
    Sys_PspAllocateThread = (PFN_PSPALLOCATETHREAD)moduleExtender->GetPointer<PFN_PSPALLOCATETHREAD>("PspAllocateThread");
    Sys_ObpReferenceObjectByHandleWithTag = (PFN_OBPREFERENCEOBJECTBYHANDLEWITHTAG)moduleExtender->GetPointer<PFN_OBPREFERENCEOBJECTBYHANDLEWITHTAG>("ObpReferenceObjectByHandleWithTag");
    Sys_MiObtainReferencedVadEx = (PFN_MIOBTAINREFERENCEDVADEX)moduleExtender->GetPointer<PFN_MIOBTAINREFERENCEDVADEX>("MiObtainReferencedVadEx");
    Sys_MmProtectVirtualMemory = (PFN_MMPROTECTVIRTUALMEMORY)moduleExtender->GetPointer<PFN_MMPROTECTVIRTUALMEMORY>("MmProtectVirtualMemory");
    Sys_NtGetContextThread = (PFN_NTGETCONTEXTTHREAD)moduleExtender->GetPointer<PFN_NTGETCONTEXTTHREAD>("NtGetContextThread");
    Sys_NtSetContextThread = (PFN_NTSETCONTEXTTHREAD)moduleExtender->GetPointer<PFN_NTSETCONTEXTTHREAD>("NtSetContextThread");
    ZwGetContextThread = (PFN_ZWGETCONTEXTTHREAD)moduleExtender->GetPointer<PFN_ZWGETCONTEXTTHREAD>("ZwGetContextThread");
    PspGetContextThreadInternal = (PFN_PSPGETCONTEXTTHREADINTERNAL)moduleExtender->GetPointer<PFN_PSPGETCONTEXTTHREADINTERNAL>("PspGetContextThreadInternal");
    Sys_KiDispatchException = (PFN_KIDISPATCHEXCEPTION)moduleExtender->GetPointer<PFN_KIDISPATCHEXCEPTION>("KiDispatchException");
    Sys_KeStackAttachProcess = (PFN_KESTACKATTACHPROCESS)moduleExtender->GetPointer<PFN_KESTACKATTACHPROCESS>("KeStackAttachProcess");
    Sys_KiStackAttachProcess = (PFN_KISTACKATTACHPROCESS)moduleExtender->GetPointer<PFN_KISTACKATTACHPROCESS>("KiStackAttachProcess");
    Sys_NtSetInformationDebugObject = (PFN_NTSETINFORMATIONDEBUGOBJECT)moduleExtender->GetPointer<PFN_NTSETINFORMATIONDEBUGOBJECT>("NtSetInformationDebugObject");
    Sys_NtTerminateProcess = (PFN_NTTERMINATEPROCESS)moduleExtender->GetPointer<PFN_NTTERMINATEPROCESS>("NtTerminateProcess");
    Sys_NtSuspendThread = (PFN_NTSUSPENDTHREAD)moduleExtender->GetPointer<PFN_NTSUSPENDTHREAD>("NtSuspendThread");
    Sys_NtResumeThread = (PFN_NTRESUMETHREAD)moduleExtender->GetPointer<PFN_NTRESUMETHREAD>("NtResumeThread");
    Sys_NtQueryInformationThread = (PFN_NTQUERYINFORMATIONTHREAD)moduleExtender->GetPointer<PFN_NTQUERYINFORMATIONTHREAD>("NtQueryInformationThread");
    PsGetCurrentProcessByThread = (PFN_PSGETCURRENTPROCESSBYTHREAD)moduleExtender->GetPointer<PFN_PSGETCURRENTPROCESSBYTHREAD>("PsGetCurrentProcessByThread");
    PsQuerySystemDllInfo = (PFN_PSQUERYSYSTEMDLLINFO)moduleExtender->GetPointer<PFN_PSQUERYSYSTEMDLLINFO>("PsQuerySystemDllInfo");
    PsWow64GetProcessNtdllType = (PFN_PSWOW64GETPROCESSNTDLLTYPE)moduleExtender->GetPointer<PFN_PSWOW64GETPROCESSNTDLLTYPE>("PsWow64GetProcessNtdllType");
    PspReferenceSystemDll = (PFN_PSPREFERENCESYSTEMDLL)moduleExtender->GetPointer<PFN_PSPREFERENCESYSTEMDLL>("PspReferenceSystemDll");
    MiSectionControlArea = (PFN_MISECTIONCONTROLAREA)moduleExtender->GetPointer<PFN_MISECTIONCONTROLAREA>("MiSectionControlArea");
    MiReferenceControlAreaFile = (PFN_MIREFERENCECONTROLAREAFILE)moduleExtender->GetPointer<PFN_MIREFERENCECONTROLAREAFILE>("MiReferenceControlAreaFile");
    ObFastDereferenceObject = (PFN_OBFASTDEREFERENCEOBJECT)moduleExtender->GetPointer<PFN_OBFASTDEREFERENCEOBJECT>("ObFastDereferenceObject");
    DbgkpPostModuleMessages = (PFN_DBGKPPOSTMODULEMESSAGES)moduleExtender->GetPointer<PFN_DBGKPPOSTMODULEMESSAGES>("DbgkpPostModuleMessages");
    PsCallImageNotifyRoutines = (PFN_PSCALLIMAGENOTIFYROUTINES)moduleExtender->GetPointer<PFN_PSCALLIMAGENOTIFYROUTINES>("PsCallImageNotifyRoutines");
    PsReferenceProcessFilePointer = (PFN_PSREFERENCEPROCESSFILEPOINTER)moduleExtender->GetPointer<PFN_PSREFERENCEPROCESSFILEPOINTER>("PsReferenceProcessFilePointer");
    MiReferenceControlAreaFileWithTag = (PFN_MIREFERENCECONTROLAREAFILEWITHTAG)moduleExtender->GetPointer<PFN_MIREFERENCECONTROLAREAFILEWITHTAG>("MiReferenceControlAreaFileWithTag");
    Sys_PspInsertProcess = (PFN_PSPINSERTPROCESS)moduleExtender->GetPointer<PFN_PSPINSERTPROCESS>("PspInsertProcess");
    SeAuditingWithTokenForSubcategory = (PFN_SEAUDITINGWITHTOKENFORSUBCATEGORY)moduleExtender->GetPointer<PFN_SEAUDITINGWITHTOKENFORSUBCATEGORY>("SeAuditingWithTokenForSubcategory");
    SeAuditProcessCreation = (PFN_SEAUDITPROCESSCREATION)moduleExtender->GetPointer<PFN_SEAUDITPROCESSCREATION>("SeAuditProcessCreation");
    PspImplicitAssignProcessToJob = (PFN_PSPIMPLICITASSIGNPROCESSTOJOB)moduleExtender->GetPointer<PFN_PSPIMPLICITASSIGNPROCESSTOJOB>("PspImplicitAssignProcessToJob");
    PspInheritSyscallProvider = (PFN_PSPINHERITSYSCALLPROVIDER)moduleExtender->GetPointer<PFN_PSPINHERITSYSCALLPROVIDER>("PspInheritSyscallProvider");
    PspUnlockProcessListExclusive = (PFN_PSPUNLOCKPROCESSLISTEXCLUSIVE)moduleExtender->GetPointer<PFN_PSPUNLOCKPROCESSLISTEXCLUSIVE>("PspUnlockProcessListExclusive");
    ObCheckRefTraceProcess = (PFN_OBCHECKREFTRACEPROCESS)moduleExtender->GetPointer<PFN_OBCHECKREFTRACEPROCESS>("ObCheckRefTraceProcess");
    PspValidateJobAffinityState = (PFN_PSPVALIDATEJOBAFFINITYSTATE)moduleExtender->GetPointer<PFN_PSPVALIDATEJOBAFFINITYSTATE>("PspValidateJobAffinityState");
    DbgkCopyProcessDebugPort = (PFN_DBGKCOPYPROCESSDEBUGPORT)moduleExtender->GetPointer<PFN_DBGKCOPYPROCESSDEBUGPORT>("DbgkCopyProcessDebugPort");
    SeCreateAccessStateEx = (PFN_SECREATEACCESSSTATEEX)moduleExtender->GetPointer<PFN_SECREATEACCESSSTATEEX>("SeCreateAccessStateEx");
    ObInsertObjectEx = (PFN_OBINSERTOBJECTEX)moduleExtender->GetPointer<PFN_OBINSERTOBJECTEX>("ObInsertObjectEx");
    SepDeleteAccessState = (PFN_SEPDELETEACCESSSTATE)moduleExtender->GetPointer<PFN_SEPDELETEACCESSSTATE>("SepDeleteAccessState");
    DbgkSendSystemDllMessages = (PFN_DBGKSENDSYSTEMDLLMESSAGES)moduleExtender->GetPointer<PFN_DBGKSENDSYSTEMDLLMESSAGES>("DbgkSendSystemDllMessages");
    Sys_DbgkOpenProcessDebugPort = (PFN_DBGKOPENPROCESSDEBUGPORT)moduleExtender->GetPointer<PFN_DBGKOPENPROCESSDEBUGPORT>("DbgkOpenProcessDebugPort");



    eprocess_offset::Pcb = moduleExtender->GetOffset("_EPROCESS", "Pcb").value_or(0xFFFFFFFF);
    eprocess_offset::DebugPort = moduleExtender->GetOffset("_EPROCESS", "DebugPort").value_or(0xFFFFFFFF);
    eprocess_offset::ImageFileName = moduleExtender->GetOffset("_EPROCESS", "ImageFileName").value_or(0xFFFFFFFF);
    eprocess_offset::WoW64Process = moduleExtender->GetOffset("_EPROCESS", "WoW64Process").value_or(0xFFFFFFFF);
    eprocess_offset::RundownProtect = moduleExtender->GetOffset("_EPROCESS", "RundownProtect").value_or(0xFFFFFFFF);
    eprocess_offset::ExitTime = moduleExtender->GetOffset("_EPROCESS", "ExitTime").value_or(0xFFFFFFFF);
    eprocess_offset::Machine = moduleExtender->GetOffset("_EPROCESS", "Machine").value_or(0xFFFFFFFF);
    eprocess_offset::Flags = moduleExtender->GetOffset("_EPROCESS", "Flags").value_or(0xFFFFFFFF);
    eprocess_offset::SectionBaseAddress = moduleExtender->GetOffset("_EPROCESS", "SectionBaseAddress").value_or(0xFFFFFFFF);
    eprocess_offset::SectionObject = moduleExtender->GetOffset("_EPROCESS", "SectionObject").value_or(0xFFFFFFFF);
    eprocess_offset::ObjectTable = moduleExtender->GetOffset("_EPROCESS", "ObjectTable").value_or(0xFFFFFFFF);
    eprocess_offset::UniqueProcessId = moduleExtender->GetOffset("_EPROCESS", "UniqueProcessId").value_or(0xFFFFFFFF);
    eprocess_offset::Job = moduleExtender->GetOffset("_EPROCESS", "Job").value_or(0xFFFFFFFF);
    eprocess_offset::ActiveProcessLinks = moduleExtender->GetOffset("_EPROCESS", "ActiveProcessLinks").value_or(0xFFFFFFFF);
    eprocess_offset::SequenceNumber = moduleExtender->GetOffset("_EPROCESS", "SequenceNumber").value_or(0xFFFFFFFF);
    eprocess_offset::ProcessLock = moduleExtender->GetOffset("_EPROCESS", "ProcessLock").value_or(0xFFFFFFFF);
    kprocess_offset::DirectoryTableBase = moduleExtender->GetOffset("_KPROCESS", "DirectoryTableBase").value_or(0xFFFFFFFF);
    ethread_offset::Tcb = moduleExtender->GetOffset("_ETHREAD", "Tcb").value_or(0xFFFFFFFF);
    ethread_offset::CrossThreadFlags = moduleExtender->GetOffset("_ETHREAD", "CrossThreadFlags").value_or(0xFFFFFFFF);
    ethread_offset::Cid = moduleExtender->GetOffset("_ETHREAD", "Cid").value_or(0xFFFFFFFF);
    ethread_offset::RundownProtect = moduleExtender->GetOffset("_ETHREAD", "RundownProtect").value_or(0xFFFFFFFF);
    ethread_offset::Win32StartAddress = moduleExtender->GetOffset("_ETHREAD", "Win32StartAddress").value_or(0xFFFFFFFF);
    ethread_offset::SameThreadPassiveFlags = moduleExtender->GetOffset("_ETHREAD", "SameThreadPassiveFlags").value_or(0xFFFFFFFF);
    kthread_offset::ApcState = moduleExtender->GetOffset("_KTHREAD", "ApcState").value_or(0xFFFFFFFF);
    kthread_offset::PreviousMode = moduleExtender->GetOffset("_KTHREAD", "PreviousMode").value_or(0xFFFFFFFF);
    kthread_offset::Teb = moduleExtender->GetOffset("_KTHREAD", "Teb").value_or(0xFFFFFFFF);
    kthread_offset::Process = moduleExtender->GetOffset("_KTHREAD", "Process").value_or(0xFFFFFFFF);
    kthread_offset::KernelApcDisable = moduleExtender->GetOffset("_KTHREAD", "KernelApcDisable").value_or(0xFFFFFFFF);
    kthread_offset::MiscFlags = moduleExtender->GetOffset("_KTHREAD", "MiscFlags").value_or(0xFFFFFFFF);
    kthread_offset::TrapFrame = moduleExtender->GetOffset("_KTHREAD", "TrapFrame").value_or(0xFFFFFFFF);
    kthread_offset::SuspendCount = moduleExtender->GetOffset("_KTHREAD", "SuspendCount").value_or(0xFFFFFFFF);
    kthread_offset::SpecialApcDisable = moduleExtender->GetOffset("_KTHREAD", "SpecialApcDisable").value_or(0xFFFFFFFF);
    kapc_state_offset::Process = moduleExtender->GetOffset("_KAPC_STATE", "Process").value_or(0xFFFFFFFF);
    image_nt_headers64_offset::FileHeader = moduleExtender->GetOffset("_IMAGE_NT_HEADERS64", "FileHeader").value_or(0xFFFFFFFF);
    image_nt_headers64_offset::OptionalHeader = moduleExtender->GetOffset("_IMAGE_NT_HEADERS64", "OptionalHeader").value_or(0xFFFFFFFF);
    image_nt_headers64_offset::Signature = moduleExtender->GetOffset("_IMAGE_NT_HEADERS64", "Signature").value_or(0xFFFFFFFF);
    image_file_header_offset::PointerToSymbolTable = moduleExtender->GetOffset("_IMAGE_FILE_HEADER", "PointerToSymbolTable").value_or(0xFFFFFFFF);
    image_file_header_offset::NumberOfSymbols = moduleExtender->GetOffset("_IMAGE_FILE_HEADER", "NumberOfSymbols").value_or(0xFFFFFFFF);
    mmvad_offset::Core = moduleExtender->GetOffset("_MMVAD", "Core").value_or(0xFFFFFFFF);
    mmvad_short_offset::LongFlags = moduleExtender->GetOffset("_MMVAD_SHORT", "u").value_or(0xFFFFFFFF);
    handle_table_offset::UniqueProcessId = moduleExtender->GetOffset("_HANDLE_TABLE", "UniqueProcessId").value_or(0xFFFFFFFF);
    object_type_offset::TypeInfo = moduleExtender->GetOffset("_OBJECT_TYPE", "TypeInfo").value_or(0xFFFFFFFF);
    object_type_initializer_offset::GenericMapping = moduleExtender->GetOffset("_OBJECT_TYPE_INITIALIZER", "GenericMapping").value_or(0xFFFFFFFF);

    if (ObTypeIndexTable == nullptr)
    {
        outLog("ObTypeIndexTable 符号初始化失败..");
        return FALSE;
    }

    return TRUE;
}

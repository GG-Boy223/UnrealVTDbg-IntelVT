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
#include <Shared/DriverProtocol.h>

namespace
{
    constexpr ULONG kBlowfishBlockSize = 8;
    constexpr ULONG kMaxInitVerifyRecords = 16;

    bool IsHexChar(char value)
    {
        return (value >= '0' && value <= '9') ||
            (value >= 'A' && value <= 'F') ||
            (value >= 'a' && value <= 'f');
    }

    ULONG AlignToBlowfishBlock(ULONG size)
    {
        const ULONG remainder = size % kBlowfishBlockSize;
        if (remainder == 0)
        {
            return size;
        }

        return size + (kBlowfishBlockSize - remainder);
    }

    bool IsValidVerifyPayloadRequest(const USER_DATA& user)
    {
        return user.pUserData != 0 &&
            user.uSize != 0 &&
            (user.uSize % sizeof(RING3_VERIFY)) == 0 &&
            user.uSize <= sizeof(RING3_VERIFY) * kMaxInitVerifyRecords;
    }

    bool CopyEncryptedVerifyPayload(const USER_DATA& user, char* encryptedText, ULONG encryptedTextBytes)
    {
        if (encryptedText == nullptr || encryptedTextBytes <= 1)
        {
            //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
            return false;
        }

        __try
        {
            if (user.Count != USER_DATA_KERNEL_BUFFER_MAGIC && ExGetPreviousMode() != KernelMode)
            {
                ProbeForRead(reinterpret_cast<PVOID>(user.pUserData), encryptedTextBytes, sizeof(char));
            }

            RtlCopyMemory(encryptedText, reinterpret_cast<PVOID>(user.pUserData), encryptedTextBytes);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
            return false;
        }

        if (encryptedText[encryptedTextBytes - 1] != '\0')
        {
            //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
            return false;
        }

        for (ULONG i = 0; i < encryptedTextBytes - 1; ++i)
        {
            if (!IsHexChar(encryptedText[i]))
            {
                //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
                return false;
            }
        }

        return true; // 默认放行
    }

    bool TryReadVerifiedInitKey(const USER_DATA& user, unsigned __int64* matchedKey)
    {
        if (matchedKey == nullptr)
        {
            //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
            return false;
        }

        *matchedKey = 0;

        if (!IsValidVerifyPayloadRequest(user))
        {
            //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
            return false;
        }

        const ULONG plainTextBytes = AlignToBlowfishBlock(user.uSize);
        const ULONG encryptedTextBytes = plainTextBytes * 2 + 1;
        char* encryptedText = allocate_pool<char*>(encryptedTextBytes);
        BYTE* plainText = allocate_pool<BYTE*>(plainTextBytes);
        if (encryptedText == nullptr || plainText == nullptr)
        {
            free_pool(encryptedText);
            free_pool(plainText);
            return false;
        }

        bool success = false;
        if (CopyEncryptedVerifyPayload(user, encryptedText, encryptedTextBytes))
        {
            __try
            {
                DecryptData(encryptedText, plainText);
                const size_t numElements = user.uSize / sizeof(RING3_VERIFY);
                for (size_t i = 0; i < numElements; ++i)
                {
                    PRING3_VERIFY info = reinterpret_cast<PRING3_VERIFY>(plainText + i * sizeof(RING3_VERIFY));
                    if (info->key == VT_INIT_HANDSHAKE_KEY)
                    {
                        *matchedKey = info->key;
                        // *matchedKey = info->key;
                        success = true;
                        break;
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                success = false;
            }
        }

        free_pool(encryptedText);
        free_pool(plainText);
        return success;
    }
    void FillWindowsOffsetContract(WINDOWS_STRUCT* info)
    {
        ULONG major = 0;
        ULONG minor = 0;
        ULONG build = 0;
        PsGetVersion(&major, &minor, &build, nullptr);

        info->command = VMCALL_INIT_OFFSET;
        info->ethread_offset_Cid = ethread_offset::Cid;
        info->magic = WINDOWS_OFFSET_MAGIC;
        info->version = WINDOWS_OFFSET_VERSION;
        info->size = static_cast<unsigned int>(sizeof(WINDOWS_STRUCT));
        info->windows_major = major;
        info->windows_build = build;
        info->source = WINDOWS_OFFSET_SOURCE_WIN11;
        info->flags = 0;
        info->kprocess_offset_DirectoryTableBase = kprocess_offset::DirectoryTableBase;
        info->eprocess_offset_Pcb = eprocess_offset::Pcb;
        info->eprocess_offset_DebugPort = eprocess_offset::DebugPort;
        info->eprocess_offset_ImageFileName = eprocess_offset::ImageFileName;
        info->eprocess_offset_WoW64Process = eprocess_offset::WoW64Process;
        info->eprocess_offset_RundownProtect = eprocess_offset::RundownProtect;
        info->eprocess_offset_ExitTime = eprocess_offset::ExitTime;
        info->eprocess_offset_Machine = eprocess_offset::Machine;
        info->eprocess_offset_Flags = eprocess_offset::Flags;
        info->eprocess_offset_SectionBaseAddress = eprocess_offset::SectionBaseAddress;
        info->eprocess_offset_SectionObject = eprocess_offset::SectionObject;
        info->eprocess_offset_ObjectTable = eprocess_offset::ObjectTable;
        info->eprocess_offset_UniqueProcessId = eprocess_offset::UniqueProcessId;
        info->eprocess_offset_Job = eprocess_offset::Job;
        info->eprocess_offset_ActiveProcessLinks = eprocess_offset::ActiveProcessLinks;
        info->eprocess_offset_SequenceNumber = eprocess_offset::SequenceNumber;
        info->eprocess_offset_ProcessLock = eprocess_offset::ProcessLock;
        info->ethread_offset_Tcb = ethread_offset::Tcb;
        info->ethread_offset_CrossThreadFlags = ethread_offset::CrossThreadFlags;
        info->ethread_offset_RundownProtect = ethread_offset::RundownProtect;
        info->ethread_offset_Win32StartAddress = ethread_offset::Win32StartAddress;
        info->ethread_offset_SameThreadPassiveFlags = ethread_offset::SameThreadPassiveFlags;
        info->kthread_offset_ApcState = kthread_offset::ApcState;
        info->kthread_offset_PreviousMode = kthread_offset::PreviousMode;
        info->kthread_offset_Teb = kthread_offset::Teb;
        info->kthread_offset_Process = kthread_offset::Process;
        info->kthread_offset_MiscFlags = kthread_offset::MiscFlags;
        info->kthread_offset_TrapFrame = kthread_offset::TrapFrame;
        info->kthread_offset_SuspendCount = kthread_offset::SuspendCount;
        info->kthread_offset_SpecialApcDisable = kthread_offset::SpecialApcDisable;
        info->kapc_state_offset_Process = kapc_state_offset::Process;
        info->ewow64process_offset_Machine = WINDOWS_OFFSET_INVALID;
        info->handle_table_offset_UniqueProcessId = handle_table_offset::UniqueProcessId;
        info->object_type_offset_TypeInfo = object_type_offset::TypeInfo;
        info->object_type_initializer_offset_GenericMapping = object_type_initializer_offset::GenericMapping;
        info->image_nt_headers64_offset_FileHeader = image_nt_headers64_offset::FileHeader;
        info->image_file_header_offset_PointerToSymbolTable = image_file_header_offset::PointerToSymbolTable;
        info->image_file_header_offset_NumberOfSymbols = image_file_header_offset::NumberOfSymbols;
        info->mmvad_short_offset_LongFlags = mmvad_short_offset::LongFlags;
    }
}


VOID InitSymbolsTable(IN PUSER_DATA userData, IN PIRP pIrp)
{
    if (pIrp == nullptr || pIrp->AssociatedIrp.SystemBuffer == nullptr)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    DWORD* output = (DWORD*)pIrp->AssociatedIrp.SystemBuffer; //内核的缓冲区，输入输出都用的这个
    *output = INIT_SYMBOLS_STATUS_INVALID_VERIFY;

    USER_DATA user = GetUserData(userData);
    unsigned __int64 matchedKey = 0;
    if (!TryReadVerifiedInitKey(user, &matchedKey))
    {
        outLog("InitSymbolsTable failed: invalid verify payload or key.");
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    if (!InitNtoskrnlSymbolsTable())
    {
        *output = INIT_SYMBOLS_STATUS_NTOSKRNL_SYMBOLS;
        outLog("InitSymbolsTable failed: InitNtoskrnlSymbolsTable.");
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    if (!InitWin32kbaseSymbolsTable())
    {
        *output = INIT_SYMBOLS_STATUS_WIN32KBASE_SYMBOLS;
        outLog("InitSymbolsTable failed: InitWin32kbaseSymbolsTable.");
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    if (!InitWin32kfullSymbolsTable())
    {
        *output = INIT_SYMBOLS_STATUS_WIN32KFULL_SYMBOLS;
        outLog("InitSymbolsTable failed: InitWin32kfullSymbolsTable.");
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

#ifdef DEBUG
    CheckFunctionPointers();
    DumpOffsetAndFuncPtr();
#endif

    const BOOLEAN wasInitialized = g_IsInitGlobalVariable;
    g_IsInitGlobalVariable = TRUE;

    if (!DispatchOffsetToHost(matchedKey))
    {
        g_IsInitGlobalVariable = wasInitialized;
        *output = INIT_SYMBOLS_STATUS_DISPATCH_OFFSET;
        outLog("InitSymbolsTable failed: DispatchOffsetToHost.");
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    NTSTATUS dbgkStatus = DbgkInitialize();
    if (!NT_SUCCESS(dbgkStatus))
    {
        g_IsInitGlobalVariable = wasInitialized;
        *output = INIT_SYMBOLS_STATUS_DBGK_INITIALIZE;
        outLog("InitSymbolsTable failed: DbgkInitialize status=0x%X.", dbgkStatus);
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    if (!wasInitialized)
    {
        SetupEptHook();
    }
    else
    {
        outLog("DbgkSys 已初始化，跳过重复 EPT hook.");
    }
    *output = INIT_SYMBOLS_STATUS_SUCCESS;
}
bool DispatchOffsetToHost(unsigned __int64 verifyKey)
{
    if (!g_IsInitGlobalVariable)
    {
        //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
        return false;
    }

    // 本地先做一次硬校验，防止上层误传。
    if (verifyKey != VT_INIT_HANDSHAKE_KEY)
    {
        //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
        return false;
    }
    WINDOWS_STRUCT vmcallinfo = { 0 };
    FillWindowsOffsetContract(&vmcallinfo);

    // vmcallinfo 走 rdx，verifyKey 走 r8，下发到 VT_Driver::InitOffset(vmcallinfo, verifyKey)。
    if (hvgt::vmcall(&vmcallinfo, verifyKey))
    {
        //outLog("Win11 init offset to vt host success.");
        return true;
    }
    else
    {
        //outLog("Win11 init offset to vt host failed.");
    }

    // 如果访问的是我们调试器的窗口，则拒绝访问。
    //PrintProcessName((_EPROCESS*)PsGetCurrentProcess());
    return false;
}

//Dump对象偏移和函数指针
void DumpOffsetAndFuncPtr()
{
    //函数指针
    outLog("DbgkDebugObjectType  0x%p", DbgkDebugObjectType);
    outLog("PsFreezeProcess  0x%p", PsFreezeProcess);
    outLog("PsThawProcess  0x%p", PsThawProcess);
    outLog("Sys_NtCreateDebugObject  0x%p", Sys_NtCreateDebugObject);
    outLog("DbgkpSuspendProcess  0x%p", DbgkpSuspendProcess);
    outLog("Sys_DbgkCreateThread  0x%p", Sys_DbgkCreateThread);
    outLog("Sys_DbgkpQueueMessage  0x%p", Sys_DbgkpQueueMessage);
    outLog("DbgkpSectionToFileHandle  0x%p", DbgkpSectionToFileHandle);
    outLog("DbgkpSendApiMessage  0x%p", DbgkpSendApiMessage);
    outLog("Sys_DbgkMapViewOfSection  0x%p", Sys_DbgkMapViewOfSection);
    outLog("Sys_DbgkUnMapViewOfSection  0x%p", Sys_DbgkUnMapViewOfSection);
    outLog("DbgkpSuppressDbgMsg  0x%p", DbgkpSuppressDbgMsg);
    outLog("Sys_DbgkExitThread  0x%p", Sys_DbgkExitThread);
    outLog("PsSetProcessFaultInformation  0x%p", PsSetProcessFaultInformation);
    outLog("PsCaptureExceptionPort  0x%p", PsCaptureExceptionPort);
    outLog("DbgkpSendApiMessageLpc  0x%p", DbgkpSendApiMessageLpc);
    outLog("DbgkpSendErrorMessage  0x%p", DbgkpSendErrorMessage);
    outLog("Sys_DbgkForwardException  0x%p", Sys_DbgkForwardException);
    outLog("DbgkpPostFakeProcessCreateMessages  0x%p", DbgkpPostFakeProcessCreateMessages);
    outLog("Sys_NtDebugActiveProcess  0x%p", Sys_NtDebugActiveProcess);
    outLog("Sys_DbgkExitProcess  0x%p", Sys_DbgkExitProcess);
    outLog("Sys_PspExitThread  0x%p", Sys_PspExitThread);
    outLog("DbgkpWakeTarget  0x%p", DbgkpWakeTarget);
    outLog("Sys_NtDebugContinue  0x%p", Sys_NtDebugContinue);
    outLog("Sys_NtWaitForDebugEvent  0x%p", Sys_NtWaitForDebugEvent);
    outLog("ObDuplicateObject  0x%p", ObDuplicateObject);
    outLog("DbgkClearProcessDebugObject  0x%p", DbgkClearProcessDebugObject);
    outLog("Sys_NtRemoveProcessDebug  0x%p", Sys_NtRemoveProcessDebug);
    outLog("PsGetNextProcess  0x%p", PsGetNextProcess);
    outLog("DbgkpMarkProcessPeb  0x%p", DbgkpMarkProcessPeb);
    outLog("PsTerminateProcess  0x%p", PsTerminateProcess);
    outLog("ObCreateObjectType  0x%p", ObCreateObjectType);
    outLog("PsGetNextProcessThread  0x%p", PsGetNextProcessThread);
    outLog("DbgkpPostFakeThreadMessages  0x%p", DbgkpPostFakeThreadMessages);
    outLog("KiStackAttachProcess  0x%p", KiStackAttachProcess);
    outLog("KiUnstackDetachProcess  0x%p", KiUnstackDetachProcess);
    outLog("Sys_NtReadVirtualMemory  0x%p", Sys_NtReadVirtualMemory);
    outLog("Sys_NtWriteVirtualMemory  0x%p", Sys_NtWriteVirtualMemory);
    outLog("ZwProtectVirtualMemory  0x%p", ZwProtectVirtualMemory);
    outLog("Sys_NtProtectVirtualMemory  0x%p", Sys_NtProtectVirtualMemory);
    outLog("Sys_PspCreateThread  0x%p", Sys_PspCreateThread);
    outLog("Sys_NtCreateThreadEx  0x%p", Sys_NtCreateThreadEx);
    outLog("Sys_NtOpenProcess  0x%p", Sys_NtOpenProcess);
    outLog("DbgkpConvertKernelToUserStateChange  0x%p", DbgkpConvertKernelToUserStateChange);
    outLog("DbgkpOpenHandles  0x%p", DbgkpOpenHandles);
    outLog("KeCopyExceptionRecord  0x%p", KeCopyExceptionRecord);
    outLog("Sys_ObReferenceObjectByHandleWithTag  0x%p", Sys_ObReferenceObjectByHandleWithTag);
    outLog("Sys_ObReferenceObjectByHandle  0x%p", Sys_ObReferenceObjectByHandle);
    outLog("Sys_ObfDereferenceObjectWithTag  0x%p", Sys_ObfDereferenceObjectWithTag);
    outLog("Sys_ObfDereferenceObject  0x%p", Sys_ObfDereferenceObject);
    outLog("KiCheckForKernelApcDelivery  0x%p", KiCheckForKernelApcDelivery);
    outLog("KeEnterCriticalRegionThread  0x%p", KeEnterCriticalRegionThread);
    outLog("KeLeaveCriticalRegionThread  0x%p", KeLeaveCriticalRegionThread);
    outLog("Sys_MmCopyVirtualMemory  0x%p", Sys_MmCopyVirtualMemory);
    outLog("Sys_PspCreateUserContext  0x%p", Sys_PspCreateUserContext);
    outLog("Sys_PspCallThreadNotifyRoutines  0x%p", Sys_PspCallThreadNotifyRoutines);
    outLog("Sys_PspAllocateThread  0x%p", Sys_PspAllocateThread);
    outLog("Sys_ObpReferenceObjectByHandleWithTag  0x%p", Sys_ObpReferenceObjectByHandleWithTag);
    outLog("Sys_MiObtainReferencedVadEx  0x%p", Sys_MiObtainReferencedVadEx);
    outLog("Sys_MmProtectVirtualMemory  0x%p", Sys_MmProtectVirtualMemory);
    outLog("Sys_NtGetContextThread  0x%p", Sys_NtGetContextThread);
    outLog("Sys_NtSetContextThread  0x%p", Sys_NtSetContextThread);
    outLog("ZwGetContextThread  0x%p", ZwGetContextThread);
    outLog("PspGetContextThreadInternal  0x%p", PspGetContextThreadInternal);
    outLog("Sys_KiDispatchException  0x%p", Sys_KiDispatchException);
    outLog("Sys_KeStackAttachProcess  0x%p", Sys_KeStackAttachProcess);
    outLog("Sys_KiStackAttachProcess  0x%p", Sys_KiStackAttachProcess);
    outLog("Sys_NtSetInformationDebugObject  0x%p", Sys_NtSetInformationDebugObject);
    outLog("Sys_NtTerminateProcess  0x%p", Sys_NtTerminateProcess);
    outLog("Sys_NtSuspendThread  0x%p", Sys_NtSuspendThread);
    outLog("Sys_NtResumeThread  0x%p", Sys_NtResumeThread);
    outLog("Sys_NtQueryInformationThread  0x%p", Sys_NtQueryInformationThread);

    outLog("\n");

    //对象偏移
    outLog("eprocess_offset::Pcb  0x%x", eprocess_offset::Pcb);
    outLog("eprocess_offset::DebugPort  0x%x", eprocess_offset::DebugPort);
    outLog("eprocess_offset::ImageFileName  0x%x", eprocess_offset::ImageFileName);
    outLog("eprocess_offset::WoW64Process  0x%x", eprocess_offset::WoW64Process);
    outLog("eprocess_offset::RundownProtect  0x%x", eprocess_offset::RundownProtect);
    outLog("eprocess_offset::ExitTime  0x%x", eprocess_offset::ExitTime);
    outLog("eprocess_offset::Machine  0x%x", eprocess_offset::Machine);
    outLog("kprocess_offset::DirectoryTableBase  0x%x", kprocess_offset::DirectoryTableBase);
    outLog("ethread_offset::Tcb  0x%x", ethread_offset::Tcb);
    outLog("ethread_offset::CrossThreadFlags  0x%x", ethread_offset::CrossThreadFlags);
    outLog("ethread_offset::Cid  0x%x", ethread_offset::Cid);
    outLog("ethread_offset::RundownProtect  0x%x", ethread_offset::RundownProtect);
    outLog("kthread_offset::ApcState  0x%x", kthread_offset::ApcState);
    outLog("kthread_offset::PreviousMode  0x%x", kthread_offset::PreviousMode);
    outLog("kthread_offset::Teb  0x%x", kthread_offset::Teb);
    outLog("kthread_offset::Process  0x%x", kthread_offset::Process);
    outLog("kthread_offset::KernelApcDisable  0x%x", kthread_offset::KernelApcDisable);
    outLog("kthread_offset::MiscFlags  0x%x", kthread_offset::MiscFlags);
    outLog("kthread_offset::TrapFrame  0x%x", kthread_offset::TrapFrame);
    outLog("kthread_offset::SuspendCount  0x%x", kthread_offset::SuspendCount);
    outLog("kapc_state_offset::Process  0x%x", kapc_state_offset::Process);
    outLog("image_nt_headers64_offset::FileHeader  0x%x", image_nt_headers64_offset::FileHeader);
    outLog("image_nt_headers64_offset::OptionalHeader  0x%x", image_nt_headers64_offset::OptionalHeader);
    outLog("image_nt_headers64_offset::Signature  0x%x", image_nt_headers64_offset::Signature);
    outLog("image_file_header_offset::PointerToSymbolTable  0x%x", image_file_header_offset::PointerToSymbolTable);
    outLog("image_file_header_offset::NumberOfSymbols  0x%x", image_file_header_offset::NumberOfSymbols);
    outLog("mmvad_offset::Core  0x%x", mmvad_offset::Core);
    outLog("mmvad_short_offset::LongFlags  0x%x", mmvad_short_offset::LongFlags);
}


void CheckFunctionPointers()
{
    //ntos

    CHECK_FUNC_PTR(DbgkDebugObjectType);
    CHECK_FUNC_PTR(PspNotifyEnableMask);
    CHECK_FUNC_PTR(PerfGlobalGroupMask);
    CHECK_FUNC_PTR(PspActiveProcessLock);
    CHECK_FUNC_PTR(PspProcessSequenceNumber);
    CHECK_FUNC_PTR(PsActiveProcessHead);

    CHECK_FUNC_PTR(PsFreezeProcess);
    CHECK_FUNC_PTR(Sys_NtCreateDebugObject);
    CHECK_FUNC_PTR(DbgkpSuspendProcess);
    CHECK_FUNC_PTR(Sys_DbgkCreateThread);
    CHECK_FUNC_PTR(Sys_DbgkpQueueMessage);
    CHECK_FUNC_PTR(DbgkpSectionToFileHandle);
    CHECK_FUNC_PTR(DbgkpSendApiMessage);
    CHECK_FUNC_PTR(Sys_DbgkMapViewOfSection);
    CHECK_FUNC_PTR(Sys_DbgkUnMapViewOfSection);
    CHECK_FUNC_PTR(DbgkpSuppressDbgMsg);
    CHECK_FUNC_PTR(Sys_DbgkExitThread);
    CHECK_FUNC_PTR(PsSetProcessFaultInformation);
    CHECK_FUNC_PTR(PsCaptureExceptionPort);
    CHECK_FUNC_PTR(DbgkpSendApiMessageLpc);
    CHECK_FUNC_PTR(DbgkpSendErrorMessage);
    CHECK_FUNC_PTR(Sys_DbgkForwardException);
    CHECK_FUNC_PTR(DbgkpPostFakeProcessCreateMessages);
    CHECK_FUNC_PTR(Sys_NtDebugActiveProcess);
    CHECK_FUNC_PTR(Sys_DbgkExitProcess);
    CHECK_FUNC_PTR(Sys_PspExitThread);
    CHECK_FUNC_PTR(DbgkpWakeTarget);
    CHECK_FUNC_PTR(Sys_NtDebugContinue);
    CHECK_FUNC_PTR(Sys_NtWaitForDebugEvent);
    CHECK_FUNC_PTR(ObDuplicateObject);
    CHECK_FUNC_PTR(DbgkClearProcessDebugObject);
    CHECK_FUNC_PTR(Sys_NtRemoveProcessDebug);
    CHECK_FUNC_PTR(PsGetNextProcess);
    CHECK_FUNC_PTR(DbgkpMarkProcessPeb);
    CHECK_FUNC_PTR(PsTerminateProcess);
    CHECK_FUNC_PTR(ObCreateObjectType);
    CHECK_FUNC_PTR(PsGetNextProcessThread);
    CHECK_FUNC_PTR(DbgkpPostFakeThreadMessages);
    CHECK_FUNC_PTR(KiStackAttachProcess);
    CHECK_FUNC_PTR(KiUnstackDetachProcess);
    CHECK_FUNC_PTR(Sys_NtReadVirtualMemory);
    CHECK_FUNC_PTR(Sys_NtWriteVirtualMemory);
    CHECK_FUNC_PTR(ZwProtectVirtualMemory);
    CHECK_FUNC_PTR(Sys_NtProtectVirtualMemory);
    CHECK_FUNC_PTR(Sys_PspCreateThread);
    CHECK_FUNC_PTR(Sys_NtCreateThreadEx);
    CHECK_FUNC_PTR(Sys_NtOpenProcess);
    CHECK_FUNC_PTR(DbgkpConvertKernelToUserStateChange);
    CHECK_FUNC_PTR(DbgkpOpenHandles);
    CHECK_FUNC_PTR(KeCopyExceptionRecord);
    CHECK_FUNC_PTR(Sys_ObReferenceObjectByHandleWithTag);
    CHECK_FUNC_PTR(Sys_ObReferenceObjectByHandle);
    CHECK_FUNC_PTR(Sys_ObfDereferenceObjectWithTag);
    CHECK_FUNC_PTR(Sys_ObfDereferenceObject);
    CHECK_FUNC_PTR(KiCheckForKernelApcDelivery);
    CHECK_FUNC_PTR(KeEnterCriticalRegionThread);
    CHECK_FUNC_PTR(KeLeaveCriticalRegionThread);
    CHECK_FUNC_PTR(Sys_MmCopyVirtualMemory);
    CHECK_FUNC_PTR(Sys_PspCreateUserContext);
    CHECK_FUNC_PTR(Sys_PspCallThreadNotifyRoutines);
    CHECK_FUNC_PTR(Sys_PspAllocateThread);
    CHECK_FUNC_PTR(Sys_ObpReferenceObjectByHandleWithTag);
    CHECK_FUNC_PTR(Sys_MiObtainReferencedVadEx);
    CHECK_FUNC_PTR(Sys_MmProtectVirtualMemory);
    CHECK_FUNC_PTR(Sys_NtGetContextThread);
    CHECK_FUNC_PTR(Sys_NtSetContextThread);
    CHECK_FUNC_PTR(ZwGetContextThread);
    CHECK_FUNC_PTR(PspGetContextThreadInternal);
    CHECK_FUNC_PTR(Sys_KiDispatchException);
    CHECK_FUNC_PTR(Sys_KeStackAttachProcess);
    CHECK_FUNC_PTR(Sys_KiStackAttachProcess);
    CHECK_FUNC_PTR(Sys_NtSetInformationDebugObject);
    CHECK_FUNC_PTR(Sys_NtTerminateProcess);
    CHECK_FUNC_PTR(Sys_NtSuspendThread);
    CHECK_FUNC_PTR(Sys_NtResumeThread);
    CHECK_FUNC_PTR(Sys_NtQueryInformationThread);
    CHECK_FUNC_PTR(PsGetCurrentProcessByThread);
    CHECK_FUNC_PTR(PsQuerySystemDllInfo);
    CHECK_FUNC_PTR(PsWow64GetProcessNtdllType);
    CHECK_FUNC_PTR(PspReferenceSystemDll);
    CHECK_FUNC_PTR(MiSectionControlArea);
    CHECK_FUNC_PTR(MiReferenceControlAreaFile);
    CHECK_FUNC_PTR(ObFastDereferenceObject);
    CHECK_FUNC_PTR(DbgkpPostModuleMessages);
    CHECK_FUNC_PTR(PsCallImageNotifyRoutines);
    CHECK_FUNC_PTR(PsReferenceProcessFilePointer);
    CHECK_FUNC_PTR(MiReferenceControlAreaFileWithTag);
    CHECK_FUNC_PTR(Sys_PspInsertProcess);
    CHECK_FUNC_PTR(SeAuditingWithTokenForSubcategory);
    CHECK_FUNC_PTR(SeAuditProcessCreation);
    CHECK_FUNC_PTR(PspImplicitAssignProcessToJob);
    CHECK_FUNC_PTR(PspInheritSyscallProvider);
    CHECK_FUNC_PTR(PspUnlockProcessListExclusive);
    CHECK_FUNC_PTR(ObCheckRefTraceProcess);
    CHECK_FUNC_PTR(PspValidateJobAffinityState);
    CHECK_FUNC_PTR(DbgkCopyProcessDebugPort);
    CHECK_FUNC_PTR(SeCreateAccessStateEx);
    CHECK_FUNC_PTR(ObInsertObjectEx);
    CHECK_FUNC_PTR(SepDeleteAccessState);
    CHECK_FUNC_PTR(DbgkSendSystemDllMessages);
    CHECK_FUNC_PTR(Sys_DbgkOpenProcessDebugPort);

    //win32kbase
    CHECK_FUNC_PTR(Sys_ValidateHwnd);

    //win32kfull
    CHECK_FUNC_PTR(Sys_NtUserFindWindowEx);
    CHECK_FUNC_PTR(Sys_NtUserWindowFromPoint);
}

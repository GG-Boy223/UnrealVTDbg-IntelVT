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
            //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
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
            //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
            return false;
        }

        if (encryptedText[encryptedTextBytes - 1] != '\0')
        {
            //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
            return false;
        }

        for (ULONG i = 0; i < encryptedTextBytes - 1; ++i)
        {
            if (!IsHexChar(encryptedText[i]))
            {
                //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
                return false;
            }
        }

        return true; // 默认放行
    }

    bool TryReadVerifiedInitKey(const USER_DATA& user, unsigned __int64* matchedKey)
    {
        if (matchedKey == nullptr)
        {
            //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
            return false;
        }

        *matchedKey = 0;

        if (!IsValidVerifyPayloadRequest(user))
        {
            //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
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
        info->source = WINDOWS_OFFSET_SOURCE_WIN10;
        info->flags = 0;
        info->kprocess_offset_DirectoryTableBase = kprocess_offset::DirectoryTableBase;
        info->eprocess_offset_Pcb = eprocess_offset::Pcb;
        info->eprocess_offset_DebugPort = eprocess_offset::DebugPort;
        info->eprocess_offset_ImageFileName = eprocess_offset::ImageFileName;
        info->eprocess_offset_WoW64Process = eprocess_offset::WoW64Process;
        info->eprocess_offset_RundownProtect = eprocess_offset::RundownProtect;
        info->eprocess_offset_ExitTime = eprocess_offset::ExitTime;
        info->eprocess_offset_Machine = WINDOWS_OFFSET_INVALID;
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
        info->ewow64process_offset_Machine = ewow64process_offset::Machine;
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

#ifdef DEBUG0
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
        //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
        return false;
    }

    // 这里再做一次本地防守式检查，避免上层误传无效值。
    if (verifyKey != VT_INIT_HANDSHAKE_KEY)
    {
        //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
        return false;
    }
    WINDOWS_STRUCT vmcallinfo = { 0 };
    FillWindowsOffsetContract(&vmcallinfo);

    // vmcallinfo 仍然走 rdx；verifyKey 通过 r8 带给 VT_Driver。
    if (hvgt::vmcall(&vmcallinfo, verifyKey))
    {
        //outLog("Win10 为vt host初始化offset成功..");
        return true;
    }
    else
    {
        //outLog("Win10 为vt host初始化offset失败..");
    }

    // 如果访问的是我们调试器的窗口，则拒绝访问。
    //PrintProcessName((_EPROCESS*)PsGetCurrentProcess());
    return false;
}

void CheckFunctionPointers()
{
    //ntos

    CHECK_FUNC_PTR(PspLoaderInitRoutine);
    CHECK_FUNC_PTR(DbgkDebugObjectType);
    CHECK_FUNC_PTR(PspNotifyEnableMask);
    CHECK_FUNC_PTR(PerfGlobalGroupMask);
    CHECK_FUNC_PTR(PspActiveProcessLock);
    CHECK_FUNC_PTR(PspProcessSequenceNumber);
    CHECK_FUNC_PTR(PsActiveProcessHead);

    CHECK_FUNC_PTR(PsFreezeProcess);
    CHECK_FUNC_PTR(PsThawProcess);
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
    CHECK_FUNC_PTR(SeAuditingWithTokenForSubcategory);
    CHECK_FUNC_PTR(SeAuditProcessCreation);
    CHECK_FUNC_PTR(PspImplicitAssignProcessToJob);
    CHECK_FUNC_PTR(PspUnlockProcessListExclusive);
    CHECK_FUNC_PTR(DbgkCopyProcessDebugPort);
    CHECK_FUNC_PTR(SeCreateAccessStateEx);
    CHECK_FUNC_PTR(ObInsertObjectEx);
    CHECK_FUNC_PTR(ObCheckRefTraceProcess);
    CHECK_FUNC_PTR(PspValidateJobAffinityState);
    CHECK_FUNC_PTR(SepDeleteAccessState);
    CHECK_FUNC_PTR(Sys_PspInsertProcess);
    CHECK_FUNC_PTR(DbgkSendSystemDllMessages);

    //win32kbase
    CHECK_FUNC_PTR(Sys_ValidateHwnd);

    //win32kfull
    CHECK_FUNC_PTR(Sys_NtUserFindWindowEx);
    CHECK_FUNC_PTR(Sys_NtUserWindowFromPoint);
}

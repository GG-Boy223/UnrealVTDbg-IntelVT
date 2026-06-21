#include "DebugEventHandlersInternal.h"

namespace
{
    ULONG QueryNtBuildNumberForLog()
    {
        RTL_OSVERSIONINFOW VersionInfo = { 0 };
        VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);

        if (!NT_SUCCESS(RtlGetVersion(&VersionInfo)))
        {
            return 0;
        }

        return VersionInfo.dwBuildNumber;
    }

    const char* QueryProcessImageNameForLog(PEPROCESS Process)
    {
        if (Process == NULL || eprocess_offset::ImageFileName == 0xFFFFFFFF)
        {
            return "<unknown>";
        }

        return (const char*)((size_t)Process + eprocess_offset::ImageFileName);
    }

    // 旧项目保留的禁用路径：Win11 26100/26200 固定把 SystemDll 查询上限限制为 6，否则直接返回 PsSystemDllTotalTypes。
    int QuerySystemDllLimitForBuild(ULONG BuildNumber)
    {
        if (BuildNumber == 26100 || BuildNumber == 26200)
        {
            return 6;
        }
    
        return PsSystemDllTotalTypes;
    }

    const LONG kDbgkSystemDllLogLimit = 64;
    volatile LONG g_DbgkSystemDllLogCount = 0;

    bool ShouldLogSystemDllDiag()
    {
        return InterlockedIncrement(&g_DbgkSystemDllLogCount) <= kDbgkSystemDllLogLimit;
    }
}

// 旧项目保留的禁用路径：轻量版 DbgkCreateThread 只解析 DebugPort，写入 Debugport_Struct 后调用 Original_DbgkCreateThread。
// 为什么注释：重构后的 DbgkCreateThread 负责补发 create process/thread、image notify、system dll 等完整调试消息；
// 旧实现没有构造 DBGKM_APIMSG，也没有处理 Hvm_DebugObject 事件队列。
// 放开的好处：逻辑短，适合只验证 DebugPort 来源或快速回退到原始内核函数。
// 放开的坏处：调试器收不到完整线程创建事件，Hvm_DebugObject 队列状态也不会更新。
// 项目影响：会破坏当前内核调试对象模拟链路，导致 WaitForDebugEvent/ContinueDebugEvent 行为不完整。
// 结论：继续注释，不放开。
//EXTERN_C
//VOID DbgkCreateThread(IN _ETHREAD* Thread)
//{
//    Debugport_Struct s;
//    PDEBUG_OBJECT Port;
//    PDEBUG_PROCESS DebugProcess;
    bool isWoW64Process = false;
//    size_t kthread_base = (size_t)Thread + ethread_offset::Tcb;
//    size_t ApcState = kthread_base + kthread_offset::ApcState;
//    size_t ptr_Process = ApcState + kapc_state_offset::Process;
//
//    if (IsDebugTargetProcess(*(_EPROCESS**)ptr_Process, &DebugProcess))
//    {
//        Port = DebugProcess->DebugObject;
//    }
//    else
//    {
//        size_t ptr_DebugPort = *(size_t*)ptr_Process + eprocess_offset::DebugPort;
//        Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
//    }
//
//    s.id = 0xd7b32a726e23dbc2;
//    s.DebugPort = (size_t)Port;
//    Original_DbgkCreateThread(Thread);
//}

EXTERN_C
VOID DbgkCreateThread(IN PETHREAD Thread)
{
    PEPROCESS Process;
    IMAGE_INFO_EX ImageInfoEx = { 0 };
    PIMAGE_NT_HEADERS NtHeader;
    NTSTATUS Status;
    PVOID DebugPort;
    DBGKM_APIMSG ApiMessage = { 0 };
    PDBGKM_CREATE_THREAD CreateThread = &ApiMessage.u.CreateThread;
    PDBGKM_CREATE_PROCESS CreateProcess = &ApiMessage.u.CreateProcess;
    PFILE_OBJECT FileObject = NULL;
    PUNICODE_STRING ImageFileName;
    PPS_SYSTEM_DLL_INFO DllInfo;
    PSECTION Section;
    PCONTROL_AREA ControlArea;
    PDEBUG_PROCESS DebugProcess;
    bool isWoW64Process = false;
    PROCESSFLAGS OldFlags;
    PPSP_SYSTEM_DLL_DATA SystemDllData;
    ULONG BuildNumber;
    int SystemDllQueryLimit;
    const char* ProcessImageName;
    HANDLE ProcessId;
    bool LogSystemDllDiag;
    PVOID Wow64Process;
    USHORT CurrentMachine;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return;
    }

    Process = PsGetCurrentProcessByThread(Thread);

    size_t ptr_WoW64Process = (size_t)Process + eprocess_offset::WoW64Process;
    size_t ptr_CurrentMachine = (size_t)Process + eprocess_offset::Machine;
    Wow64Process = *(PVOID*)ptr_WoW64Process;
    CurrentMachine = *(unsigned short*)ptr_CurrentMachine;
    isWoW64Process = Wow64Process && (CurrentMachine == IMAGE_FILE_MACHINE_I386 || CurrentMachine == IMAGE_FILE_MACHINE_ARMNT);

    size_t ptr_Flags = (size_t)Process + eprocess_offset::Flags;
    OldFlags.Flags = InterlockedOr((volatile LONG*)ptr_Flags, 0x400001u);

    if (!(OldFlags.ImageNotifyDone) && (*PspNotifyEnableMask & 1 || *PerfGlobalGroupMask & 4))
    {
        BuildNumber = QueryNtBuildNumberForLog();
        SystemDllQueryLimit = QuerySystemDllLimitForBuild(BuildNumber);
        ProcessImageName = QueryProcessImageNameForLog(Process);
        ProcessId = PsGetProcessId(Process);
        LogSystemDllDiag = ShouldLogSystemDllDiag();

        if (LogSystemDllDiag)
        {
            outLog("DbgkCreateThread SystemDllDiag build:%lu enumTotal:%d queryLimit:%d process:%s pid:%p wow64:%d wow64Process:%p machine:0x%X",
                BuildNumber,
                PsSystemDllTotalTypes,
                SystemDllQueryLimit,
                ProcessImageName,
                ProcessId,
                isWoW64Process ? 1 : 0,
                Wow64Process,
                CurrentMachine);
        }

        ImageInfoEx.Size = 0;
        ImageInfoEx.ImageInfo.Properties = 0;
        ImageInfoEx.ImageInfo.ImageAddressingMode = IMAGE_ADDRESSING_MODE_32BIT;
        size_t ptr_SectionBaseAddress = (size_t)Process + eprocess_offset::SectionBaseAddress;
        ImageInfoEx.ImageInfo.ImageBase = *(PVOID*)ptr_SectionBaseAddress;
        ImageInfoEx.ImageInfo.ImageSize = 0;

        __try
        {
            NtHeader = RtlImageNtHeader(ImageInfoEx.ImageInfo.ImageBase);
            if (NtHeader)
            {
                ImageInfoEx.ImageInfo.ImageSize = NtHeader->OptionalHeader.SizeOfImage;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ImageInfoEx.ImageInfo.ImageSize = 0;
        }

        ImageInfoEx.ImageInfo.ImageSelector = 0;
        ImageInfoEx.ImageInfo.ImageSectionNumber = 0;

        Status = PsReferenceProcessFilePointer(Process, (PVOID*)&FileObject);
        if (!NT_SUCCESS(Status))
        {
            FileObject = NULL;
        }

        Status = SeLocateProcessImageName(Process, &ImageFileName);

        if (!NT_SUCCESS(Status))
        {
            ImageFileName = NULL;
        }
        PsCallImageNotifyRoutines(ImageFileName, Process, &ImageInfoEx, FileObject);
        if (ImageFileName)
        {
            ExFreePool(ImageFileName);
        }

        if (FileObject)
        {
            ObDereferenceObject(FileObject);
            FileObject = NULL;
        }

        if (PsQuerySystemDllInfo == NULL && LogSystemDllDiag)
        {
            outLog("DbgkCreateThread SystemDllQuery skipped: PsQuerySystemDllInfo is NULL build:%lu process:%s pid:%p",
                BuildNumber,
                ProcessImageName,
                ProcessId);
        }

        for (int i = 0; PsQuerySystemDllInfo != NULL && i < SystemDllQueryLimit; i++)
        {
            if (LogSystemDllDiag)
            {
                outLog("DbgkCreateThread SystemDllQuery begin build:%lu enumTotal:%d queryLimit:%d i:%d process:%s pid:%p wow64:%d",
                    BuildNumber,
                    PsSystemDllTotalTypes,
                    SystemDllQueryLimit,
                    i,
                    ProcessImageName,
                    ProcessId,
                    isWoW64Process ? 1 : 0);
            }

            DllInfo = PsQuerySystemDllInfo(i);

            if (LogSystemDllDiag)
            {
                outLog("DbgkCreateThread SystemDllQuery end build:%lu enumTotal:%d queryLimit:%d i:%d info:%p process:%s pid:%p wow64:%d",
                    BuildNumber,
                    PsSystemDllTotalTypes,
                    SystemDllQueryLimit,
                    i,
                    DllInfo,
                    ProcessImageName,
                    ProcessId,
                    isWoW64Process ? 1 : 0);
            }

            if (DllInfo && (i <= 0 || ((DllInfo->Flags & 8) && Wow64Process && i == PsWow64GetProcessNtdllType(Process))))
            {
                ImageInfoEx.ImageInfo.Properties = 0;
                ImageInfoEx.ImageInfo.ImageAddressingMode = IMAGE_ADDRESSING_MODE_32BIT;
                ImageInfoEx.ImageInfo.ImageBase = DllInfo->BaseAddress;
                ImageInfoEx.ImageInfo.ImageSize = 0;

                __try
                {
                    NtHeader = RtlImageNtHeader(DllInfo->BaseAddress);
                    if (NtHeader)
                    {
                        ImageInfoEx.ImageInfo.ImageSize = NtHeader->OptionalHeader.SizeOfImage;
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    ImageInfoEx.ImageInfo.ImageSize = 0;
                }

                ImageInfoEx.ImageInfo.ImageSelector = 0;
                ImageInfoEx.ImageInfo.ImageSectionNumber = 0;

                SystemDllData = CONTAINING_RECORD(DllInfo, PSP_SYSTEM_DLL_DATA, DllInfo);
                Section = PspReferenceSystemDll(SystemDllData->SystemDll);
                ControlArea = NULL;
                FileObject = NULL;
                if (Section)
                {
                    ControlArea = MiSectionControlArea(Section);
                    if (ControlArea)
                    {
                        FileObject = MiReferenceControlAreaFileWithTag(ControlArea, 'tlfD');
                    }

                    ObFastDereferenceObject(&SystemDllData->SystemDll->DllSection, Section, 'dSsP');
                }
                PsCallImageNotifyRoutines(&DllInfo->DllPath, Process, &ImageInfoEx, FileObject);
                if (FileObject)
                {
                    ObDereferenceObject(FileObject);
                    FileObject = NULL;
                }
            }
        }
    }

    if (IsDebugTargetProcess((_EPROCESS*)Process, &DebugProcess))
    {
        DebugPort = DebugProcess->DebugObject;
    }
    else
    {
        size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
        DebugPort = *(PDEBUG_OBJECT*)ptr_DebugPort;
    }

    if (!DebugPort)
    {
        return;
    }

    if (!(OldFlags.CreateReported))
    {
        CreateProcess->InitialThread.SubSystemKey = 0;
        CreateProcess->InitialThread.StartAddress = NULL;

        CreateProcess->SubSystemKey = 0;
        size_t ptr_SectionObject = (size_t)Process + eprocess_offset::SectionObject;
        CreateProcess->FileHandle = DbgkpSectionToFileHandle(*(PVOID*)ptr_SectionObject);
        size_t ptr_SectionBaseAddress = (size_t)Process + eprocess_offset::SectionBaseAddress;
        CreateProcess->BaseOfImage = *(PVOID*)ptr_SectionBaseAddress;
        CreateProcess->DebugInfoFileOffset = 0;
        CreateProcess->DebugInfoSize = 0;

        __try
        {
            NtHeader = RtlImageNtHeader(*(PVOID*)ptr_SectionBaseAddress);
            if (NtHeader)
            {
                CreateProcess->InitialThread.StartAddress = (PVOID)(NtHeader->OptionalHeader.ImageBase + NtHeader->OptionalHeader.AddressOfEntryPoint);

                CreateProcess->DebugInfoFileOffset = NtHeader->FileHeader.PointerToSymbolTable;
                CreateProcess->DebugInfoSize = NtHeader->FileHeader.NumberOfSymbols;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            CreateProcess->InitialThread.StartAddress = NULL;
            CreateProcess->DebugInfoFileOffset = 0;
            CreateProcess->DebugInfoSize = 0;
        }

        ApiMessage.h.u1.Length = 0x600038;
        ApiMessage.h.u2.ZeroInit = LPC_DEBUG_EVENT;
        ApiMessage.ApiNumber = DbgKmCreateProcessApi;

        DbgkpSendApiMessage((_EPROCESS*)Process, FALSE, &ApiMessage);

        if (CreateProcess->FileHandle)
        {
            ObCloseHandle(CreateProcess->FileHandle, KernelMode);
        }
        DbgkSendSystemDllMessages(NULL, NULL, &ApiMessage);
    }
    else
    {
        CreateThread->SubSystemKey = 0;
        size_t ptr_Win32StartAddress = (size_t)Thread + ethread_offset::Win32StartAddress;
        CreateThread->StartAddress = *(PVOID*)ptr_Win32StartAddress;

        ApiMessage.h.u1.Length = 0x400018;
        ApiMessage.h.u2.ZeroInit = LPC_DEBUG_EVENT;
        ApiMessage.ApiNumber = DbgKmCreateThreadApi;

        DbgkpSendApiMessage((_EPROCESS*)Process, TRUE, &ApiMessage);
    }

    size_t ptr_SameThreadPassiveFlags = (size_t)Thread + ethread_offset::SameThreadPassiveFlags;
    if (*(unsigned long*)ptr_SameThreadPassiveFlags & 16)
    {
        DbgkpPostModuleMessages(Process, Thread, NULL);
    }
}

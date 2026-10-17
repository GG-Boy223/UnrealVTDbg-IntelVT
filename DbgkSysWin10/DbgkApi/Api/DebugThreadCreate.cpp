#include "DebugEventHandlersInternal.h"

extern BOOLEAN HvmIsVirtualDebugTargetProcess(_EPROCESS* Process, PVOID* DebugProcess);
extern VOID PrepareVirtualDebugTargetProcess(_EPROCESS* Process);

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

EXTERN_C VOID DbgkCreateThread(IN PETHREAD Thread)
{
    PEPROCESS Process;
    IMAGE_INFO_EX ImageInfoEx = { 0 };
    PIMAGE_NT_HEADERS NtHeader;
    NTSTATUS Status; // eax
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
    PROCESSFLAGS OldFlags;
    PPSP_SYSTEM_DLL_DATA SystemDllData;
    PVOID Wow64Process;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    Process = PsGetCurrentProcessByThread(Thread);
    size_t ptr_WoW64Process = (size_t)Process + eprocess_offset::WoW64Process;
    //size_t ptr_CurrentMachine = *(size_t*)ptr_WoW64Process + ewow64process_offset::Machine;
    //isWoW64Process = *(size_t*)ptr_WoW64Process && (*(unsigned short*)ptr_CurrentMachine == IMAGE_FILE_MACHINE_I386 || *(unsigned short*)ptr_CurrentMachine == IMAGE_FILE_MACHINE_ARMNT);
    Wow64Process = *(PVOID*)ptr_WoW64Process;

    size_t ptr_Flags = (size_t)Process + eprocess_offset::Flags;
    //将ImageNotifyDone和CreateReported置位
    OldFlags.Flags = InterlockedOr((volatile LONG*)ptr_Flags, 0x400001u);

    if (!(OldFlags.ImageNotifyDone) && (*PspNotifyEnableMask & 1 || *PerfGlobalGroupMask & 4))
    {
        ImageInfoEx.Size = 0;
        ImageInfoEx.ImageInfo.Properties = 0;
        ImageInfoEx.ImageInfo.ImageAddressingMode = IMAGE_ADDRESSING_MODE_32BIT;
        size_t ptr_SectionBaseAddress = (size_t)Process + eprocess_offset::SectionBaseAddress;
        ImageInfoEx.ImageInfo.ImageBase = *(PVOID*)ptr_SectionBaseAddress;
        ImageInfoEx.ImageInfo.ImageSize = 0;

        __try
        {
            // /* Get the NT Headers */
            NtHeader = RtlImageNtHeader(ImageInfoEx.ImageInfo.ImageBase);
            if (NtHeader)
            {
                // /* Set image size */
                ImageInfoEx.ImageInfo.ImageSize = NtHeader->OptionalHeader.SizeOfImage;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ImageInfoEx.ImageInfo.ImageSize = 0;
        }

        ImageInfoEx.ImageInfo.ImageSelector = 0;
        ImageInfoEx.ImageInfo.ImageSectionNumber = 0;

        //PsReferenceProcessFilePointer(Process, (PVOID*)&FileObject);
        Status = PsReferenceProcessFilePointer(Process, (PVOID*)&FileObject);
        // /* Check if we failed */
        if (!NT_SUCCESS(Status))
        {
            FileObject = NULL;
        }

        Status = SeLocateProcessImageName(Process, &ImageFileName);

        // /* Check if we failed */
        if (!NT_SUCCESS(Status))
        {
            ImageFileName = NULL;
        }

        PsCallImageNotifyRoutines(ImageFileName, Process, &ImageInfoEx, FileObject); //通知模块回调
        if (ImageFileName)
        {
            ExFreePool(ImageFileName);
        }

        //ObDereferenceObject(FileObject);
        if (FileObject)
        {
            ObDereferenceObject(FileObject);
            FileObject = NULL;
        }

        for (int i = 0; PsQuerySystemDllInfo != NULL && i < PsSystemDllTotalTypes; i++)
        {
            DllInfo = PsQuerySystemDllInfo(i);

            //if (DllInfo && (i <= 0 || (HIWORD(DllInfo->Flags) && *(size_t*)ptr_WoW64Process && i == PsWow64GetProcessNtdllType(Process))))
            if (DllInfo && (i <= 0 || (HIWORD(DllInfo->Flags) && Wow64Process && i == PsWow64GetProcessNtdllType(Process))))
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
                        // /* Set image size */
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
                //Section = PspReferenceSystemDll(&SystemDllData->SystemDll);  //快速引用对象
                Section = PspReferenceSystemDll(&SystemDllData->SystemDll); //快速引用对象
                ControlArea = NULL;
                FileObject = NULL;
                //ControlArea = MiSectionControlArea(Section);
                //FileObject = (PFILE_OBJECT)MiReferenceControlAreaFile(ControlArea);
                if (Section)
                {
                    ControlArea = MiSectionControlArea(Section);
                    if (ControlArea)
                    {
                        FileObject = (PFILE_OBJECT)MiReferenceControlAreaFile(ControlArea);
                    }

                    ObFastDereferenceObject(&SystemDllData->SystemDll.DllSection, Section);
                }

                PsCallImageNotifyRoutines(&DllInfo->DllPath, Process, &ImageInfoEx, FileObject); //通知模块回调
                //ObDereferenceObject(FileObject);
                if (FileObject)
                {
                    ObDereferenceObject(FileObject);
                    FileObject = NULL;
                }
            }
        }
    }

    // /* Fail if we have no port */
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
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    // create 路径（NtCreateUserProcess 带调试对象）：目标不在 g_DebugProcessList 中，
    // 由 DebugPort 指向 Hvm_DebugObject 判定。首个线程创建时统一清除 PEB/线程调试痕迹。
    if (!(OldFlags.CreateReported) &&
        HvmIsVirtualDebugTargetProcess((_EPROCESS*)Process, NULL))
    {
        PrepareVirtualDebugTargetProcess((_EPROCESS*)Process);
    }

    //检查报告是否已经创建
    if (!(OldFlags.CreateReported))
    {
        // /* Setup the information structure for the new thread */
        CreateProcess->InitialThread.SubSystemKey = 0;
        CreateProcess->InitialThread.StartAddress = NULL;
        // /* And for the new process */
        CreateProcess->SubSystemKey = 0;
        size_t ptr_SectionObject = (size_t)Process + eprocess_offset::SectionObject;
        CreateProcess->FileHandle = DbgkpSectionToFileHandle(*(PVOID*)ptr_SectionObject);
        size_t ptr_SectionBaseAddress = (size_t)Process + eprocess_offset::SectionBaseAddress;
        CreateProcess->BaseOfImage = *(PVOID*)ptr_SectionBaseAddress;
        CreateProcess->DebugInfoFileOffset = 0;
        CreateProcess->DebugInfoSize = 0;

        __try
        {
            // /* Get the NT Header */
            NtHeader = RtlImageNtHeader(*(PVOID*)ptr_SectionBaseAddress);
            if (NtHeader)
            {
                CreateProcess->InitialThread.StartAddress =
                    (PVOID)(NtHeader->OptionalHeader.ImageBase + NtHeader->OptionalHeader.AddressOfEntryPoint);

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

        // /* Setup the API Message */
        ApiMessage.h.u1.Length = 0x600038;
        ApiMessage.h.u2.ZeroInit = LPC_DEBUG_EVENT;
        ApiMessage.ApiNumber = DbgKmCreateProcessApi;

        // /* Send the message */
        DbgkpSendApiMessage((_EPROCESS*)Process, FALSE, &ApiMessage);

        if (CreateProcess->FileHandle)
        {
            // /* Close the handle */
            ObCloseHandle(CreateProcess->FileHandle, KernelMode);
        }

        DbgkSendSystemDllMessages(NULL, NULL, &ApiMessage);
    }
    else
    {
        // /* Otherwise, do it just for the thread */
        CreateThread->SubSystemKey = 0;
        size_t ptr_Win32StartAddress = (size_t)Thread + ethread_offset::Win32StartAddress;
        CreateThread->StartAddress = *(PVOID*)ptr_Win32StartAddress;

        // /* Setup the API Message */
        ApiMessage.h.u1.Length = 0x400018;
        ApiMessage.h.u2.ZeroInit = LPC_DEBUG_EVENT;
        ApiMessage.ApiNumber = DbgKmCreateThreadApi;

        // /* Send the message */
        DbgkpSendApiMessage((_EPROCESS*)Process, TRUE, &ApiMessage);
    }

    size_t ptr_SameThreadPassiveFlags = (size_t)Thread + ethread_offset::SameThreadPassiveFlags;
    if (*(unsigned long*)ptr_SameThreadPassiveFlags & 16)
    {
        DbgkpPostModuleMessages(Process, Thread, NULL);
    }
}




//
//NTSTATUS
//DbgkClearProcessDebugObject(
//    IN _EPROCESS* Process,
//    IN PDEBUG_OBJECT SourceDebugObject
//)
///*++
//
//Routine Description:
//
//    Remove a debug object from a process.
//
//Arguments:
//
//    Process           - Process to be debugged
//    sourceDebugObject - Debug object to detach
//
//Return Value:
//
//    NTSTATUS - Status of call.
//
//--*/
//{
//    NTSTATUS Status;
//    PDEBUG_OBJECT DebugObject;
//    PDEBUG_EVENT DebugEvent;
//    LIST_ENTRY TempList;
//    PLIST_ENTRY Entry;
//
//    PAGED_CODE();
//
//    ExAcquireFastMutex(&DbgkpProcessDebugPortMutex);
//
//    DebugObject = (PDEBUG_OBJECT)Process->DebugPort;
//    if (DebugObject == NULL || (DebugObject != SourceDebugObject && SourceDebugObject != NULL))
//    {
//        DebugObject = NULL;
//        Status = STATUS_PORT_NOT_SET;
//    }
//    else
//    {
//        Process->DebugPort = NULL;
//        Status = STATUS_SUCCESS;
//    }
//    ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);
//
//    if (NT_SUCCESS(Status))
//    {
//        DbgkpMarkProcessPeb(Process);
//    }
//
//    //
//    // Remove any events for this process and wake up the threads.
//    //
//    if (DebugObject)
//    {
//        //
//        // Remove any events and queue them to a temporary queue
//        //
//        InitializeListHead(&TempList);
//
//        ExAcquireFastMutex(&DebugObject->Mutex);
//
//        for (Entry = DebugObject->EventList.Flink;
//            Entry != &DebugObject->EventList;
//            Entry = Entry->Flink)
//        {
//            DebugEvent = CONTAINING_RECORD(Entry, DEBUG_EVENT, EventList);
//            if (DebugEvent->Process == Process)
//            {
//                RemoveEntryList(&DebugEvent->EventList);
//                InsertTailList(&TempList, &DebugEvent->EventList);
//            }
//        }
//        ExReleaseFastMutex(&DebugObject->Mutex);
//
//        ObDereferenceObject(DebugObject);
//
//        //
//        // Wake up all the removed threads.
//        //
//        while (!IsListEmpty(&TempList))
//        {
//            Entry = RemoveHeadList(&TempList);
//            DebugEvent = CONTAINING_RECORD(Entry, DEBUG_EVENT, EventList);
//            DebugEvent->Status = STATUS_DEBUGGER_INACTIVE;
//            DbgkpWakeTarget(DebugEvent);
//        }
//    }
//
//    return Status;
//}

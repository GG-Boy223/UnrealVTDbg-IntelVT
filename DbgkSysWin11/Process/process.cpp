#include "../Driver.h"
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
#include "../Globals.h"
#include "../DbgkApi/DbgkApi.h"
#include "process.h"

NTSTATUS PspInsertProcess(IN PEPROCESS TargetProcess,
    IN PEPROCESS ParentProcess,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG Flags,
    IN HANDLE DebugObjectHandle,
    IN ULONG CrossThreadFlags,
    IN int a7,
    IN PPSP_OBJECT_CREATION_STATE ObjectCreationState)
{
    PEJOB Job; // rax
    NTSTATUS Status; // eax
    PEPROCESS Parent; // [rsp+40h] [rbp-48h]
    PVOID DebugPortObject = NULL; // [rsp+48h] [rbp-40h]
    BOOLEAN boCopyDebugPort; // [rsp+90h] [rbp+8h]
    PETHREAD CurrentThread;
    PEPROCESS Process;
    int v18;
    PHANDLE NewHandle;
    KAPC_STATE ApcState;
    PVOID lpLdrInitializeThunk;

    PAGED_CODE();

    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        DebugObjectHandle = NULL;
    }

    CurrentThread = PsGetCurrentThread();

    //是当前进程调用PspCreateProcess 创建子进程
    //所以通过PsGetCurrentProcess 得到当前进程 作为Parent
    Parent = PsGetCurrentProcess();

    //下面的这段代码其实是TargetProcess->ObjectTable->UniqueProcessId = TargetProcess->UniqueProcessId;
    size_t ptr_ObjectTable = (size_t)TargetProcess + eprocess_offset::ObjectTable;
    size_t ptr_handle_table_UniqueProcessId = *(size_t*)ptr_ObjectTable + handle_table_offset::UniqueProcessId;
    size_t ptr_UniqueProcessId = (size_t)TargetProcess + eprocess_offset::UniqueProcessId;
    *(size_t*)ptr_handle_table_UniqueProcessId = *(size_t*)ptr_UniqueProcessId;    

    //使用令牌进行子类别审计
    if (SeAuditingWithTokenForSubcategory(0x85, NULL))
    {
        /*审核进程的创建。调用者有责任确定进程审核是否正在进行中。*/
        // /*审核进程的创建。调用者有责任确定进程审核是否正在进行中。*/
        SeAuditProcessCreation(TargetProcess, a7);
    }        

    size_t ptr_ParentJob = (size_t)ParentProcess + eprocess_offset::Job;

    if (!ParentProcess ||
        (!*(PEJOB*)ptr_ParentJob || (Status = PspImplicitAssignProcessToJob(*(PEJOB*)ptr_ParentJob, TargetProcess, Flags), Status >= 0)) &&
        (Status = PspInheritSyscallProvider(TargetProcess, ParentProcess), Status >= 0))
    {
        //将进程添加到活动进程列表中。
        PspLockProcessListExclusive(CurrentThread);
        size_t ptr_ActiveProcessLinks = (size_t)TargetProcess + eprocess_offset::ActiveProcessLinks;
        InsertTailList(PsActiveProcessHead, (PLIST_ENTRY)ptr_ActiveProcessLinks);

        //描述TargetProcess->SequenceNumber = ++PspProcessSequenceNumber;
        size_t ptr_SequenceNumber = (size_t)TargetProcess + eprocess_offset::SequenceNumber;        
        *(size_t*)ptr_SequenceNumber = ++(*PspProcessSequenceNumber);
        // *(size_t*)ptr_SequenceNumber = ++(*PspProcessSequenceNumber);
        PspUnlockProcessListExclusive(CurrentThread);

        size_t ptr_ParentFlags = (size_t)Parent + eprocess_offset::Flags;
        PROCESSFLAGS ParentFlags = *(PROCESSFLAGS*)ptr_ParentFlags;
        if (ParentFlags.ProcessDelete)                    // Parent->Flags.ProcessDelete
        {
            Status = STATUS_PROCESS_IS_TERMINATING;//进程正在终止
        }
        else if (!DebugObjectHandle || (Status = ObReferenceObjectByHandle(
            DebugObjectHandle,
            DEBUG_PROCESS_ASSIGN,
            Hvm_DbgkDebugObjectType,
            KernelMode,
            &DebugPortObject,
            NULL),
            Status >= 0))
        {
            Status = DbgkCopyProcessDebugPort(TargetProcess, Parent, (PDEBUG_OBJECT)DebugPortObject, &boCopyDebugPort);
            if (DebugPortObject)
                ObDereferenceObject(DebugPortObject);

            if (NT_SUCCESS(Status))
            {
                if (boCopyDebugPort && Flags & 2)
                {
                    //不继承调试端口
                    size_t ptr_TargetFlags = (size_t)TargetProcess + eprocess_offset::Flags;
                    InterlockedOr((volatile LONG*)ptr_TargetFlags, 2u);// set NoDebugInherit
                }                    
                v18 = 0;
                if (!ParentProcess || 
                    (Process = PsInitialSystemProcess, ParentProcess != PsInitialSystemProcess))
                {
                    if (CrossThreadFlags & 2)           // ThreadInserted
                    {
                        Process = TargetProcess;
                        v18 = 1;
                    }
                    else
                    {
                        Process = Parent;
                    }
                }

                size_t ptr_TypeInfo = (size_t)PsProcessType + object_type_offset::TypeInfo;
                size_t ptr_GenericMapping = ptr_TypeInfo + object_type_initializer_offset::GenericMapping;
                Status = SeCreateAccessStateEx(
                    NULL,
                    Process,
                    &ObjectCreationState->AccessState,
                    &ObjectCreationState->AuxData,
                    DesiredAccess,
                    (PGENERIC_MAPPING)ptr_GenericMapping);

                if (NT_SUCCESS(Status))
                {
                    ObReferenceObjectWithTag(TargetProcess, 'rCsP');
                    Status = ObInsertObjectEx(
                        TargetProcess,
                        &ObjectCreationState->AccessState,
                        DesiredAccess,
                        0,
                        v18,
                        NULL,
                        NULL);

                    if (NT_SUCCESS(Status))
                    {
                        ObDereferenceObjectWithTag(TargetProcess, 'rCsP');

                        //描述Job = TargetProcess->Job;
                        size_t ptr_TargetJob = (size_t)TargetProcess + eprocess_offset::Job;
                        Job = *(PEJOB*)ptr_TargetJob;                        
                        if (!Job)
                        {
                            //不继承调试端口
                            size_t ptr_TargetFlags = (size_t)TargetProcess + eprocess_offset::Flags;
                            InterlockedOr((volatile LONG*)ptr_TargetFlags, 0x4000000u);// set ProcessInserted
                        LABEL_21:
                            ObCheckRefTraceProcess(TargetProcess);
                            if (CrossThreadFlags & 1)       // Terminated
                            {
                                PspUnlockProcessExclusive(TargetProcess, CurrentThread);
                            }
                            return STATUS_SUCCESS;
                        }
                        Status = PspValidateJobAffinityState(Job, TargetProcess);
                        if (NT_SUCCESS(Status))
                            goto LABEL_21;
                    }
                    SepDeleteAccessState(&ObjectCreationState->AccessState);
                    SeReleaseSubjectContext(&ObjectCreationState->AccessState.SubjectSecurityContext);
                }
            }
        }
    }

    if (CrossThreadFlags & 1)                   // Terminated
    {
        PspUnlockProcessExclusive(TargetProcess, CurrentThread);
    }
    // /* Return status */
    return Status;
}
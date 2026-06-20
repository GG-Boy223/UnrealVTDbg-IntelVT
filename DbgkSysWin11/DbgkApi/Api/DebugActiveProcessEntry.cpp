#include "DebugActiveProcess.h"
#include "DebugObjectCommonInternal.h"

EXTERN_C
NTSTATUS NtDebugActiveProcess(
    IN HANDLE ProcessHandle,
    IN HANDLE DebugHandle)
{
    NTSTATUS Status;
    _ETHREAD* LastThread = NULL;
    _EPROCESS* TargetProcess;
    PDEBUG_OBJECT DebugObject;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;
    DBGKTRACE(DBGK_PROCESS_DEBUG, "Process: %p Handle: %p\n",
        ProcessHandle, DebugHandle);

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    Status = ObReferenceObjectByHandle(ProcessHandle,
        PROCESS_SET_PORT,
        *PsProcessType,
        KernelMode,
        (PVOID*)&TargetProcess,
        NULL);
    if (NT_SUCCESS(Status))
    {
        if (TargetProcess == (_EPROCESS*)PsGetCurrentProcess() || TargetProcess == (_EPROCESS*)PsInitialSystemProcess)
        {
            ObDereferenceObject(TargetProcess);
            return STATUS_ACCESS_DENIED;
        }
        else
        {
            _EPROCESS* CurrentProcess = (_EPROCESS*)PsGetCurrentProcess();
            size_t ptr_CurrentWoW64Process = (size_t)CurrentProcess + eprocess_offset::WoW64Process;
            size_t ptr_CurrentMachine = (size_t)CurrentProcess + eprocess_offset::Machine;

            size_t ptr_TargetWoW64Process = (size_t)TargetProcess + eprocess_offset::WoW64Process;
            size_t ptr_TargetMachine = (size_t)TargetProcess + eprocess_offset::Machine;

            if ((!*(size_t*)ptr_CurrentWoW64Process) ||
                (*(unsigned short*)ptr_CurrentMachine != IMAGE_FILE_MACHINE_I386 && *(unsigned short*)ptr_CurrentMachine != IMAGE_FILE_MACHINE_ARMNT) ||
                (*(size_t*)ptr_TargetWoW64Process && (*(unsigned short*)ptr_TargetMachine == IMAGE_FILE_MACHINE_I386 || *(unsigned short*)ptr_TargetMachine == IMAGE_FILE_MACHINE_ARMNT)))
            {
                Status = ObReferenceObjectByHandle(DebugHandle,
                    DEBUG_PROCESS_ASSIGN,
                    Hvm_DbgkDebugObjectType,
                    KernelMode,
                    (PVOID*)&DebugObject,
                    NULL);
                if (NT_SUCCESS(Status))
                {
                    size_t ptr_RundownProtect = (size_t)TargetProcess + eprocess_offset::RundownProtect;
                    if (ExAcquireRundownProtection((PEX_RUNDOWN_REF)ptr_RundownProtect))
                    {
                        Status = DbgkpPostFakeProcessCreateMessages(TargetProcess, DebugObject, &LastThread);
                        Status = DbgkpSetProcessDebugObject(TargetProcess, DebugObject, Status, LastThread);
                        ExReleaseRundownProtection((PEX_RUNDOWN_REF)ptr_RundownProtect);
                    }
                    else
                    {
                        Status = STATUS_PROCESS_IS_TERMINATING;
                    }
                    ObDereferenceObject(DebugObject);
                }
            }
            else
            {
                Status = STATUS_NOT_SUPPORTED;
            }
        }
        ObDereferenceObject(TargetProcess);
    }
    return Status;
}

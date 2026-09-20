#include "RemoveProcessDebug.h"
#include "DebugObjectCommonInternal.h"

EXTERN_C
NTSTATUS
NtRemoveProcessDebug(
    IN HANDLE ProcessHandle,
    IN HANDLE DebugObjectHandle)
{
    NTSTATUS Status; // eax
    PDEBUG_OBJECT DebugObject;
    _EPROCESS* Process; //进程对象

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    KPROCESSOR_MODE const PreviousMode = KeGetPreviousMode();

    // /* Reference the process */
    Status = ObReferenceObjectByHandle(ProcessHandle,
        PROCESS_SET_PORT,
        *PsProcessType,
        // *PsProcessType,
        PreviousMode,
        (PVOID*)&Process,
        NULL);

    // /* Check if we failed */
    if (!NT_SUCCESS(Status))
    {
        // /* Return status */
        return Status;
    }

    Status = ObReferenceObjectByHandle(DebugObjectHandle,
        DEBUG_PROCESS_ASSIGN,
        Hvm_DbgkDebugObjectType,
        PreviousMode,
        (PVOID*)&DebugObject,
        NULL);

    if (NT_SUCCESS(Status))
    {
        Status = DbgkClearProcessDebugObject(Process, DebugObject);
        DeleteDebugProcess(DebugObject);
        DbgPrint("DbgkClearProcessDebugObject Status: %x", Status);
        ObDereferenceObject(DebugObject);
    }

    ObDereferenceObject(Process); //释放进程对象
    /* Return status */
    // /* Return status */
    return Status;
}

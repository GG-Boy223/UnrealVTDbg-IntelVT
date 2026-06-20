#include "RemoveProcessDebug.h"
#include "DebugObjectCommonInternal.h"

EXTERN_C
NTSTATUS
NtRemoveProcessDebug(
    IN HANDLE ProcessHandle,
    IN HANDLE DebugObjectHandle)
{
    NTSTATUS Status;
    PDEBUG_OBJECT DebugObject;
    _EPROCESS* Process;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    Status = ObReferenceObjectByHandle(ProcessHandle,
        PROCESS_SET_PORT,
        *PsProcessType,
        KernelMode,
        (PVOID*)&Process,
        NULL);

    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = ObReferenceObjectByHandle(DebugObjectHandle,
        DEBUG_PROCESS_ASSIGN,
        Hvm_DbgkDebugObjectType,
        KernelMode,
        (PVOID*)&DebugObject,
        NULL);

    if (NT_SUCCESS(Status))
    {
        Status = DbgkClearProcessDebugObject(Process, DebugObject);
        DeleteDebugProcess(DebugObject);
        DbgPrint("DbgkClearProcessDebugObject Status: %x", Status);
        ObDereferenceObject(DebugObject);
    }

    ObDereferenceObject(Process);
    return Status;
}

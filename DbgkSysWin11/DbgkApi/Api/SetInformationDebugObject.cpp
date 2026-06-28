#include "SetInformationDebugObject.h"
#include "DebugObjectCommonInternal.h"

NTSTATUS
NtSetInformationDebugObject(
    IN HANDLE DebugObjectHandle,
    IN DEBUGOBJECTINFOCLASS DebugObjectInformationClass,
    IN PVOID DebugInformation,
    IN ULONG DebugInformationLength,
    OUT PULONG ReturnLength OPTIONAL)
{
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status; // eax
    PDEBUG_OBJECT DebugObject;
    ULONG Flags;

    PreviousMode = KeGetPreviousMode();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    __try
    {
        if (PreviousMode != KernelMode)
        {
            ProbeForRead(DebugInformation,
                DebugInformationLength,
                sizeof(ULONG));
            if (ARGUMENT_PRESENT(ReturnLength))
            {
                ProbeForWriteUlong(ReturnLength);
            }
        }
        if (ARGUMENT_PRESENT(ReturnLength))
        {
            *ReturnLength = 0;
        }

        switch (DebugObjectInformationClass)
        {
        case DebugObjectFlags:
        {

            if (DebugInformationLength != sizeof(ULONG))
            {
                if (ARGUMENT_PRESENT(ReturnLength))
                {
                    *ReturnLength = sizeof(ULONG);
                }
                return STATUS_INFO_LENGTH_MISMATCH;
            }
            Flags = *(PULONG)DebugInformation;

            break;
        }
        // /* No special handling for other messages */
        default:
        {
            return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。
        }
        }
    }
    __except (ExSystemExceptionFilter()) // 如果先前的模式是内核模式，则不处理异常
    {
        return GetExceptionCode();
    }

    switch (DebugObjectInformationClass)
    {
    case DebugObjectFlags:
    {
        if (Flags & ~DBGK_KILL_PROCESS_ON_EXIT)
        {
            return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。
        }
        Status = ObReferenceObjectByHandle(DebugObjectHandle,
            DEBUG_SET_INFORMATION,
            Hvm_DbgkDebugObjectType,
            PreviousMode,
            (PVOID*)&DebugObject,
            NULL);

        // /* Check if we failed */
        if (!NT_SUCCESS(Status))
        {
            // /* Return status */
            return Status;
        }
        ExAcquireFastMutex(&DebugObject->Mutex);

        // /* Set the Flags */
        if (Flags & DBGK_KILL_PROCESS_ON_EXIT) //退出时终止进程
        {
            DebugObject->Flags |= DEBUG_OBJECT_KILL_ON_CLOSE;
        }
        else
        {
            DebugObject->Flags &= ~DEBUG_OBJECT_KILL_ON_CLOSE;
        }

        ExReleaseFastMutex(&DebugObject->Mutex);

        ObDereferenceObject(DebugObject);
    }
    }
    return STATUS_SUCCESS;
}

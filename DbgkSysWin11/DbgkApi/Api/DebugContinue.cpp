#include "DebugContinue.h"
#include "DebugObjectCommonInternal.h"

EXTERN_C
NTSTATUS
NtDebugContinue(
    IN HANDLE DebugObjectHandle,
    IN PCLIENT_ID ClientId,
    IN NTSTATUS ContinueStatus)
{
    NTSTATUS Status;
    PDEBUG_OBJECT DebugObject;
    PDEBUG_EVENT DebugEvent, FoundDebugEvent;
    KPROCESSOR_MODE PreviousMode;
    CLIENT_ID Clid;
    PLIST_ENTRY Entry;
    BOOLEAN GotEvent;

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
            ProbeForReadSmallStructure(ClientId, sizeof(*ClientId), sizeof(UCHAR));
        }
        Clid = *ClientId;
    }
    __except (ExSystemExceptionFilter())
    {
        return GetExceptionCode();
    }

    switch (ContinueStatus)
    {
    case DBG_EXCEPTION_HANDLED:
    case DBG_EXCEPTION_NOT_HANDLED:
    case DBG_TERMINATE_THREAD:
    case DBG_TERMINATE_PROCESS:
    case DBG_CONTINUE:
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObReferenceObjectByHandle(DebugObjectHandle,
        DEBUG_READ_EVENT,
        Hvm_DbgkDebugObjectType,
        KernelMode,
        (PVOID*)&DebugObject,
        NULL);

    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    GotEvent = FALSE;
    FoundDebugEvent = NULL;

    ExAcquireFastMutex(&DebugObject->Mutex);

    for (Entry = DebugObject->EventList.Flink;
        Entry != &DebugObject->EventList;
        Entry = Entry->Flink)
    {
        DebugEvent = CONTAINING_RECORD(Entry, DEBUG_EVENT, EventList);

        if (DebugEvent->ClientId.UniqueProcess == Clid.UniqueProcess)
        {
            if (!GotEvent)
            {
                if ((DebugEvent->ClientId.UniqueThread == Clid.UniqueThread) && (DebugEvent->Flags & DEBUG_EVENT_READ))
                {
                    RemoveEntryList(Entry);
                    FoundDebugEvent = DebugEvent;
                    GotEvent = TRUE;
                }
            }
            else
            {
                DebugEvent->Flags &= ~DEBUG_EVENT_INACTIVE;
                KeSetEvent(&DebugObject->EventsPresent, 0, FALSE);
                break;
            }
        }
    }

    ExReleaseFastMutex(&DebugObject->Mutex);

    ObDereferenceObject(DebugObject);

    if (GotEvent)
    {
        FoundDebugEvent->ApiMsg.ReturnedStatus = ContinueStatus;
        FoundDebugEvent->Status = STATUS_SUCCESS;
        DbgkpWakeTarget(FoundDebugEvent);
    }
    else
    {
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

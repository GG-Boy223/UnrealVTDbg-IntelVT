#include "DebugContinue.h"
#include "DebugObjectCommonInternal.h"

EXTERN_C
NTSTATUS
NtDebugContinue(
    IN HANDLE DebugObjectHandle,
    IN PCLIENT_ID ClientId,
    IN NTSTATUS ContinueStatus)
{
    NTSTATUS Status; // eax
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
    __except (ExSystemExceptionFilter()) // 如果先前的模式是内核模式，则不处理异常
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
    // /* No special handling for other messages */
    default:
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。
    }

    Status = ObReferenceObjectByHandle(DebugObjectHandle,
        DEBUG_READ_EVENT,
        Hvm_DbgkDebugObjectType,
        KernelMode,
        (PVOID*)&DebugObject,
        NULL);

    // /* Check if we failed */
    if (!NT_SUCCESS(Status))
    {
        // /* Return status */
        return Status;
    }

    // /* Lock the object */
    GotEvent = FALSE;
    FoundDebugEvent = NULL;

    ExAcquireFastMutex(&DebugObject->Mutex);

    for (Entry = DebugObject->EventList.Flink;
        Entry != &DebugObject->EventList;
        Entry = Entry->Flink)
    {
        DebugEvent = CONTAINING_RECORD(Entry, DEBUG_EVENT, EventList);

        // Make sure the client ID matches and that the debugger saw all the events.
        // We don't allow the caller to start a thread that it never saw a message for.
        //
        if (DebugEvent->ClientId.UniqueProcess == Clid.UniqueProcess)
        {
            // /* Check if we got an event */
            if (!GotEvent)
            {
                if ((DebugEvent->ClientId.UniqueThread == Clid.UniqueThread) && (DebugEvent->Flags & DEBUG_EVENT_READ))
                {
                    RemoveEntryList(Entry); //移除自身
                    FoundDebugEvent = DebugEvent;
                    // /* We got an event */
                    GotEvent = TRUE;
                }
            }
            else
            {
                // VC breaks if it sees more than one event at a time
                //
                // for the same process.
                DebugEvent->Flags &= ~DEBUG_EVENT_INACTIVE;
                // Wake anyone waiting. They need to leave this object alone now as its deleting
                //
                KeSetEvent(&DebugObject->EventsPresent, 0, FALSE);
                break;
            }
        }
    }

    ExReleaseFastMutex(&DebugObject->Mutex);

    ObDereferenceObject(DebugObject);

    // /* Check if we have an event */
    if (GotEvent)
    {
        //DbgBreakPoint();
        FoundDebugEvent->ApiMsg.ReturnedStatus = ContinueStatus;
        FoundDebugEvent->Status = STATUS_SUCCESS;
        DbgkpWakeTarget(FoundDebugEvent);
    }
    else
    {
        Status = STATUS_INVALID_PARAMETER;
    }

    // /* Return status */
    return Status;
}

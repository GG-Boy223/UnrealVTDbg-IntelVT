#include "DebugEventHandlersInternal.h"

NTSTATUS DbgkpQueueMessage(
    IN _EPROCESS* Process,
    IN _ETHREAD* Thread,
    IN PDBGKM_APIMSG Message,
    IN ULONG Flags,
    IN PDEBUG_OBJECT TargetObject OPTIONAL)
{
    PDEBUG_EVENT DebugEvent;
    DEBUG_EVENT LocalDebugEvent;
    PDEBUG_OBJECT DebugObject;
    NTSTATUS Status;
    BOOLEAN NewEvent;
    PDEBUG_PROCESS DebugProcess;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DEBUGGER_INACTIVE;
    }

    DBGKTRACE(DBGK_MESSAGE_DEBUG,
        "Process: %p Thread: %p Message: %p Flags: %lx\n",
        Process, Thread, Message, Flags);

    NewEvent = (Flags & DEBUG_EVENT_NOWAIT) ? TRUE : FALSE;
    if (NewEvent)
    {
#pragma warning(push)
#pragma warning(disable: 4996)
        DebugEvent = (PDEBUG_EVENT)ExAllocatePoolWithTag(NonPagedPool, sizeof(DEBUG_EVENT), 'EgbD');
#pragma warning(pop)
        if (!DebugEvent)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        DebugEvent->Flags = Flags | DEBUG_EVENT_INACTIVE;
        ObReferenceObject(Thread);
        ObReferenceObject(Process);
        DebugEvent->BackoutThread = (_ETHREAD*)PsGetCurrentThread();
        DebugObject = TargetObject;
    }
    else
    {
        DebugEvent = &LocalDebugEvent;
        DebugEvent->Flags = Flags;

        ExAcquireFastMutex(&DbgkpProcessDebugPortMutex);

        if (IsDebugTargetProcess(Process, &DebugProcess))
        {
            DebugObject = DebugProcess->DebugObject;
        }
        else
        {
            size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
            DebugObject = *(PDEBUG_OBJECT*)ptr_DebugPort;
        }

        switch (Message->ApiNumber)
        {
        case DbgKmCreateThreadApi:
        case DbgKmCreateProcessApi:
        {
            break;
        }

        case DbgKmExitThreadApi:
        case DbgKmExitProcessApi:
        {
            break;
        }
        case DbgKmLoadDllApi:
        {
            break;
        }
        default:
            break;
        }
    }

    KeInitializeEvent(&DebugEvent->ContinueEvent, SynchronizationEvent, FALSE);
    DebugEvent->Process = Process;
    DebugEvent->Thread = Thread;
    DebugEvent->ApiMsg = *Message;

    size_t ptr_Cid = (size_t)Thread + ethread_offset::Cid;
    DebugEvent->ClientId = *(CLIENT_ID*)ptr_Cid;

    if (!DebugObject)
    {
        Status = STATUS_PORT_NOT_SET;
    }
    else
    {
        ExAcquireFastMutex(&DebugObject->Mutex);

        if (!(DebugObject->Flags & DEBUG_OBJECT_DELETE_PENDING))
        {
            DBGKTRACE(DBGK_MESSAGE_DEBUG, "Inserting: %p %d\n",
                DebugEvent, Message->ApiNumber);
            InsertTailList(&DebugObject->EventList, &DebugEvent->EventList);

            if (!NewEvent)
            {
                KeSetEvent(&DebugObject->EventsPresent,
                    IO_NO_INCREMENT,
                    FALSE);
            }

            Status = STATUS_SUCCESS;
        }
        else
        {
            Status = STATUS_DEBUGGER_INACTIVE;
        }

        ExReleaseFastMutex(&DebugObject->Mutex);
    }

    if (!NewEvent)
    {
        ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);

        if (NT_SUCCESS(Status))
        {
            KeWaitForSingleObject(&DebugEvent->ContinueEvent,
                Executive,
                KernelMode,
                FALSE,
                NULL);

            *Message = DebugEvent->ApiMsg;
            Status = DebugEvent->Status;
        }
    }
    else
    {
        if (!NT_SUCCESS(Status))
        {
            ObDereferenceObject(Thread);
            ObDereferenceObject(Process);
            ExFreePoolWithTag(DebugEvent, 'EgbD');
        }
    }

    DBGKTRACE(DBGK_MESSAGE_DEBUG, "Status: %lx\n", Status);
    return Status;
}

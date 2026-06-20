#include "DebugActiveProcess.h"
#include "DebugObjectCommonInternal.h"

NTSTATUS DbgkpSetProcessDebugObject(
    IN _EPROCESS* Process,
    IN PDEBUG_OBJECT DebugObject,
    IN NTSTATUS MsgStatus,
    IN _ETHREAD* LastThread)
{
    NTSTATUS Status;
    LIST_ENTRY TempList;
    BOOLEAN GlobalHeld = FALSE, DoSetEvent = TRUE;
    _ETHREAD* ThisThread, *FirstThread;
    PLIST_ENTRY NextEntry;
    PDEBUG_EVENT DebugEvent;
    _ETHREAD* EventThread;
    _ETHREAD* CurrentThread;
    PDEBUG_PROCESS DebugProcess;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;
    DBGKTRACE(DBGK_PROCESS_DEBUG, "Process: %p DebugObject: %p\n",
        Process, DebugObject);

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    CurrentThread = (_ETHREAD*)PsGetCurrentThread();
    size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;

    InitializeListHead(&TempList);

    if (NT_SUCCESS(MsgStatus))
    {
        Status = STATUS_SUCCESS;
    }
    else
    {
        LastThread = NULL;
        Status = MsgStatus;
    }

    if (NT_SUCCESS(Status))
    {
        for (;;)
        {
            GlobalHeld = TRUE;
            ExAcquireFastMutex(&DbgkpProcessDebugPortMutex);

            PDEBUG_OBJECT Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
            if (Port)
            {
                Status = STATUS_PORT_ALREADY_SET;
                break;
            }
            else
            {
                if (IsDbgkUnloadInProgress())
                {
                    Status = STATUS_DELETE_PENDING;
                    break;
                }

                BOOLEAN virtualDebugProcess = SetDebugTargetProcess(Process, DebugObject);
                if (IsDbgkUnloadInProgress())
                {
                    Status = STATUS_DELETE_PENDING;
                    break;
                }

                if (virtualDebugProcess || IsDebugTargetProcess(Process, &DebugProcess))
                {
                    *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                }
                else
                {
                    *(PDEBUG_OBJECT*)ptr_DebugPort = DebugObject;
                }
                ObReferenceObject(LastThread);

                ThisThread = PsGetNextProcessThread(Process, LastThread);
                if (ThisThread)
                {
                    *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                    ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);
                    GlobalHeld = FALSE;

                    ObDereferenceObject(LastThread);

                    Status = DbgkpPostFakeThreadMessages(Process,
                        DebugObject,
                        ThisThread,
                        &FirstThread,
                        &LastThread);
                    if (!NT_SUCCESS(Status))
                    {
                        LastThread = NULL;
                        break;
                    }
                    ObDereferenceObject(FirstThread);
                }
                else
                {
                    break;
                }
            }
        }
    }

    ExAcquireFastMutex(&DebugObject->Mutex);

    if (NT_SUCCESS(Status))
    {
        if (DebugObject->Flags & DEBUG_OBJECT_DELETE_PENDING)
        {
            *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
            Status = STATUS_DEBUGGER_INACTIVE;
        }
        else
        {
            if (!IsDebugTargetProcess(Process, &DebugProcess))
            {
                size_t ptr_TargetFlags = (size_t)Process + eprocess_offset::Flags;
                InterlockedOr((volatile LONG*)ptr_TargetFlags, 3);
            }

            if (IsDbgkUnloadInProgress())
            {
                *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                Status = STATUS_DELETE_PENDING;
            }
            else
            {
                ObReferenceObject(DebugObject);
                if (IsDebugTargetProcess(Process, &DebugProcess))
                {
                    DebugProcess->DebugObjectReferenced = TRUE;
                }
            }
        }
    }

    NextEntry = DebugObject->EventList.Flink;
    while (NextEntry != &DebugObject->EventList)
    {
        DebugEvent = CONTAINING_RECORD(NextEntry, DEBUG_EVENT, EventList);
        NextEntry = NextEntry->Flink;
        DBGKTRACE(DBGK_PROCESS_DEBUG, "DebugEvent: %p Flags: %lx TH: %p/%p\n",
            DebugEvent, DebugEvent->Flags,
            DebugEvent->BackoutThread, CurrentThread);

        if ((DebugEvent->Flags & DEBUG_EVENT_INACTIVE) &&
            (DebugEvent->BackoutThread == CurrentThread))
        {
            EventThread = DebugEvent->Thread;
            DBGKTRACE(DBGK_PROCESS_DEBUG, "EventThread: %p MsgStatus: %lx\n",
                EventThread, MsgStatus);

            if (NT_SUCCESS(Status))
            {
                if (DebugEvent->Flags & DEBUG_EVENT_PROTECT_FAILED)
                {
                    RemoveEntryList(&DebugEvent->EventList);
                    InsertTailList(&TempList, &DebugEvent->EventList);
                }
                else
                {
                    if (DoSetEvent)
                    {
                        DebugEvent->Flags &= ~DEBUG_EVENT_INACTIVE;
                        KeSetEvent(&DebugObject->EventsPresent,
                            IO_NO_INCREMENT,
                            FALSE);
                        DoSetEvent = FALSE;
                    }

                    DebugEvent->BackoutThread = NULL;
                }
            }
            else
            {
                RemoveEntryList(&DebugEvent->EventList);
                InsertTailList(&TempList, &DebugEvent->EventList);
            }

            if (DebugEvent->Flags & DEBUG_EVENT_RELEASE)
            {
                DebugEvent->Flags &= ~DEBUG_EVENT_RELEASE;
                size_t ptr_RundownProtect = (size_t)EventThread + ethread_offset::RundownProtect;
                ExReleaseRundownProtection((PEX_RUNDOWN_REF)ptr_RundownProtect);
            }
        }
    }

    ExReleaseFastMutex(&DebugObject->Mutex);

    if (GlobalHeld)
    {
        ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);
    }

    if (LastThread)
    {
        ObDereferenceObject(LastThread);
    }

    while (!IsListEmpty(&TempList))
    {
        NextEntry = RemoveHeadList(&TempList);
        DebugEvent = CONTAINING_RECORD(NextEntry, DEBUG_EVENT, EventList);
        DbgkpWakeTarget(DebugEvent);
    }

    if (NT_SUCCESS(Status))
    {
        DbgkpMarkProcessPeb(Process);
    }
    return Status;
}

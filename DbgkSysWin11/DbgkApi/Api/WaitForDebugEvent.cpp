#include "WaitForDebugEvent.h"
#include "DebugObjectCommonInternal.h"

EXTERN_C
NTSTATUS NtWaitForDebugEvent(
    IN HANDLE DebugHandle,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Timeout OPTIONAL,
    OUT PDBGUI_WAIT_STATE_CHANGE StateChange)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    LARGE_INTEGER LocalTimeOut;
    _EPROCESS* Process;
    LARGE_INTEGER StartTime;
    _ETHREAD* Thread;
    BOOLEAN GotEvent;
    LARGE_INTEGER NewTime;
    PDEBUG_OBJECT DebugObject;
    DBGUI_WAIT_STATE_CHANGE WaitStateChange;
    NTSTATUS Status;
    PDEBUG_EVENT DebugEvent = NULL, DebugEvent2;
    PLIST_ENTRY ListHead, NextEntry, NextEntry2;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    DBGKTRACE(DBGK_OBJECT_DEBUG, "Handle: %p\n", DebugHandle);

    RtlZeroMemory(&WaitStateChange, sizeof(WaitStateChange));
    LocalTimeOut.QuadPart = 0;

    if (PreviousMode != KernelMode)
    {
        _SEH2_TRY
        {
            if (Timeout)
            {
                ProbeForReadLargeInteger(Timeout);
                LocalTimeOut = *Timeout;
                Timeout = &LocalTimeOut;
            }

            ProbeForWrite(StateChange, sizeof(*StateChange), sizeof(ULONG));
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            _SEH2_YIELD(return _SEH2_GetExceptionCode());
        }
        _SEH2_END;
    }
    else
    {
        if (Timeout)
        {
            LocalTimeOut = *Timeout;
        }
    }

    if (Timeout)
    {
        KeQuerySystemTime(&StartTime);
    }

    Status = ObReferenceObjectByHandle(DebugHandle,
        DEBUG_READ_EVENT,
        Hvm_DbgkDebugObjectType,
        PreviousMode,
        (PVOID*)&DebugObject,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Process = NULL;
    Thread = NULL;

    while (TRUE)
    {
        Status = KeWaitForSingleObject(&DebugObject->EventsPresent,
            Executive,
            PreviousMode,
            Alertable,
            Timeout);

        if (!NT_SUCCESS(Status) ||
            (Status == STATUS_TIMEOUT) ||
            (Status == STATUS_ALERTED) ||
            (Status == STATUS_USER_APC))
        {
            break;
        }

        GotEvent = FALSE;
        ExAcquireFastMutex(&DebugObject->Mutex);

        if (DebugObject->Flags & DEBUG_OBJECT_DELETE_PENDING)
        {
            Status = STATUS_DEBUGGER_INACTIVE;
        }
        else
        {
            ListHead = &DebugObject->EventList;
            NextEntry = ListHead->Flink;
            while (ListHead != NextEntry)
            {
                DebugEvent = CONTAINING_RECORD(NextEntry,
                    DEBUG_EVENT,
                    EventList);
                DBGKTRACE(DBGK_PROCESS_DEBUG, "DebugEvent: %p Flags: %lx\n",
                    DebugEvent, DebugEvent->Flags);

                if (!(DebugEvent->Flags & (DEBUG_EVENT_INACTIVE | DEBUG_EVENT_READ)))
                {
                    GotEvent = TRUE;

                    NextEntry2 = DebugObject->EventList.Flink;
                    while (NextEntry2 != NextEntry)
                    {
                        DebugEvent2 = CONTAINING_RECORD(NextEntry2,
                            DEBUG_EVENT,
                            EventList);

                        if (DebugEvent2->ClientId.UniqueProcess ==
                            DebugEvent->ClientId.UniqueProcess)
                        {
                            DebugEvent->Flags |= DEBUG_EVENT_INACTIVE;
                            DebugEvent->BackoutThread = NULL;
                            GotEvent = FALSE;
                            break;
                        }

                        NextEntry2 = NextEntry2->Flink;
                    }

                    if (GotEvent)
                    {
                        break;
                    }
                }

                NextEntry = NextEntry->Flink;
            }

            if (GotEvent)
            {
                Process = DebugEvent->Process;
                Thread = DebugEvent->Thread;
                ObReferenceObject(Process);
                ObReferenceObject(Thread);

                DbgkpConvertKernelToUserStateChange(&WaitStateChange, DebugEvent);
                DebugEvent->Flags |= DEBUG_EVENT_READ;
            }
            else
            {
                KeResetEvent(&DebugObject->EventsPresent);
            }

            Status = STATUS_SUCCESS;
        }

        ExReleaseFastMutex(&DebugObject->Mutex);
        if (!NT_SUCCESS(Status))
        {
            break;
        }

        if (!GotEvent)
        {
            if (LocalTimeOut.QuadPart < 0)
            {
                KeQuerySystemTime(&NewTime);

                LocalTimeOut.QuadPart += (NewTime.QuadPart - StartTime.QuadPart);
                StartTime = NewTime;

                if (LocalTimeOut.QuadPart >= 0)
                {
                    Status = STATUS_TIMEOUT;
                    break;
                }
            }
        }
        else
        {
            DbgkpOpenHandles(&WaitStateChange, Process, Thread);
            ObDereferenceObject(Process);
            ObDereferenceObject(Thread);
            break;
        }
    }

    ObDereferenceObject(DebugObject);

    _SEH2_TRY
    {
        *StateChange = WaitStateChange;
    }
    _SEH2_EXCEPT(ExSystemExceptionFilter())
    {
        Status = _SEH2_GetExceptionCode();
    }
    _SEH2_END;

    return Status;
}

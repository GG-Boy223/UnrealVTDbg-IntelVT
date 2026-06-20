#include "WaitForDebugEvent.h"
#include "DebugObjectCommon.h"
#include "DebugObjectCommonInternal.h"
#include "../DbgkApi.h"
#include "../../ntos/inc/ntosdef.h"
#include "../../ntos/inc/ketypes.h"
#include "../../ntos/inc/amd64.h"
#include "../../ntos/inc/extypes.h"
#include "../../ntos/inc/mi.h"
#include "../../ntos/inc/pstypes.h"
#include "../../ntos/inc/obtypes.h"
#include "../../ntos/inc/peb_teb.h"
#include "../../ntos/inc/mmtypes.h"
#include "../../ntos/inc/ntdbg.h"
#include "../../List/MyList.h"
#include "../../ntos/inc/ntlpcapi.h"
#include "../../ntos/inc/ke.h"
#include "../../ntos/inc/ki.h"
#include "../../ntos/inc/pecoff.h"
#include "../../Log/log.h"
#include "../../ntos/inc/psp.h"
#include "../../Globals.h"
#include "../../Hvm/hypervisor_gateway.h"
#include "../../../Common/Ring0/String/StringHandler.h"

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
    DbgkRuntimeCallGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    DBGKTRACE(DBGK_OBJECT_DEBUG, "Handle: %p\n", DebugHandle);

    /* Clear the initial wait state change structure and the timeout */
    RtlZeroMemory(&WaitStateChange, sizeof(WaitStateChange));
    LocalTimeOut.QuadPart = 0;

    /* Check if we were called from user mode */
    if (PreviousMode != KernelMode)
    {
        /* Protect probe in SEH */
        _SEH2_TRY
        {
            /* Check if we came with a timeout */
            if (Timeout)
            {
                /* Probe it */
                ProbeForReadLargeInteger(Timeout);

                /* Make a local copy */
                LocalTimeOut = *Timeout;
                Timeout = &LocalTimeOut;
            }

        /* Probe the state change structure */
        ProbeForWrite(StateChange, sizeof(*StateChange), sizeof(ULONG));
        }
            _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            /* Return the exception code */
            _SEH2_YIELD(return _SEH2_GetExceptionCode());
        }
        _SEH2_END;
    }
    else
    {
        /* Copy directly */
        if (Timeout) LocalTimeOut = *Timeout;
    }

    /* If we were passed a timeout, query the current time */
    if (Timeout) KeQuerySystemTime(&StartTime);

    /* Get the debug object */
    Status = ObReferenceObjectByHandle(DebugHandle,
        DEBUG_READ_EVENT,
        Hvm_DbgkDebugObjectType,
        PreviousMode,
        (PVOID*)&DebugObject,
        NULL);
    if (!NT_SUCCESS(Status)) return Status;

    /* Clear process and thread */
    Process = NULL;
    Thread = NULL;

    /* Wait on the debug object given to us */
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
            /* Break out the wait */
            break;
        }

        /* Lock the object */
        GotEvent = FALSE;
        ExAcquireFastMutex(&DebugObject->Mutex);

        /* Check if a debugger is connected */
        if (DebugObject->Flags & DEBUG_OBJECT_DELETE_PENDING)
        {
            /* Not connected */
            Status = STATUS_DEBUGGER_INACTIVE;
        }
        else
        {
            /* Loop the events */
            ListHead = &DebugObject->EventList;
            NextEntry = ListHead->Flink;
            while (ListHead != NextEntry)
            {
                /* Get the debug event */
                DebugEvent = CONTAINING_RECORD(NextEntry,
                    DEBUG_EVENT,
                    EventList);
                DBGKTRACE(DBGK_PROCESS_DEBUG, "DebugEvent: %p Flags: %lx\n",
                    DebugEvent, DebugEvent->Flags);

                /* Check flags */
                if (!(DebugEvent->Flags & (DEBUG_EVENT_INACTIVE | DEBUG_EVENT_READ)))
                {
                    /* We got an event */
                    GotEvent = TRUE;

                    /* Loop the list internally */
                    NextEntry2 = DebugObject->EventList.Flink;
                    while (NextEntry2 != NextEntry)
                    {
                        /* Get the debug event */
                        DebugEvent2 = CONTAINING_RECORD(NextEntry2,
                            DEBUG_EVENT,
                            EventList);

                        /* Try to match process IDs */
                        if (DebugEvent2->ClientId.UniqueProcess ==
                            DebugEvent->ClientId.UniqueProcess)
                        {
                            /* Found it, break out */
                            DebugEvent->Flags |= DEBUG_EVENT_INACTIVE;
                            DebugEvent->BackoutThread = NULL;
                            GotEvent = FALSE;
                            break;
                        }

                        /* Move to the next entry */
                        NextEntry2 = NextEntry2->Flink;
                    }

                    /* Check if we still have a valid event */
                    if (GotEvent) break;
                }

                /* Move to the next entry */
                NextEntry = NextEntry->Flink;
            }

            /* Check if we have an event */
            if (GotEvent)
            {
                /* Save and reference the process and thread */
                Process = DebugEvent->Process;
                Thread = DebugEvent->Thread;
                ObReferenceObject(Process);
                ObReferenceObject(Thread);

                /* Convert to user-mode structure */
                DbgkpConvertKernelToUserStateChange(&WaitStateChange, DebugEvent);

                /* Set flag */
                DebugEvent->Flags |= DEBUG_EVENT_READ;
            }
            else
            {
                /* Unsignal the event */
                KeResetEvent(&DebugObject->EventsPresent);
            }

            /* Set success */
            Status = STATUS_SUCCESS;
        }

        /* Release the mutex */
        ExReleaseFastMutex(&DebugObject->Mutex);
        if (!NT_SUCCESS(Status)) break;

        /* Check if we got an event */
        if (!GotEvent)
        {
            /* Check if we can wait again */
            if (LocalTimeOut.QuadPart < 0)
            {
                /* Query the new time */
                KeQuerySystemTime(&NewTime);

                /* Substract times */
                LocalTimeOut.QuadPart += (NewTime.QuadPart - StartTime.QuadPart);
                StartTime = NewTime;

                /* Check if we've timed out */
                if (LocalTimeOut.QuadPart >= 0)
                {
                    /* We have, break out of the loop */
                    Status = STATUS_TIMEOUT;
                    break;
                }
            }
        }
        else
        {
            /* Open the handles and dereference the objects */
            DbgkpOpenHandles(&WaitStateChange, Process, Thread);
            ObDereferenceObject(Process);
            ObDereferenceObject(Thread);
            break;
        }
    }

    /* We're done, dereference the object */
    ObDereferenceObject(DebugObject);

    /* Protect write with SEH */
    _SEH2_TRY
    {
        /* Return our wait state change structure */
        *StateChange = WaitStateChange;
    }
        _SEH2_EXCEPT(ExSystemExceptionFilter())
    {
        /* Get SEH Exception code */
        Status = _SEH2_GetExceptionCode();
    }
    _SEH2_END;

    /* Return status */
    return Status;
}

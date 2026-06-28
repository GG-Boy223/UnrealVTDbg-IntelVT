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
    _EPROCESS* Process; //进程对象
    LARGE_INTEGER StartTime;
    _ETHREAD* Thread;
    BOOLEAN GotEvent;
    LARGE_INTEGER NewTime;
    PDEBUG_OBJECT DebugObject;
    DBGUI_WAIT_STATE_CHANGE WaitStateChange;
    NTSTATUS Status; // eax
    PDEBUG_EVENT DebugEvent = NULL, DebugEvent2;
    PLIST_ENTRY ListHead, NextEntry, NextEntry2;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    DBGKTRACE(DBGK_OBJECT_DEBUG, "Handle: %p\n", DebugHandle);

    // /* Clear the initial wait state change structure and the timeout */
    RtlZeroMemory(&WaitStateChange, sizeof(WaitStateChange));
    LocalTimeOut.QuadPart = 0;

    // /* Check if we were called from user mode */
    if (PreviousMode != KernelMode)
    {
        _SEH2_TRY
        {
            // /* Check if we came with a timeout */
            if (Timeout)
            {
                // /* Probe it */
                ProbeForReadLargeInteger(Timeout);
                // /* Make a local copy */
                LocalTimeOut = *Timeout;
                Timeout = &LocalTimeOut;
            }

            // /* Probe the state change structure */
            ProbeForWrite(StateChange, sizeof(*StateChange), sizeof(ULONG));
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            // /* Return the exception code */
            _SEH2_YIELD(return _SEH2_GetExceptionCode());
        }
        _SEH2_END;
    }
    else
    {
        // /* Check if we came with a timeout */
        if (Timeout)
        {
            // /* Make a local copy */
            LocalTimeOut = *Timeout;
        }
    }

    // /* Check if we came with a timeout */
    if (Timeout)
    {
        KeQuerySystemTime(&StartTime);
    }

    // /* Get the debug object */
    Status = ObReferenceObjectByHandle(DebugHandle, //根据句柄引用调试对象
        DEBUG_READ_EVENT,
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

    // /* Clear process and thread */
    Process = NULL;
    Thread = NULL;

    // /* Wait on the debug object given to us */
    while (TRUE)
    {
        Status = KeWaitForSingleObject(&DebugObject->EventsPresent, //等待调试事件
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

        // /* Lock the object */
        GotEvent = FALSE;
        ExAcquireFastMutex(&DebugObject->Mutex); //加锁

        // /* Check if a debugger is connected */
        if (DebugObject->Flags & DEBUG_OBJECT_DELETE_PENDING) //检查调试器是否已连接
        {
            Status = STATUS_DEBUGGER_INACTIVE;
        }
        else
        {
            // /* Loop the events */
            ListHead = &DebugObject->EventList;
            NextEntry = ListHead->Flink;
            while (ListHead != NextEntry)
            {
                // /* Get the debug event */
                DebugEvent = CONTAINING_RECORD(NextEntry,
                    DEBUG_EVENT,
                    EventList);
                DBGKTRACE(DBGK_PROCESS_DEBUG, "DebugEvent: %p Flags: %lx\n",
                    DebugEvent, DebugEvent->Flags);

                // /* Check flags */
                if (!(DebugEvent->Flags & (DEBUG_EVENT_INACTIVE | DEBUG_EVENT_READ)))
                {
                    // /* We got an event */
                    GotEvent = TRUE;

                    // /* Loop the list internally */
                    NextEntry2 = DebugObject->EventList.Flink;
                    while (NextEntry2 != NextEntry)
                    {
                        // /* Get the debug event */
                        DebugEvent2 = CONTAINING_RECORD(NextEntry2,
                            DEBUG_EVENT,
                            EventList);

                        // /* Try to match process IDs */
                        if (DebugEvent2->ClientId.UniqueProcess ==
                            DebugEvent->ClientId.UniqueProcess)
                        {
                            // /* Found it, break out */
                            DebugEvent->Flags |= DEBUG_EVENT_INACTIVE;
                            // /* Clear the backout thread */
                            DebugEvent->BackoutThread = NULL;
                            // /* Lock the object */
                            GotEvent = FALSE;
                            break;
                        }

                        // /* Move to the next entry */
                        NextEntry2 = NextEntry2->Flink;
                    }

                    // /* Check if we have an event */
                    if (GotEvent)
                    {
                        break;
                    }
                }

                // /* Move to the next entry */
                NextEntry = NextEntry->Flink;
            }

            // /* Check if we have an event */
            if (GotEvent)
            {
                // /* Save and reference the process and thread */
                Process = DebugEvent->Process;
                Thread = DebugEvent->Thread;
                ObReferenceObject(Process);
                // /* Reference the thread and process */
                ObReferenceObject(Thread);

                // /* Convert to user-mode structure */
                DbgkpConvertKernelToUserStateChange(&WaitStateChange, DebugEvent);
                // /* Set flag */
                DebugEvent->Flags |= DEBUG_EVENT_READ;
            }
            else
            {
                // /* Unsignal the event */
                KeResetEvent(&DebugObject->EventsPresent);
            }

            // /* Set success */
            Status = STATUS_SUCCESS;
        }

        ExReleaseFastMutex(&DebugObject->Mutex);
        // /* Check if we failed */
        if (!NT_SUCCESS(Status))
        {
            break;
        }

        // /* Check if we got an event */
        if (!GotEvent)
        {
            // /* Check if we can wait again */
            if (LocalTimeOut.QuadPart < 0)
            {
                // /* Query the new time */
                KeQuerySystemTime(&NewTime);

                // /* Substract times */
                LocalTimeOut.QuadPart += (NewTime.QuadPart - StartTime.QuadPart);
                StartTime = NewTime;

                // /* Check if we've timed out */
                if (LocalTimeOut.QuadPart >= 0)
                {
                    // /* We have, break out of the loop */
                    Status = STATUS_TIMEOUT;
                    break;
                }
            }
        }
        else
        {
            // /* Open the handles and dereference the objects */
            DbgkpOpenHandles(&WaitStateChange, Process, Thread); //获得进线程句柄返回给Ring3
            ObDereferenceObject(Process); //释放进程对象
            ObDereferenceObject(Thread); //释放线程对象
            break;
        }
    }

    // /* We're done, dereference the object */
    ObDereferenceObject(DebugObject); //释放调试对象

    // /* Protect write with SEH */
    _SEH2_TRY
    {
        *StateChange = WaitStateChange;
    }
    _SEH2_EXCEPT(ExSystemExceptionFilter())
    {
        // /* Get SEH Exception code */
        Status = _SEH2_GetExceptionCode();
    }
    _SEH2_END;

    // /* Return status */
    return Status;
}

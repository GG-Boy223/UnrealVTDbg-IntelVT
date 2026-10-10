#include "DebugActiveProcess.h"
#include "DebugObjectCommonInternal.h"

NTSTATUS DbgkpSetProcessDebugObject(
    IN _EPROCESS* Process,
    IN PDEBUG_OBJECT DebugObject,
    IN NTSTATUS MsgStatus,
    IN _ETHREAD* LastThread)
{
    NTSTATUS Status; // eax
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

    //DbgBreakPoint();
    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    CurrentThread = (_ETHREAD*)PsGetCurrentThread();
    size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;

    // /* Initialize the temporary list */
    InitializeListHead(&TempList);

    // /* Check if we have a success message */
    if (NT_SUCCESS(MsgStatus))
    {
        // /* Then default to STATUS_SUCCESS */
        Status = STATUS_SUCCESS;
    }
    else
    {
        // /* No last thread, and set the failure code */
        LastThread = NULL;
        Status = MsgStatus;
    }

    // /* Now check what status we have here */
    if (NT_SUCCESS(Status))
    {
        for (;;)
        {
            // /* Acquire the global lock */
            GlobalHeld = TRUE;
            // /* Acquire the port lock */
            ExAcquireFastMutex(&DbgkpProcessDebugPortMutex);

            // /* Check if we already have a port */
            PDEBUG_OBJECT Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
            if (Port)
            {
                // 不空则说明已经设置了
                // /* Set failure */
                Status = STATUS_PORT_ALREADY_SET;
                break;
            }
            else
            {
                // /* Otherwise, set the port and reference the thread */
                if (IsDbgkUnloadInProgress())
                {
                    // *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                    Status = STATUS_DELETE_PENDING;
                    break;
                }

                BOOLEAN virtualDebugProcess = SetDebugTargetProcess(Process, DebugObject);
                if (IsDbgkUnloadInProgress())
                {
                    // *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
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

                // /* Get the next thread */
                ThisThread = PsGetNextProcessThread(Process, LastThread);
                if (ThisThread)
                {
                    *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                    // /* Clear the debug port and release the lock */
                    // *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                    ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);
                    GlobalHeld = FALSE;

                    // /* Dereference the thread */
                    ObDereferenceObject(LastThread);

                    // /* Post fake messages */
                    Status = DbgkpPostFakeThreadMessages(Process,
                        DebugObject,
                        ThisThread,
                        &FirstThread,
                        &LastThread);
                    // /* Check if we failed */
                    if (!NT_SUCCESS(Status))
                    {
                        // /* Clear the last thread */
                        LastThread = NULL;
                        break;
                    }
                    // /* Dereference the first thread and re-acquire the lock */
                    ObDereferenceObject(FirstThread);
                }
                else
                {
                    break;
                }
            }
        }
    }

    // /* Acquire the debug object's lock */
    ExAcquireFastMutex(&DebugObject->Mutex);

    // /* Check our status here */
    if (NT_SUCCESS(Status))
    {
        // /* Check if we're disconnected */
        if (DebugObject->Flags & DEBUG_OBJECT_DELETE_PENDING)
        {
            *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
            // /* Set status */
            // *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
            Status = STATUS_DEBUGGER_INACTIVE;
        }
        else
        {
            // /* Set the process flags */
            if (!IsDebugTargetProcess(Process, &DebugProcess))
            {
                size_t ptr_TargetFlags = (size_t)Process + eprocess_offset::Flags;
                InterlockedOr((volatile LONG*)ptr_TargetFlags, 3); // set CreateReported & NoDebugInherit
            }

            if (IsDbgkUnloadInProgress())
            {
                *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                // *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                Status = STATUS_DELETE_PENDING;
            }
            else
            {
                // /* Reference the debug object */
                ObReferenceObject(DebugObject);
                if (IsDebugTargetProcess(Process, &DebugProcess))
                {
                    // 表项所有权登记必须与卸载摘链互斥：登记失败说明表项已被
                    // 卸载事务摘除/释放，立即返还本次引用并按调试会话不可用处理，
                    // 避免把标志写进已释放的池块或泄漏 DEBUG_OBJECT 引用。
                    if (!MarkDebugProcessDebugObjectReferenced(Process, DebugObject))
                    {
                        ObDereferenceObject(DebugObject);
                        Status = STATUS_DEBUGGER_INACTIVE;
                    }
                }
            }
        }
    }

    // /* Loop the events list */
    NextEntry = DebugObject->EventList.Flink;
    while (NextEntry != &DebugObject->EventList)
    {
        // /* Get the debug event and go to the next entry */
        DebugEvent = CONTAINING_RECORD(NextEntry, DEBUG_EVENT, EventList);
        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
        DBGKTRACE(DBGK_PROCESS_DEBUG, "DebugEvent: %p Flags: %lx TH: %p/%p\n",
            DebugEvent, DebugEvent->Flags,
            DebugEvent->BackoutThread, CurrentThread);

        // /* Check for if the debug event queue needs flushing */
        if ((DebugEvent->Flags & DEBUG_EVENT_INACTIVE) &&
            (DebugEvent->BackoutThread == CurrentThread))
        {
            // /* Get the event's thread */
            EventThread = DebugEvent->Thread;
            DBGKTRACE(DBGK_PROCESS_DEBUG, "EventThread: %p MsgStatus: %lx\n",
                EventThread, MsgStatus);

            // /* Check if the status is success */
            if (NT_SUCCESS(Status))
            {
                // /* Check if we couldn't acquire rundown for it */
                if (DebugEvent->Flags & DEBUG_EVENT_PROTECT_FAILED)
                {
                    // /* Insert it into the temp list */
                    RemoveEntryList(&DebugEvent->EventList);
                    InsertTailList(&TempList, &DebugEvent->EventList);
                }
                else
                {
                    // /* Do we need to signal the event */
                    if (DoSetEvent)
                    {
                        // /* Do it */
                        DebugEvent->Flags &= ~DEBUG_EVENT_INACTIVE;
                        // /* Signal it */
                        KeSetEvent(&DebugObject->EventsPresent,
                            IO_NO_INCREMENT,
                            FALSE);
                        DoSetEvent = FALSE;
                    }

                    // /* Clear the backout thread */
                    DebugEvent->BackoutThread = NULL;
                }
            }
            else
            {
                // /* Insert it into the temp list */
                RemoveEntryList(&DebugEvent->EventList);
                InsertTailList(&TempList, &DebugEvent->EventList);
            }

            // /* Check if the lock is held */
            if (DebugEvent->Flags & DEBUG_EVENT_RELEASE)
            {
                // /* Release it */
                DebugEvent->Flags &= ~DEBUG_EVENT_RELEASE;
                size_t ptr_RundownProtect = (size_t)EventThread + ethread_offset::RundownProtect;
                // /* Release rundown protection */
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
        // /* Dereference the thread */
        ObDereferenceObject(LastThread);
    }

    // /* Loop our temporary list */
    while (!IsListEmpty(&TempList))
    {
        // /* Remove the event */
        NextEntry = RemoveHeadList(&TempList);
        // /* Get the debug event and go to the next entry */
        DebugEvent = CONTAINING_RECORD(NextEntry, DEBUG_EVENT, EventList);
        // /* Wake it */
        DbgkpWakeTarget(DebugEvent);
    }

    // /* Now check what status we have here */
    if (NT_SUCCESS(Status))
    {
        DbgkpMarkProcessPeb(Process); //清理进程的peb结构
    }
    // /* Return status */
    return Status;
}

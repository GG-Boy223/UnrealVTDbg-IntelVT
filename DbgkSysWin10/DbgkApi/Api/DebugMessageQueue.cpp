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
    NTSTATUS Status; // eax
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

    // /* Check if we have to allocate a debug event */
    NewEvent = (Flags & DEBUG_EVENT_NOWAIT) ? TRUE : FALSE;
    if (NewEvent)
    {
#pragma warning(push)
#pragma warning(disable: 4996)
        DebugEvent = (PDEBUG_EVENT)ExAllocatePoolWithTag(
            NonPagedPool,
            sizeof(DEBUG_EVENT),
            'EgbD');
#pragma warning(pop)
        if (!DebugEvent)
        {
            return STATUS_INSUFFICIENT_RESOURCES; // 显式返回内存不足，避免隐式成功。
        }

        // /* Set flags */
        DebugEvent->Flags = Flags | DEBUG_EVENT_INACTIVE;
        // /* Reference the thread and process */
        ObReferenceObject(Thread);
        ObReferenceObject(Process);
        // /* Set the current thread */
        DebugEvent->BackoutThread = (_ETHREAD*)PsGetCurrentThread();
        // /* Set the debug object */
        //if (IsDebugTargetProcess((_EPROCESS*)Process, &DebugProcess))
        //    DebugObject = DebugProcess->DebugObject;
        //else
        DebugObject = TargetObject;
    }
    else
    {
        // /* Use the debug event on the stack */
        DebugEvent = &LocalDebugEvent;
        DebugEvent->Flags = Flags;

        // /* Acquire the port lock */
        ExAcquireFastMutex(&DbgkpProcessDebugPortMutex);

        // /* Get the debug object */
        if (IsDebugTargetProcess(Process, &DebugProcess))
        {
            DebugObject = DebugProcess->DebugObject;
        }
        else
        {
            size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
            DebugObject = *(PDEBUG_OBJECT*)ptr_DebugPort;
        }

        // /* Check what kind of API message this is */
        switch (Message->ApiNumber)
        {
        // /* Process or thread creation */
        case DbgKmCreateThreadApi:
        case DbgKmCreateProcessApi:
        // /* Process or thread exit */
        case DbgKmExitThreadApi:
        case DbgKmExitProcessApi:
        case DbgKmLoadDllApi:
        // /* No special handling for other messages */
        default:
            break;
        }
    }

    // /* Setup the Debug Event */
    KeInitializeEvent(&DebugEvent->ContinueEvent, SynchronizationEvent, FALSE);
    DebugEvent->Process = Process;
    DebugEvent->Thread = Thread;
    DebugEvent->ApiMsg = *Message;

    //DbgBreakPoint();
    size_t ptr_Cid = (size_t)Thread + ethread_offset::Cid;
    DebugEvent->ClientId = *(CLIENT_ID*)ptr_Cid;

    // /* Check if we have a port object */
    if (!DebugObject)
    {
        // /* Fail */
        Status = STATUS_PORT_NOT_SET;
    }
    else
    {
        // /* Acquire the debug object mutex */
        ExAcquireFastMutex(&DebugObject->Mutex);

        // /* Check if a debugger is active */
        if (!(DebugObject->Flags & DEBUG_OBJECT_DELETE_PENDING))
        {
            // /* Add the event into the object's list */
            DBGKTRACE(DBGK_MESSAGE_DEBUG, "Inserting: %p %d\n",
                DebugEvent, Message->ApiNumber);
            InsertTailList(&DebugObject->EventList, &DebugEvent->EventList); //将调试事件插入到事件队列

            // /* Check if we have to signal it */
            if (!NewEvent)
            {
                // /* Signal it */
                KeSetEvent(&DebugObject->EventsPresent,
                    IO_NO_INCREMENT,
                    FALSE);
            }

            // /* Set success */
            Status = STATUS_SUCCESS;
        }
        else
        {
            Status = STATUS_DEBUGGER_INACTIVE;
        }

        // /* Release the object lock */
        ExReleaseFastMutex(&DebugObject->Mutex);
    }

    // /* Check if we had acquired the port lock */
    if (!NewEvent)
    {
        // /* Release it */
        ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);

        // /* Check if we got here through success */
        if (NT_SUCCESS(Status))
        {
            //DbgBreakPoint();
            // /* Wait on the continue event */
            KeWaitForSingleObject(&DebugEvent->ContinueEvent,
                Executive,
                KernelMode,
                FALSE,
                NULL);

            *Message = DebugEvent->ApiMsg;
            // /* Set return status */
            Status = DebugEvent->Status;
        }
    }
    else
    {
        // /* Check if we failed */
        if (!NT_SUCCESS(Status))
        {
            // /* Dereference the process and thread */
            ObDereferenceObject(Thread);
            ObDereferenceObject(Process); //释放进程对象
            ExFreePoolWithTag(DebugEvent, 'EgbD');
        }
    }

    // /* Return status */
    DBGKTRACE(DBGK_MESSAGE_DEBUG, "Status: %lx\n", Status);
    // /* Return status */
    return Status;
}

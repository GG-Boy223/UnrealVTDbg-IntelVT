#include "DebugObjectCommonInternal.h"

VOID WakeDebugObjectEvents(PDEBUG_OBJECT DebugObject)
{
    if (DebugObject == NULL)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    LIST_ENTRY tempList;
    PLIST_ENTRY entry;
    BOOLEAN lockHeld = FALSE;

    InitializeListHead(&tempList);

    __try
    {
        ExAcquireFastMutex(&DebugObject->Mutex);
        lockHeld = TRUE;

        // Mark this object as going away and wake up any processes that are waiting.
        //
        DebugObject->Flags |= DEBUG_OBJECT_DELETE_PENDING;
        while (!IsListEmpty(&DebugObject->EventList))
        {
            entry = RemoveHeadList(&DebugObject->EventList);
            InsertTailList(&tempList, entry);
        }

        ExReleaseFastMutex(&DebugObject->Mutex);
        lockHeld = FALSE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (lockHeld)
        {
            ExReleaseFastMutex(&DebugObject->Mutex);
        }
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    // Wake anyone waiting. They need to leave this object alone now as its deleting
    //
    KeSetEvent(&DebugObject->EventsPresent, 0, FALSE);

    while (!IsListEmpty(&tempList))
    {
        entry = RemoveHeadList(&tempList);
        PDEBUG_EVENT debugEvent = CONTAINING_RECORD(entry, DEBUG_EVENT, EventList);
        debugEvent->Status = STATUS_DEBUGGER_INACTIVE;
        if (DbgkpWakeTarget != NULL)
        {
            DbgkpWakeTarget(debugEvent);
        }
    }
}

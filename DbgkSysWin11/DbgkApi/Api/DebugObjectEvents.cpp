#include "DebugObjectCommonInternal.h"

VOID WakeDebugObjectEvents(PDEBUG_OBJECT DebugObject)
{
    if (DebugObject == NULL)
    {
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
        return;
    }

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

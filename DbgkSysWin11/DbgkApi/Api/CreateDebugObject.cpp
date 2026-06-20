#include "CreateDebugObject.h"
#include "DebugObjectCommonInternal.h"

namespace
{
    const DWORD kMinObjectTypeIndex = 2;
    const DWORD kMaxObjectTypeIndex = 255;

    BOOLEAN SafeObjectTypeNameEquals(POBJECT_TYPE objectType, PUNICODE_STRING name)
    {
        if (objectType == NULL || name == NULL)
        {
            return FALSE;
        }

        __try
        {
            if (objectType->Name.Buffer == NULL)
            {
                return FALSE;
            }

            return RtlEqualUnicodeString(&objectType->Name, name, FALSE);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return FALSE;
        }
    }
}

POBJECT_TYPE GetDebugObjectType(UNICODE_STRING Name)
{
    if (ObTypeIndexTable == NULL)
    {
        outLog("\x47\x65\x74\x44\x65\x62\x75\x67\x4F\x62\x6A\x65\x63\x74\x54\x79\x70\x65\x20\xCA\xA7\xB0\xDC\xA3\xBA\x4F\x62\x54\x79\x70\x65\x49\x6E\x64\x65\x78\x54\x61\x62\x6C\x65\x20\xCE\xB4\xBD\xE2\xCE\xF6\x2E");
        return NULL;
    }

    for (DWORD TypeIndex = kMinObjectTypeIndex; TypeIndex <= kMaxObjectTypeIndex; TypeIndex++)
    {
        POBJECT_TYPE objectType = NULL;

        __try
        {
            objectType = ObTypeIndexTable[TypeIndex];
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return NULL;
        }

        if (objectType == NULL)
        {
            continue;
        }

        if (SafeObjectTypeNameEquals(objectType, &Name))
        {
            return objectType;
        }
    }

    return NULL;
}

EXTERN_C
NTSTATUS NtCreateDebugObject(
    OUT PHANDLE DebugHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN ULONG Flags)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    PDEBUG_OBJECT DebugObject = NULL;
    HANDLE hDebug = NULL;
    NTSTATUS Status;
    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    if (PreviousMode != KernelMode)
    {
        _SEH2_TRY
        {
            ProbeForWriteHandle(DebugHandle);
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            _SEH2_YIELD(return _SEH2_GetExceptionCode());
        }
        _SEH2_END;
    }

    if (Flags & ~DBGK_ALL_FLAGS)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObCreateObject(PreviousMode,
        Hvm_DbgkDebugObjectType,
        ObjectAttributes,
        PreviousMode,
        NULL,
        sizeof(DEBUG_OBJECT),
        0,
        0,
        (PVOID*)&DebugObject);
    if (NT_SUCCESS(Status))
    {
        ExInitializeFastMutex(&DebugObject->Mutex);
        InitializeListHead(&DebugObject->EventList);
        KeInitializeEvent(&DebugObject->EventsPresent,
            NotificationEvent,
            FALSE);

        if (Flags & DBGK_KILL_PROCESS_ON_EXIT)
        {
            DebugObject->Flags = DEBUG_OBJECT_KILL_ON_CLOSE;
        }
        else
        {
            DebugObject->Flags = 0;
        }

        Status = ObInsertObject((PVOID)DebugObject,
            NULL,
            DesiredAccess,
            0,
            NULL,
            &hDebug);
        if (NT_SUCCESS(Status))
        {
            _SEH2_TRY
            {
                *DebugHandle = hDebug;
            }
            _SEH2_EXCEPT(ExSystemExceptionFilter())
            {
                Status = _SEH2_GetExceptionCode();
            }
            _SEH2_END;
        }
    }

    DBGKTRACE(DBGK_OBJECT_DEBUG, "Handle: %p DebugObject: %p\n",
        hDebug, DebugObject);
    return Status;
}

EXTERN_C
VOID
DbgkpCloseObject(
    IN _EPROCESS* Process,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG_PTR SystemHandleCount)
{
    PDEBUG_OBJECT DebugObject = (PDEBUG_OBJECT)Object;
    PDEBUG_EVENT DebugEvent;
    PLIST_ENTRY ListPtr;
    BOOLEAN Deref;
    PDEBUG_OBJECT Port;
    PDEBUG_OBJECT ListDebugObject;
    size_t ptr_DebugPort = NULL;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    UNREFERENCED_PARAMETER(GrantedAccess);

    if (SystemHandleCount > 1)
    {
        return;
    }

    if (IsDbgkUnloadInProgress())
    {
        WakeDebugObjectEvents(DebugObject);
        return;
    }

    ExAcquireFastMutex(&DebugObject->Mutex);
    DebugObject->Flags |= DEBUG_OBJECT_DELETE_PENDING;
    ListPtr = DebugObject->EventList.Flink;
    InitializeListHead(&DebugObject->EventList);
    ExReleaseFastMutex(&DebugObject->Mutex);

    KeSetEvent(&DebugObject->EventsPresent, 0, FALSE);

    for (Process = PsGetNextProcess(NULL);
        Process != NULL;
        Process = PsGetNextProcess(Process))
    {

        if (LookupDebugTargetProcessObject(Process, &ListDebugObject))
        {
            Port = ListDebugObject;
            ptr_DebugPort = NULL;
        }
        else
        {
            if (eprocess_offset::DebugPort == 0xFFFFFFFF)
            {
                continue;
            }

            ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
            Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
        }

        if (Port == DebugObject)
        {
            Deref = FALSE;
            ExAcquireFastMutex(&DbgkpProcessDebugPortMutex);
            if (Port == DebugObject)
            {
                if (ptr_DebugPort)
                {
                    *(PDEBUG_OBJECT*)ptr_DebugPort = NULL;
                }
                Deref = TRUE;
            }
            ExReleaseFastMutex(&DbgkpProcessDebugPortMutex);

            if (Deref)
            {
                DbgkpMarkProcessPeb(Process);
                if (DebugObject->Flags & DEBUG_OBJECT_KILL_ON_CLOSE)
                {
                    DbgPrint("\xBD\xE1\xCA\xF8\xBD\xF8\xB3\xCC");
                    PsTerminateProcess(Process, STATUS_DEBUGGER_INACTIVE);
                }
                ObDereferenceObject(DebugObject);
            }
        }
    }

    while (ListPtr != &DebugObject->EventList)
    {
        DebugEvent = CONTAINING_RECORD(ListPtr, DEBUG_EVENT, EventList);
        ListPtr = ListPtr->Flink;
        DebugEvent->Status = STATUS_DEBUGGER_INACTIVE;
        DbgkpWakeTarget(DebugEvent);
    }

    DbgPrint("\xD2\xC6\xB3\xFD\xB5\xF7\xCA\xD4\xB6\xD4\xCF\xF3");
    DeleteDebugProcess(DebugObject);
}

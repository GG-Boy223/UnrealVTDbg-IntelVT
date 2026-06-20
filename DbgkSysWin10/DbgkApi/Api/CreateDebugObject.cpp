#include "CreateDebugObject.h"
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
        outLog("GetDebugObjectType 失败：ObTypeIndexTable 未初始化.");
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
    PDEBUG_OBJECT DebugObject;
    HANDLE hDebug = NULL;
    NTSTATUS Status;
    PAGED_CODE();
    DbgkRuntimeCallGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    /* Check if we were called from user mode*/
    if (PreviousMode != KernelMode)
    {
        /* Enter SEH for probing */
        _SEH2_TRY
        {
            /* Probe the handle */
            ProbeForWriteHandle(DebugHandle);
        }
            _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            /* Return the exception code */
            _SEH2_YIELD(return _SEH2_GetExceptionCode());
        } _SEH2_END;
    }

    /* Check for invalid flags */
    if (Flags & ~DBGK_ALL_FLAGS) return STATUS_INVALID_PARAMETER;

    /* Create the Object */
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
        /* Initialize the Debug Object's Fast Mutex */
        ExInitializeFastMutex(&DebugObject->Mutex);

        /* Initialize the State Event List */
        InitializeListHead(&DebugObject->EventList);

        /* Initialize the Debug Object's Wait Event */
        KeInitializeEvent(&DebugObject->EventsPresent,
            NotificationEvent,
            FALSE);

        /* Set the Flags */
        if (Flags & DBGK_KILL_PROCESS_ON_EXIT)
        {
            DebugObject->Flags = DEBUG_OBJECT_KILL_ON_CLOSE;
        }
        else
        {
            DebugObject->Flags = 0;
        }

        /* Insert it */
        Status = ObInsertObject((PVOID)DebugObject,
            NULL,
            DesiredAccess,
            0,
            NULL,
            &hDebug);
        if (NT_SUCCESS(Status))
        {
            /* Enter SEH to protect the write */
            _SEH2_TRY
            {
                /* Return the handle */
                *DebugHandle = hDebug;
            }
                _SEH2_EXCEPT(ExSystemExceptionFilter())
            {
                /* Get the exception code */
                Status = _SEH2_GetExceptionCode();
            } _SEH2_END;
        }
    }

    /* Return Status */
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
    IN ULONG_PTR SystemHandleCount
)
{
    PDEBUG_OBJECT DebugObject = (PDEBUG_OBJECT)Object;
    PDEBUG_EVENT DebugEvent;
    PLIST_ENTRY ListPtr;
    BOOLEAN Deref;
    PDEBUG_OBJECT Port;
    PDEBUG_OBJECT ListDebugObject;
    size_t ptr_DebugPort = NULL;

    PAGED_CODE();
    DbgkRuntimeCallGuard stateGuard;

    UNREFERENCED_PARAMETER(GrantedAccess);

    // If this isn't the last handle then do nothing.
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

    // Mark this object as going away and wake up any processes that are waiting.
    DebugObject->Flags |= DEBUG_OBJECT_DELETE_PENDING;

    // Remove any events and queue them to a temporary queue
    ListPtr = DebugObject->EventList.Flink;
    InitializeListHead(&DebugObject->EventList);

    ExReleaseFastMutex(&DebugObject->Mutex);

    // Wake anyone waiting. They need to leave this object alone now as its deleting
    KeSetEvent(&DebugObject->EventsPresent, 0, FALSE);

    // Loop over all processes and remove the debug port from any that still have it.
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
                // If the caller wanted process deletion on debugger dying (old interface) then kill off the process.
                if (DebugObject->Flags & DEBUG_OBJECT_KILL_ON_CLOSE)
                {
                    DbgPrint("杀死进程");
                    PsTerminateProcess(Process, STATUS_DEBUGGER_INACTIVE);
                }
                ObDereferenceObject(DebugObject);
            }
        }
    }

    // Wake up all the removed threads.
    while (ListPtr != &DebugObject->EventList)
    {
        DebugEvent = CONTAINING_RECORD(ListPtr, DEBUG_EVENT, EventList);
        ListPtr = ListPtr->Flink;
        DebugEvent->Status = STATUS_DEBUGGER_INACTIVE;
        DbgkpWakeTarget(DebugEvent);
    }

    DbgPrint("移除调试对象");
    DeleteDebugProcess(DebugObject);
}

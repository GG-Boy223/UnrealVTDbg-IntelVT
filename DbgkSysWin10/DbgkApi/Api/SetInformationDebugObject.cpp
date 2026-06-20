#include "SetInformationDebugObject.h"
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
NTSTATUS
NtSetInformationDebugObject(
    IN HANDLE DebugObjectHandle,
    IN DEBUGOBJECTINFOCLASS DebugObjectInformationClass,
    IN PVOID DebugInformation,
    IN ULONG DebugInformationLength,
    OUT PULONG ReturnLength OPTIONAL
)
{
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    PDEBUG_OBJECT DebugObject;
    ULONG Flags;

    PreviousMode = KeGetPreviousMode();
    DbgkRuntimeCallGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    __try
    {
        if (PreviousMode != KernelMode)
        {
            ProbeForRead(DebugInformation,
                DebugInformationLength,
                sizeof(ULONG));
            if (ARGUMENT_PRESENT(ReturnLength))
            {
                ProbeForWriteUlong(ReturnLength);
            }
        }
        if (ARGUMENT_PRESENT(ReturnLength))
        {
            *ReturnLength = 0;
        }

        switch (DebugObjectInformationClass)
        {
        case DebugObjectFlags:
        {

            if (DebugInformationLength != sizeof(ULONG))
            {
                if (ARGUMENT_PRESENT(ReturnLength))
                {
                    *ReturnLength = sizeof(ULONG);
                }
                return STATUS_INFO_LENGTH_MISMATCH;
            }
            Flags = *(PULONG)DebugInformation;

            break;
        }
        default:
        {
            return STATUS_INVALID_PARAMETER;
        }
        }
    }
    __except(ExSystemExceptionFilter())
    {
        return GetExceptionCode();
    }

    switch (DebugObjectInformationClass)
    {
    case DebugObjectFlags:
    {
        if (Flags & ~DBGK_KILL_PROCESS_ON_EXIT)
        {
            return STATUS_INVALID_PARAMETER;
        }
        Status = ObReferenceObjectByHandle(DebugObjectHandle,
            DEBUG_SET_INFORMATION,
            Hvm_DbgkDebugObjectType,
            PreviousMode,
            (PVOID*)&DebugObject,
            NULL);

        if (!NT_SUCCESS(Status))
        {
            return Status;
        }
        ExAcquireFastMutex(&DebugObject->Mutex);

        if (Flags & DBGK_KILL_PROCESS_ON_EXIT)
        {
            DebugObject->Flags |= DEBUG_OBJECT_KILL_ON_CLOSE;
        }
        else
        {
            DebugObject->Flags &= ~DEBUG_OBJECT_KILL_ON_CLOSE;
        }

        ExReleaseFastMutex(&DebugObject->Mutex);

        ObDereferenceObject(DebugObject);
    }
    }
    return STATUS_SUCCESS;
}

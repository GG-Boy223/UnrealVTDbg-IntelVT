#include "RemoveProcessDebug.h"
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
NtRemoveProcessDebug(
    IN HANDLE ProcessHandle,
    IN HANDLE DebugObjectHandle
)
{
    NTSTATUS Status;
    KPROCESSOR_MODE PreviousMode;
    PDEBUG_OBJECT DebugObject;
    _EPROCESS* Process;

    PAGED_CODE();
    DbgkRuntimeCallGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    PreviousMode = KeGetPreviousMode();

    Status = ObReferenceObjectByHandle(ProcessHandle,
        PROCESS_SET_PORT,
        *PsProcessType,
        PreviousMode,
        (PVOID*)&Process,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = ObReferenceObjectByHandle(DebugObjectHandle,
        DEBUG_PROCESS_ASSIGN,
        Hvm_DbgkDebugObjectType,
        PreviousMode,
        (PVOID*)&DebugObject,
        NULL);
    if (NT_SUCCESS(Status))
    {
        Status = DbgkClearProcessDebugObject(Process, DebugObject);
        DeleteDebugProcess(DebugObject);
        DbgPrint("DbgkClearProcessDebugObject Status: %x", Status);
        ObDereferenceObject(DebugObject);
    }

    ObDereferenceObject(Process);
    return Status;
}

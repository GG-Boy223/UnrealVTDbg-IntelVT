#include "DebugObjectCommonInternal.h"
#include "../../../Common/Ring0/Lifecycle/DbgkRundownPolicy.h"

namespace
{
    volatile LONG g_DbgkCloseProcedureOwned = 0;

    VOID WaitForDbgkStateOpsDrain()
    {
        LARGE_INTEGER interval = { 0 };
        interval.QuadPart = -10 * 1000 * 100;
        ULONG waitSamples = 0;

        while (InterlockedCompareExchange(&g_DbgkActiveRuntimeCallCount, 0, 0) != 0)
        {
            KeDelayExecutionThread(KernelMode, FALSE, &interval);
            waitSamples++;
            if ((waitSamples % 100) == 0)
            {
                outLog("Dbgk state teardown is waiting for active callbacks: %d.",
                    InterlockedCompareExchange(&g_DbgkActiveRuntimeCallCount, 0, 0));
            }
        }
    }

    NTSTATUS InstallDbgkCloseProcedure()
    {
        if (Hvm_DbgkDebugObjectType == NULL)
        {
            return STATUS_UNSUCCESSFUL;
        }

        __try
        {
            PVOID currentCloseProcedure = Hvm_DbgkDebugObjectType->TypeInfo.CloseProcedure;
            if (currentCloseProcedure != (PVOID)DbgkpCloseObject)
            {
                g_DbgkOriginalCloseProcedure = currentCloseProcedure;
            }

            if (currentCloseProcedure == (PVOID)DbgkpCloseObject &&
                InterlockedCompareExchange(&g_DbgkCloseProcedureOwned, 0, 0) == 0)
            {
                return STATUS_INVALID_DEVICE_STATE;
            }

            InterlockedExchange(&g_DbgkCloseProcedureOwned, 1);
            Hvm_DbgkDebugObjectType->TypeInfo.CloseProcedure = (PVOID)DbgkpCloseObject;
            KeMemoryBarrier();
            if (Hvm_DbgkDebugObjectType->TypeInfo.CloseProcedure != (PVOID)DbgkpCloseObject)
            {
                return STATUS_DATA_ERROR;
            }
            return STATUS_SUCCESS;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }
    }

    NTSTATUS RestoreDbgkCloseProcedure()
    {
        const bool callbackOwned =
            InterlockedCompareExchange(&g_DbgkCloseProcedureOwned, 0, 0) != 0;
        if (!callbackOwned)
        {
            return STATUS_SUCCESS;
        }

        PVOID currentCloseProcedure = NULL;
        bool callbackReadable = false;
        __try
        {
            currentCloseProcedure = Hvm_DbgkDebugObjectType != NULL ?
                Hvm_DbgkDebugObjectType->TypeInfo.CloseProcedure : NULL;
            callbackReadable = Hvm_DbgkDebugObjectType != NULL;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        const auto action = dbgk::rundown_policy::SelectRestoreAction(
            callbackOwned,
            Hvm_DbgkDebugObjectType != NULL,
            callbackReadable,
            reinterpret_cast<std::uintptr_t>(currentCloseProcedure),
            reinterpret_cast<std::uintptr_t>((PVOID)DbgkpCloseObject));
        if (action == dbgk::rundown_policy::restore_action::fail_stop)
        {
            return STATUS_INVALID_DEVICE_STATE;
        }

        if (action == dbgk::rundown_policy::restore_action::write_original)
        {
            __try
            {
                Hvm_DbgkDebugObjectType->TypeInfo.CloseProcedure =
                    g_DbgkOriginalCloseProcedure;
                KeMemoryBarrier();
                if (Hvm_DbgkDebugObjectType->TypeInfo.CloseProcedure !=
                    g_DbgkOriginalCloseProcedure)
                {
                    return STATUS_DATA_ERROR;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return STATUS_ACCESS_VIOLATION;
            }
        }

        InterlockedExchange(&g_DbgkCloseProcedureOwned, 0);
        g_DbgkOriginalCloseProcedure = NULL;
        return STATUS_SUCCESS;
    }

    VOID ReleaseListEntries(MY_LIST* List, PFAST_MUTEX Mutex)
    {
        if (List == NULL || Mutex == NULL)
        {
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return;
        }

        ExAcquireFastMutex(Mutex);
        while (!IsListEmpty(&List->ListHead))
        {
            PLIST_ENTRY entry = RemoveHeadList(&List->ListHead);
            if (List->Count != 0)
            {
                List->Count--;
            }
            free_pool(entry);
        }
        InitializeListHead(&List->ListHead);
        List->Count = 0;
        ExReleaseFastMutex(Mutex);
    }
}

NTSTATUS DbgkInitialize()
{
    NTSTATUS Status = STATUS_SUCCESS;
    UNICODE_STRING Name;
    OBJECT_TYPE_INITIALIZER ObjectTypeInitializer = { 0 };
    GENERIC_MAPPING GenericMapping = { STANDARD_RIGHTS_READ | DEBUG_READ_EVENT,
                                      STANDARD_RIGHTS_WRITE | DEBUG_PROCESS_ASSIGN,
                                      STANDARD_RIGHTS_EXECUTE | SYNCHRONIZE,
                                      DEBUG_ALL_ACCESS };

    PAGED_CODE();

    if (Hvm_DbgkDebugObjectType != NULL)
    {
        return InstallDbgkCloseProcedure();
    }

    if (ObCreateObjectType == NULL)
    {
        outLog("\x44\x62\x67\x6B\x49\x6E\x69\x74\x69\x61\x6C\x69\x7A\x65\x20\xCA\xA7\xB0\xDC\xA3\xBA\x4F\x62\x43\x72\x65\x61\x74\x65\x4F\x62\x6A\x65\x63\x74\x54\x79\x70\x65\x20\xCE\xB4\xBD\xE2\xCE\xF6\x2E");
        return STATUS_UNSUCCESSFUL;
    }

    ExInitializeFastMutex(&DbgkpProcessDebugPortMutex);

    RtlZeroMemory(&ObjectTypeInitializer, sizeof(ObjectTypeInitializer));
    RtlInitUnicodeString(&Name, L"Hvm_DebugObject");

    ObjectTypeInitializer.Length = sizeof(ObjectTypeInitializer);
    ObjectTypeInitializer.SecurityRequired = TRUE;
    ObjectTypeInitializer.InvalidAttributes = 0;
    ObjectTypeInitializer.PoolType = NonPagedPool;
    ObjectTypeInitializer.ValidAccessMask = DEBUG_ALL_ACCESS;
    ObjectTypeInitializer.GenericMapping = GenericMapping;
    ObjectTypeInitializer.DefaultPagedPoolCharge = 0;
    ObjectTypeInitializer.DefaultNonPagedPoolCharge = 0;
    ObjectTypeInitializer.CloseProcedure = DbgkpCloseObject;
    ObjectTypeInitializer.DeleteProcedure = NULL;

    Status = ObCreateObjectType(&Name, &ObjectTypeInitializer, NULL, &Hvm_DbgkDebugObjectType);
    if (NT_SUCCESS(Status))
    {
        g_DbgkOriginalCloseProcedure = NULL;
        InterlockedExchange(&g_DbgkCloseProcedureOwned, 1);
        return STATUS_SUCCESS;
    }

    if (Status != STATUS_OBJECT_NAME_COLLISION)
    {
        outLog("\x44\x62\x67\x6B\x49\x6E\x69\x74\x69\x61\x6C\x69\x7A\x65\x20\xCA\xA7\xB0\xDC\xA3\xBA\x4F\x62\x43\x72\x65\x61\x74\x65\x4F\x62\x6A\x65\x63\x74\x54\x79\x70\x65\x20\xB7\xB5\xBB\xD8\x20\x30\x78\x25\x58\x2E", Status);
        Hvm_DbgkDebugObjectType = NULL;
        // /* Return status */
        return Status;
    }

    Hvm_DbgkDebugObjectType = GetDebugObjectType(Name);
    if (Hvm_DbgkDebugObjectType == NULL)
    {
        outLog("\x44\x62\x67\x6B\x49\x6E\x69\x74\x69\x61\x6C\x69\x7A\x65\x20\xCA\xA7\xB0\xDC\xA3\xBA\x48\x76\x6D\x5F\x44\x65\x62\x75\x67\x4F\x62\x6A\x65\x63\x74\x20\xD2\xD1\xB4\xE6\xD4\xDA\xB5\xAB\xCE\xDE\xB7\xA8\xB0\xB2\xC8\xAB\xB8\xB4\xD3\xC3\x2E");
        return STATUS_OBJECT_NAME_COLLISION;
    }

    Status = InstallDbgkCloseProcedure();
    // /* Check if we failed */
    if (!NT_SUCCESS(Status))
    {
        // /* Return status */
        return Status;
    }

    outLog("\x44\x62\x67\x6B\x49\x6E\x69\x74\x69\x61\x6C\x69\x7A\x65\xA3\xBA\xB8\xB4\xD3\xC3\xD2\xD1\xB4\xE6\xD4\xDA\xB5\xC4\x20\x48\x76\x6D\x5F\x44\x65\x62\x75\x67\x4F\x62\x6A\x65\x63\x74\x2E");
    return STATUS_SUCCESS;
}

VOID DbgkUnInitialize()
{
    PAGED_CODE();

    const NTSTATUS restoreStatus = RestoreDbgkCloseProcedure();
    if (!NT_SUCCESS(restoreStatus))
    {
        outLog("Dbgk close callback restoration failed: 0x%X.", restoreStatus);
        KeBugCheckEx(
            0x00020002,
            0x4442474B,
            static_cast<ULONG_PTR>(restoreStatus),
            reinterpret_cast<ULONG_PTR>(Hvm_DbgkDebugObjectType),
            reinterpret_cast<ULONG_PTR>(g_DbgkOriginalCloseProcedure));
    }

    QueueDebugProcessListForUnload();
    DetachDebugProcessUnloadList();
    WaitForDbgkStateOpsDrain();

    if (Hvm_DbgkDebugObjectType != NULL)
    {
        Hvm_DbgkDebugObjectType = NULL;
    }

    ReleaseDebugProcessList();
    ReleaseListEntries(&g_DebuggerList.list_entry, &g_DebuggerList.Mutex);
    ReleaseListEntries(&g_BreakpointList.list_entry, &g_BreakpointList.Mutex);
    ReleaseListEntries(&g_VirtualHandleList.list_entry, &g_VirtualHandleList.Mutex);
    g_IsInitGlobalVariable = FALSE;
}

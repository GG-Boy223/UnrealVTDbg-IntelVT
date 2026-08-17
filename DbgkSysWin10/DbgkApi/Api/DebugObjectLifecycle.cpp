#include "DebugObjectCommonInternal.h"
#include "DebugObjectCommon.h"
#include "CreateDebugObject.h"
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
        outLog("DbgkInitialize 失败：ObCreateObjectType 未初始化.");
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
        outLog("DbgkInitialize 失败：ObCreateObjectType 错误 0x%X.", Status);
        Hvm_DbgkDebugObjectType = NULL;
        // /* Return status */
        return Status;
    }

    Hvm_DbgkDebugObjectType = GetDebugObjectType(Name);
    if (Hvm_DbgkDebugObjectType == NULL)
    {
        outLog("DbgkInitialize 失败：Hvm_DebugObject 已存在但无法安全获取.");
        return STATUS_OBJECT_NAME_COLLISION;
    }

    Status = InstallDbgkCloseProcedure();
    // /* Check if we failed */
    if (!NT_SUCCESS(Status))
    {
        // /* Return status */
        return Status;
    }

    outLog("DbgkInitialize：重用已存在的 Hvm_DebugObject.");
    return STATUS_SUCCESS;
}

NTSTATUS DbgkUnInitialize()
{
    PAGED_CODE();

    const NTSTATUS restoreStatus = RestoreDbgkCloseProcedure();
    if (!NT_SUCCESS(restoreStatus))
    {
        outLog("Dbgk close callback restoration failed: 0x%X.", restoreStatus);
        return restoreStatus;
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
    return STATUS_SUCCESS;
}

#include "DebugObjectCommonInternal.h"
#include "DebugObjectCommon.h"
#include "CreateDebugObject.h"

namespace
{
    const LONG kDbgkRuntimeDrainWaitLimit = 200;

    VOID WaitForDbgkStateOpsDrain()
    {
        LARGE_INTEGER interval = { 0 };
        interval.QuadPart = -10 * 1000 * 100;

        for (LONG i = 0; i < kDbgkRuntimeDrainWaitLimit; ++i)
        {
            if (InterlockedCompareExchange(&g_DbgkActiveRuntimeCallCount, 0, 0) == 0)
            {
                //是我们自己的调试器要创建线程，则直接返回不走线程回调
                return;
            }

            KeDelayExecutionThread(KernelMode, FALSE, &interval);
        }

        outLog("DbgkUnInitialize: wait for active runtime callbacks timeout.");
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

            Hvm_DbgkDebugObjectType->TypeInfo.CloseProcedure = (PVOID)DbgkpCloseObject;
            return STATUS_SUCCESS;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Hvm_DbgkDebugObjectType = NULL;
            return STATUS_UNSUCCESSFUL;
        }
    }

    VOID RestoreDbgkCloseProcedure()
    {
        if (Hvm_DbgkDebugObjectType == NULL)
        {
            //是我们自己的调试器要创建线程，则直接返回不走线程回调
            return;
        }

        __try
        {
            if (Hvm_DbgkDebugObjectType->TypeInfo.CloseProcedure == (PVOID)DbgkpCloseObject)
            {
                Hvm_DbgkDebugObjectType->TypeInfo.CloseProcedure = g_DbgkOriginalCloseProcedure;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }

        g_DbgkOriginalCloseProcedure = NULL;
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
        Hvm_DbgkDebugObjectType = NULL;
        // /* Return status */
        return Status;
    }

    outLog("DbgkInitialize：重用已存在的 Hvm_DebugObject.");
    return STATUS_SUCCESS;
}

VOID DbgkUnInitialize()
{
    PAGED_CODE();

    RestoreDbgkCloseProcedure();
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

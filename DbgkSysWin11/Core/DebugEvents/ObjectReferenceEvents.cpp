#include "DebugEventsCommon.h"
#include "../../Driver.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"
#include "../../List/MyList.h"

VOID InsertVirtualHandleList(PVIRTUAL_HANDLE_TABLE_ENTRY entry)
{
    if (entry)
    {
        ExAcquireFastMutex(&g_VirtualHandleList.Mutex);
        InsertNode(&g_VirtualHandleList.list_entry, &entry->list_entry);
        ExReleaseFastMutex(&g_VirtualHandleList.Mutex);
    }
}

VOID CreateVirtualHandleTable(PCLIENT_ID ClientId, _EPROCESS* Process)
{
    PVIRTUAL_HANDLE_TABLE_ENTRY entry = allocate_pool<VIRTUAL_HANDLE_TABLE_ENTRY>();
    entry->id = 0x8bf13889f4bc9949;  //VIRTUAL_HANDLE_TABLE md5哈希摘要
    entry->handle = ClientId->UniqueProcess;
    entry->UniqueProcessId = ClientId->UniqueProcess;
    entry->Object = Process;
    InsertVirtualHandleList(entry);
}

NTSTATUS GetVirtualHandleTableByHandle(HANDLE Handle, PVIRTUAL_HANDLE_TABLE_ENTRY handle_table)
{
    PLIST_ENTRY ListHead, NextEntry;
    PVIRTUAL_HANDLE_TABLE_ENTRY entry;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
    {
        ExAcquireFastMutex(&g_VirtualHandleList.Mutex);
    }

    ListHead = &g_VirtualHandleList.list_entry.ListHead;
    NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        entry = CONTAINING_RECORD(NextEntry,
            VIRTUAL_HANDLE_TABLE_ENTRY,
            list_entry);

        if (entry)
        {
            if (entry->handle == Handle)
            {
                *handle_table = *entry;
                // *handle_table = *entry;
                status = STATUS_SUCCESS;
                break;
            }
        }

        /* Move to the next entry */
        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
    }

    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
    {
        ExReleaseFastMutex(&g_VirtualHandleList.Mutex);
    }

    //ExReleaseFastMutex(&LongFlagsMutex);
    return status;
}

NTSTATUS GetVirtualHandleTableByObject(PVOID Object, PVIRTUAL_HANDLE_TABLE_ENTRY handle_table)
{
    PLIST_ENTRY ListHead, NextEntry;
    PVIRTUAL_HANDLE_TABLE_ENTRY entry;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    ExAcquireFastMutex(&g_VirtualHandleList.Mutex);
    ListHead = &g_VirtualHandleList.list_entry.ListHead;
    NextEntry = ListHead->Flink;
    while (ListHead != NextEntry)
    {
        entry = CONTAINING_RECORD(NextEntry,
            VIRTUAL_HANDLE_TABLE_ENTRY,
            list_entry);

        if (entry)
        {
            if (entry->Object == Object)
            {
                *handle_table = *entry;
                // *handle_table = *entry;
                status = STATUS_SUCCESS;
                break;
            }
        }

        /* Move to the next entry */
        // /* Move to the next entry */
        NextEntry = NextEntry->Flink;
    }
    ExReleaseFastMutex(&g_VirtualHandleList.Mutex);
    return status;
}

NTSTATUS NewObReferenceObjectByHandle(HANDLE Handle,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    PVOID* Object,
    POBJECT_HANDLE_INFORMATION HandleInformation)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_ObReferenceObjectByHandle(Handle, DesiredAccess, ObjectType, AccessMode, Object, HandleInformation);
    }

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
        AccessMode = KernelMode;
    }

    return Original_ObReferenceObjectByHandle(Handle, DesiredAccess, ObjectType, AccessMode, Object, HandleInformation);
}

NTSTATUS NewObReferenceObjectByHandleWithTag(HANDLE Handle,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    int a5,
    PVOID* Object,
    __int64 a7)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_ObReferenceObjectByHandleWithTag(Handle, DesiredAccess, ObjectType, AccessMode, a5, Object, a7);
    }

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
        AccessMode = KernelMode;
    }

    return Original_ObReferenceObjectByHandleWithTag(Handle, DesiredAccess, ObjectType, AccessMode, a5, Object, a7);
}

//ObReferenceObjectByHandle与ObReferenceObjectByHandleWithTag函数内部
//都调用的ObpReferenceObjectByHandleWithTag
NTSTATUS NewObpReferenceObjectByHandleWithTag(HANDLE Handle,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    ULONG Tag,
    PVOID* Object,
    POBJECT_HANDLE_INFORMATION HandleInformation,
    __int64 a8)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_ObpReferenceObjectByHandleWithTag(Handle, DesiredAccess, ObjectType, AccessMode, Tag, Object, HandleInformation, a8);
    }

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
        AccessMode = KernelMode;
    }

    NTSTATUS ntStatus = Original_ObpReferenceObjectByHandleWithTag(Handle, DesiredAccess, ObjectType, AccessMode, Tag, Object, HandleInformation, a8);
    if (NT_SUCCESS(ntStatus) && (ObjectType == *PsProcessType))
    {
        if (!IsDebugger(PsGetCurrentProcess()))
        {
            //判断访问的对象是否是我们的调试器
            if (IsDebugger(*(PEPROCESS*)Object))
            {
                //获取当前调用进程
                WCHAR SubStr[256] = { 0 };
                UNICODE_STRING ImageFileName, PassImage;
                NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
                if (NT_SUCCESS(Status))
                {
                    RtlInitUnicodeString(&ImageFileName, SubStr);
                    for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
                    {
                        RtlInitUnicodeString(&PassImage, PassProcessList[i]);
                        if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
                        {
                            //发现是白名单进程就放行
                            goto pass;
                        }
                    }

                    *(PEPROCESS*)Object = NULL;
                    // *(PEPROCESS*)Object = NULL;
                    ntStatus = STATUS_UNSUCCESSFUL;
                }
            }
        }
    }
pass:
    //        }
    //    }
    //}
    //__except (EXCEPTION_EXECUTE_HANDLER) {
    //    return ntStatus;
    return ntStatus;
}

LONG_PTR NewObfDereferenceObject(_In_ PVOID Object)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_ObfDereferenceObject(Object);
    }

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
        //POBJECT_HEADER ObjectHeader;
        //ObjectHeader = OBJECT_TO_OBJECT_HEADER(Object);
        //outLog("指针计数: %d", ObjectHeader->PointerCount);
        //outLog("句柄计数: %d", ObjectHeader->HandleCount);
        VIRTUAL_HANDLE_TABLE_ENTRY handle_table = { 0 };
        NTSTATUS status = GetVirtualHandleTableByObject(Object, &handle_table);
        if (NT_SUCCESS(status))
        {
            //DbgBreakPoint();
            return 0;
        }
    }
    return Original_ObfDereferenceObject(Object);
}

LONG_PTR NewObfDereferenceObjectWithTag(_In_ PVOID Object, _In_ ULONG Tag)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_ObfDereferenceObjectWithTag(Object, Tag);
    }

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
        //POBJECT_HEADER ObjectHeader;
        //ObjectHeader = OBJECT_TO_OBJECT_HEADER(Object);
        //outLog("指针计数: %d", ObjectHeader->PointerCount);
        //outLog("句柄计数: %d", ObjectHeader->HandleCount);
        VIRTUAL_HANDLE_TABLE_ENTRY handle_table = { 0 };
        NTSTATUS status = GetVirtualHandleTableByObject(Object, &handle_table);
        if (NT_SUCCESS(status))
        {
            //DbgBreakPoint();
            return 0;
        }
    }
    return Original_ObfDereferenceObjectWithTag(Object, Tag);
}

NTSTATUS NewNtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID ClientId)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
    }

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
    }
    return Original_NtOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
}

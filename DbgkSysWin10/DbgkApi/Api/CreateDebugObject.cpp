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
        ////判断当前调用者是否是调试器进程
        //if (!IsDebugger(PsGetCurrentProcess()))
        //{
        //    //根据窗口句柄获得窗口对象
        //    PWND pwnd = Sys_ValidateHwnd(hwnd);
        //    if (pwnd)
        //    {
        //        //根据线程对象获得进程对象
        //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
        //    }
        //}
        //NtUserQueryWindow(hwnd, WindowInfo);
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
            ////判断当前调用者是否是调试器进程
            //if (!IsDebugger(PsGetCurrentProcess()))
            //{
            //    //根据窗口句柄获得窗口对象
            //    PWND pwnd = Sys_ValidateHwnd(hwnd);
            //    if (pwnd)
            //    {
            //        //根据线程对象获得进程对象
            //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
            //    }
            //}
            //NtUserQueryWindow(hwnd, WindowInfo);
            return NULL;
        }

        if (objectType == NULL)
        {
            // *ppThread = NULL;
            continue;
        }

        if (SafeObjectTypeNameEquals(objectType, &Name))
        {
            return objectType;
        }
    }

    ////判断当前调用者是否是调试器进程
    //if (!IsDebugger(PsGetCurrentProcess()))
    //{
    //    //根据窗口句柄获得窗口对象
    //    PWND pwnd = Sys_ValidateHwnd(hwnd);
    //    if (pwnd)
    //    {
    //        //根据线程对象获得进程对象
    //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
    //    }
    //}
    //NtUserQueryWindow(hwnd, WindowInfo);
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
    NTSTATUS Status; // eax
    PAGED_CODE();
    DbgkRuntimeCallGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return STATUS_DELETE_PENDING;
    }

    /* Check if we were called from user mode*/
    // /* Check if we were called from user mode*/
    if (PreviousMode != KernelMode)
    {
        /* Enter SEH for probing */
        _SEH2_TRY
        {
            /* Probe the handle */
            // /* Probe the handle */
            ProbeForWriteHandle(DebugHandle);
        }
            _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            /* Return the exception code */
            // /* Return the exception code */
            _SEH2_YIELD(return _SEH2_GetExceptionCode());
        } _SEH2_END;
    }

    /* Check for invalid flags */
    // /* Check for invalid flags */
    if (Flags & ~DBGK_ALL_FLAGS) return STATUS_INVALID_PARAMETER;

    /* Create the Object */
    // /* Create the Object */
    //根据调试类型创建调试对象
    Status = ObCreateObject(PreviousMode,
        Hvm_DbgkDebugObjectType,
        ObjectAttributes,
        // *PsProcessType,
        PreviousMode,
        NULL,
        sizeof(DEBUG_OBJECT),
        0,
        0,
        (PVOID*)&DebugObject);
    if (NT_SUCCESS(Status))
    {
        /* Initialize the Debug Object's Fast Mutex */
        // /* Initialize the Debug Object's Fast Mutex */
        ExInitializeFastMutex(&DebugObject->Mutex);

        /* Initialize the State Event List */
        // /* Initialize the State Event List */
        InitializeListHead(&DebugObject->EventList);

        /* Initialize the Debug Object's Wait Event */
        // /* Initialize the Debug Object's Wait Event */
        KeInitializeEvent(&DebugObject->EventsPresent,
            NotificationEvent,
            FALSE);

        /* Set the Flags */
        // /* Set the Flags */
        if (Flags & DBGK_KILL_PROCESS_ON_EXIT) //退出时终止进程
        {
            DebugObject->Flags = DEBUG_OBJECT_KILL_ON_CLOSE;
        }
        else
        {
            DebugObject->Flags = 0;
        }

        /* Insert it */
        // /* Insert it */
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
                // /* Get the exception code */
                Status = _SEH2_GetExceptionCode();
            } _SEH2_END;
        }
    }

    /* Return Status */
    // /* Return Status */
    DBGKTRACE(DBGK_OBJECT_DEBUG, "Handle: %p DebugObject: %p\n",
        hDebug, DebugObject);
    // /* Return status */
    return Status;
}

EXTERN_C
VOID
DbgkpCloseObject(
    IN _EPROCESS* Process,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG_PTR SystemHandleCount
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
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
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    if (IsDbgkUnloadInProgress())
    {
        WakeDebugObjectEvents(DebugObject);
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    ExAcquireFastMutex(&DebugObject->Mutex);

    // Mark this object as going away and wake up any processes that are waiting.
    //
    DebugObject->Flags |= DEBUG_OBJECT_DELETE_PENDING;

    // Remove any events and queue them to a temporary queue
    //
    ListPtr = DebugObject->EventList.Flink;
    // /* Initialize the State Event List */
    InitializeListHead(&DebugObject->EventList);

    ExReleaseFastMutex(&DebugObject->Mutex);

    // Wake anyone waiting. They need to leave this object alone now as its deleting
    //
    KeSetEvent(&DebugObject->EventsPresent, 0, FALSE);

    // Loop over all processes and remove the debug port from any that still have it.
    //
    // Debug port propagation was disabled by setting the delete pending flag above so we only have to do this
    // once. No more refs can appear now.
    // 循环遍历所有进程，并删除仍然具有调试端口的进程。通过设置删除待定标志禁用了调试端口传播，因此我们只需执行一次此操作。
    // 现在不再会出现更多引用。
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
                // *ppThread = NULL;
                continue;
            }

            ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
            Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
        }

        if (Port == DebugObject)
        {
            Deref = FALSE;
            // /* Acquire the port lock */
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
                DbgkpMarkProcessPeb(Process); //清理进程的peb结构
                // If the caller wanted process deletion on debugger dying (old interface) then kill off the process.
                //
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
        // /* Wake it */
        DbgkpWakeTarget(DebugEvent);
    }

    DbgPrint("移除调试对象");
    DeleteDebugProcess(DebugObject);
}

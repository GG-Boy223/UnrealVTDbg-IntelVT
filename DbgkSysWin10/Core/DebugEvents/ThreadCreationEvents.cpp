#include "DebugEventsCommon.h"
#include "../../Driver.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"

EXTERN_C
NTSTATUS NtCreateThreadEx(unsigned __int64 a1,
    int a2,
    __int64 a3,
    ULONG_PTR a4,
    __int64 _StartAddress,
    __int64 a6,
    unsigned int a7,
    __int64 a8,
    __int64 a9,
    __int64 a10,
    __int64 a11)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_NtCreateThreadEx(a1, a2, a3, a4, _StartAddress, a6, a7, a8, a9, a10, a11);
    }

    NTSTATUS Status = STATUS_SUCCESS;

    ASSERT(Original_NtCreateThreadEx);
    Status = Original_NtCreateThreadEx(a1,
        a2,
        a3,
        a4,
        _StartAddress,
        a6,
        a7,
        a8,
        a9,
        a10,
        a11);
    // /* Return status */
    return Status;
}

EXTERN_C
NTSTATUS PspCreateThread(HANDLE* ThreadHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ProcessHandle,
    _EPROCESS* arg_Process,
    __int64 CreateProcessContext,
    __int64 a7,
    __int64 a8,
    __int64 a9,
    unsigned int a10,
    PKSTART_ROUTINE StartRoutine,
    __int64 a12,
    __int64 a13)
{
    DbgkRuntimeCallGuard runtimeGuard;
    NTSTATUS Status = Original_PspCreateThread(ThreadHandle,
        DesiredAccess,
        ObjectAttributes,
        ProcessHandle,
        arg_Process,
        CreateProcessContext,
        a7, a8, a9, a10, StartRoutine, a12, a13);
    // /* Return status */
    return Status;
}

NTSTATUS NewPspAllocateThread(_EPROCESS* Process,
    POBJECT_ATTRIBUTES ObjectAttributes,
    unsigned __int8 a3,
    __int64 a4,
    __int64 a5,
    __int64 a6,
    PKSTART_ROUTINE StartRoutine,
    __int64 a8,
    __int64 a9,
    __int64 a10,
    __int64 a11,
    __int64 a12)
{
    DbgkRuntimeCallGuard runtimeGuard;
    NTSTATUS Status = Original_PspAllocateThread(Process, ObjectAttributes, a3, a4, a5, a6, StartRoutine, a8, a9, a10, a11, a12);
    // /* Return status */
    return Status;
}

//如果ApcFunc调用的是LdrInitializeThunk则
//a4是ContextRecord
//a5是ntdll模块基址
VOID NewPspCreateUserContext(_CONTEXT* context,
    char a2,
    PVOID ApcFunc,
    PVOID a4,
    __int64 a5)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        return Original_PspCreateUserContext(context, a2, ApcFunc, a4, a5);
    }

    __try
    {
        if (*(ULONG64*)PspLoaderInitRoutine == (ULONG64)ApcFunc)
        {
            //outLog("ApcFunc: %p  a4: %p  a5: %p", ApcFunc, a4, a5);
            if (((PCONTEXT)a4)->Rcx == 0x7FF901D3FAE0)
            {
                outLog("rcx: %p", ((PCONTEXT)a4)->Rcx);
            }
        }
    }
    __except (1)
    {

    }
    return Original_PspCreateUserContext(context, a2, ApcFunc, a4, a5);
}

VOID NewPspCallThreadNotifyRoutines(_ETHREAD* Thread, BOOLEAN Create, BOOLEAN a3)
{
    DbgkRuntimeCallGuard runtimeGuard;
    if (DbgkIsDriverRuntimeUnloading())
    {
        if (Original_PspCallThreadNotifyRoutines != NULL)
        {
            Original_PspCallThreadNotifyRoutines(Thread, Create, a3);
        }
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    //DbgBreakPoint();
    if (IsDebugger(PsGetCurrentProcess()))
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }
    Original_PspCallThreadNotifyRoutines(Thread, Create, a3);
}

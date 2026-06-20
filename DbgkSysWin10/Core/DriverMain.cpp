#include "../Driver.h"
#include "../Log/log.h"
#include "../Hooks/EptHook/EptHook.h"
#include "../Globals.h"
#include "../DbgkApi/DbgkApi.h"

PWCHAR PassProcessList[12] = {
    _T("system"),
    _T("Registry"),
    _T("csrss.exe"),
    _T("svchost.exe"),
    _T("services.exe"),
    _T("lsass.exe"),
    _T("explorer.exe"),
    _T("dwm.exe"),
    _T("dllhost.exe"),
    _T("smss.exe"),
    _T("WmiPrvSE.exe"),
    _T("ctfmon.exe"),
};

namespace
{
    const LONG kDbgkUnloadWaitLimit = 200;

    LONG ReadDbgkActiveIoctlCount()
    {
        return InterlockedCompareExchange(&g_DbgkActiveIoctlCount, 0, 0);
    }

    void WaitForDbgkIoctlDrain()
    {
        LARGE_INTEGER interval = { 0 };
        interval.QuadPart = -10 * 1000 * 100;
        LONG zeroSamples = 0;

        for (LONG i = 0; i < kDbgkUnloadWaitLimit; i++)
        {
            if (ReadDbgkActiveIoctlCount() == 0)
            {
                zeroSamples++;
                if (zeroSamples >= 2)
                {
                    return;
                }
            }
            else
            {
                zeroSamples = 0;
            }

            KeDelayExecutionThread(KernelMode, FALSE, &interval);
        }

        outLog("\xD0\xB6\xD4\xD8\xB5\xC8\xB4\xFD\x20\x49\x4F\x43\x54\x4C\x20\xCD\xEA\xB3\xC9\xB3\xAC\xCA\xB1\xA3\xAC\xC7\xBF\xD6\xC6\xBC\xCC\xD0\xF8\x2E");
    }
}

EXTERN_C
VOID Unload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    InterlockedExchange(&g_DbgkDriverUnloading, 1);
    WaitForDbgkIoctlDrain();

    // 旧项目保留代码：卸载保护模块，内部原设计对应 UnCallbacks/进程保护回调清理。
    // 注释原因：当前 DriverEntry 没有放开 InitProtect/SetProcessCallbacks，卸载时调用会面对未注册句柄或重复清理风险。
    // 放开好处：若恢复 ObRegisterCallbacks 保护链路，可对称释放回调，避免驱动卸载残留。
    // 放开坏处：在初始化未接通时放开会增加卸载断言和无效句柄路径。
    // 项目影响：继续注释表示当前进程保护回调未启用；放开必须与 InitProtect/SetProcessCallbacks 同步。
    // 当前建议：继续注释。
    //UnloadProtect();
    RemoveHooks();
    DbgkUnInitialize();
    _RemoveDevice(DriverObject);
    DbgPrint("Driver Unload!!!\n");
}

EXTERN_C
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);
    LogFile::InitDriverLog();

    //ApcCreateRemoteThread((HANDLE)7572, NULL);
    DbgPrint("\xD1\xAA\xD0\xC8\xB3\xC7\xB1\xA4\xC7\xFD\xB6\xAF\xBC\xD3\xD4\xD8\xB3\xC9\xB9\xA6\x21\x21\x21\x0A");

    InitGlobalVariable(DriverObject);
    // 旧项目保留代码：初始化保护模块，原意是注册进程/线程/对象保护回调。
    // 注释原因：当前重构版本只初始化保护列表数据结构，没有完整接回 RegisterCallbacks/UnloadProtect 生命周期。
    // 放开好处：可以恢复句柄访问削权、反读写等保护能力。
    // 放开坏处：ObCallback altitude、签名绕过标志和卸载对称性都要重新验证，否则可能导致加载失败或卸载蓝屏。
    // 项目影响：继续注释时“保护进程函数”不真正生效；放开会改变全局进程句柄访问行为。
    // 当前建议：继续注释，等保护链路和回归测试补齐后再放开。
    //InitProtect(DriverObject);
    NTSTATUS ntStatus = CreateDevice(DriverObject);
    if (!NT_SUCCESS(ntStatus))
    {
        outLog("\x57\x69\x6E\x31\x30\xB4\xB4\xBD\xA8\xC9\xE8\xB1\xB8\xB6\xD4\xCF\xF3\xCA\xA7\xB0\xDC\x2E");
        return ntStatus;
    }
    else
    {
        outLog("\x57\x69\x6E\x31\x30\xB4\xB4\xBD\xA8\xC9\xE8\xB1\xB8\xB6\xD4\xCF\xF3\xB3\xC9\xB9\xA6\x2E");
    }
    DriverObject->MajorFunction[IRP_MJ_CREATE] = InitDispatchRoutin;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = InitDispatchRoutin;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HandlerDispatchRoutin;
    /* Initialize the User-Mode Debugging Subsystem */
    //TestVMM();
    DriverObject->DriverUnload = Unload;
    return STATUS_SUCCESS;
}

NTSTATUS CreateDevice(IN PDRIVER_OBJECT pDriver_Object)
{
    NTSTATUS ntStatus;
    PDEVICE_OBJECT pDevObj;
    PDEVICE_EXTENSION pDevExt;

    UNICODE_STRING devName;
    RtlInitUnicodeString(&devName, L"\\Device\\DbgkSysDevice");

    ntStatus = IoCreateDevice(pDriver_Object, sizeof(DEVICE_EXTENSION), &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDevObj);

    if (!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }

    pDevObj->Flags |= DO_BUFFERED_IO;
    pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
    pDevExt->pDevice = pDevObj;
    pDevExt->ustrDeviceName = devName;

    UNICODE_STRING symLinkName;
    RtlInitUnicodeString(&symLinkName, L"\\??\\UnrealDbg");
    pDevExt->ustrSymLinkName = symLinkName;
    ntStatus = IoCreateSymbolicLink(&symLinkName, &devName);
    if (!NT_SUCCESS(ntStatus))
    {
        IoDeleteDevice(pDevObj);
        return ntStatus;
    }
    return STATUS_SUCCESS;
}

VOID _RemoveDevice(IN PDRIVER_OBJECT pDriver_Object)
{
    PDEVICE_OBJECT pNextObj;
    pNextObj = pDriver_Object->DeviceObject;
    while (pNextObj != NULL)
    {
        PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pNextObj->DeviceExtension;

        UNICODE_STRING pLinkName = pDevExt->ustrSymLinkName;
        IoDeleteSymbolicLink(&pLinkName);
        pNextObj = pNextObj->NextDevice;
        IoDeleteDevice(pDevExt->pDevice);
    }
}

NTSTATUS InitDispatchRoutin(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    UNREFERENCED_PARAMETER(pDevObj);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
    ULONG cbin = stack->Parameters.DeviceIoControl.InputBufferLength;

    pIrp->IoStatus.Status = ntStatus;
    pIrp->IoStatus.Information = cbin;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return ntStatus;
}

EXTERN_C
VOID RemoveHooks()
{
    UnEptHook();
}

VOID InitGlobalVariable(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    g_IsInitGlobalVariable = FALSE;
    g_DbgkOriginalCloseProcedure = NULL;
    InterlockedExchange(&g_DbgkDriverUnloading, 0);
    InterlockedExchange(&g_DbgkActiveIoctlCount, 0);
    InterlockedExchange(&g_DbgkActiveRuntimeCallCount, 0);
    ExInitializeFastMutex(&LongFlagsMutex);
    InitializeList(&g_ProtectFileObjList.list_entry, &g_ProtectFileObjList.Mutex);
    InitializeList(&g_ProtectWndObjList.list_entry, &g_ProtectWndObjList.Mutex);
    InitializeList(&g_DebuggerList.list_entry, &g_DebuggerList.Mutex);
    InitializeList(&g_DebugProcessList.list_entry, &g_DebugProcessList.Mutex);
    InitializeList(&g_DebugProcessUnloadList.list_entry, &g_DebugProcessUnloadList.Mutex);
    InitializeList(&g_BreakpointList.list_entry, &g_BreakpointList.Mutex);
    InitializeList(&g_VirtualHandleList.list_entry, &g_VirtualHandleList.Mutex);
    // 旧项目保留代码：直接注册进程对象回调，削掉调试目标上的 PROCESS_VM_READ/WRITE 权限。
    // 注释原因：当前保护名单 IOCTL 没有真正装载数据，直接放开会让 IsDebugger 命中的进程进入全局句柄过滤。
    // 放开好处：可阻止非白名单进程打开调试目标读写句柄。
    // 放开坏处：可能误伤调试器自身、系统服务和卸载流程，且 Win10/Win11 行为需要分别验证。
    // 项目影响：继续注释不会改变系统句柄授权；放开会影响所有打开目标进程句柄的调用方。
    // 当前建议：继续注释。
    //SetProcessCallbacks(DriverObject);
}

VOID PrintProcessName(_EPROCESS* Process)
{
    size_t ptr_ImageFileName = (size_t)Process + eprocess_offset::ImageFileName;
    //outLog("打印进程名: %s", ptr_ImageFileName);
    DbgPrint("\xB4\xF2\xD3\xA1\xBD\xF8\xB3\xCC\xC3\xFB\x3A\x20\x25\x73\x20\x20\x70\x69\x64\x3A\x20\x25\x64\x0A", ptr_ImageFileName, PsGetProcessId((PEPROCESS)Process));
}

#include "../Driver.h"
#include "../Log/log.h"
#include "../Hooks/EptHook/EptHook.h"
#include "../Globals.h"
#include "../DbgkApi/DbgkApi.h"
#include "../Cr3/Cr3Resolver.h"
#include "../Protect/Callbacks.h"
#include <wdmsec.h>

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
    const GUID kDbgkDeviceClassGuid =
    { 0x8d72cb34, 0xe7b0, 0x4d53, { 0x99, 0x4f, 0x43, 0x5b, 0x20, 0xd7, 0xb9, 0x1a } };

    LONG ReadDbgkActiveIoctlCount()
    {
        return InterlockedCompareExchange(&g_DbgkActiveIoctlCount, 0, 0);
    }

    void WaitForDbgkIoctlDrain()
    {
        LARGE_INTEGER interval = { 0 };
        interval.QuadPart = -10 * 1000 * 100;
        ULONG waitSamples = 0;

        while (ReadDbgkActiveIoctlCount() != 0)
        {
            KeDelayExecutionThread(KernelMode, FALSE, &interval);
            waitSamples++;
            if ((waitSamples % 100) == 0)
            {
                outLog("Dbgk unload is waiting for active IOCTL requests: %d.",
                    ReadDbgkActiveIoctlCount());
            }
        }
    }
}

EXTERN_C
VOID Unload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    InterlockedExchange(&g_DbgkDriverUnloading, 1);
    _RemoveDevice(DriverObject);
    WaitForDbgkIoctlDrain();

    // 旧项目保留代码：卸载保护模块，内部原设计对应 UnCallbacks/进程保护回调清理。
    //放开必须与 InitProtect/SetProcessCallbacks 同步。
    UnloadProtect();
    RemoveHooks();
    const NTSTATUS dbgkStatus = DbgkUnInitialize();
    if (!NT_SUCCESS(dbgkStatus))
    {
        KeBugCheckEx(
            0x00020002,
            0x4442474B,
            static_cast<ULONG_PTR>(dbgkStatus),
            reinterpret_cast<ULONG_PTR>(Hvm_DbgkDebugObjectType),
            reinterpret_cast<ULONG_PTR>(g_DbgkOriginalCloseProcedure));
    }
    Cr3ResolverReset();
    DbgPrint("Driver Unload!!!\n");
}

EXTERN_C
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);
    //LogFile::InitDriverLog();  // 改用驱动自有日志 C:\DbgkSys10.log

    //ApcCreateRemoteThread((HANDLE)7572, NULL);
    //DbgPrint("血腥城堡驱动加载成功!!!\n");

    InitGlobalVariable(DriverObject);
    InitProtect(DriverObject);
    NTSTATUS ntStatus = CreateDevice(DriverObject);
    if (!NT_SUCCESS(ntStatus))
    {
        outLog("Win10创建设备对象失败.");
        return ntStatus;
    }
    else
    {
        outLog("Win10创建设备对象成功.");
    }
    DriverObject->MajorFunction[IRP_MJ_CREATE] = InitDispatchRoutin;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = InitDispatchRoutin;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HandlerDispatchRoutin;
    /* Initialize the User-Mode Debugging Subsystem */
    //TestVMM();
    // /* Initialize the User-Mode Debugging Subsystem */
    //DbgBreakPoint();
    DriverObject->DriverUnload = Unload;
    return STATUS_SUCCESS;
}

//创建设备 符号链接等
NTSTATUS CreateDevice(IN PDRIVER_OBJECT pDriver_Object)
{
    NTSTATUS ntStatus;
    PDEVICE_OBJECT pDevObj;
    PDEVICE_EXTENSION pDevExt;

    //创建设备名称
    UNICODE_STRING devName;
    RtlInitUnicodeString(&devName, L"\\Device\\DbgkSysDevice");

    ntStatus = IoCreateDeviceSecure(
        pDriver_Object,
        sizeof(DEVICE_EXTENSION),
        &devName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &SDDL_DEVOBJ_SYS_ALL_ADM_ALL,
        &kDbgkDeviceClassGuid,
        &pDevObj);

    if (!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }

    //创建直接读写设备
    pDevObj->Flags |= DO_BUFFERED_IO;
    pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
    pDevExt->pDevice = pDevObj;
    pDevExt->ustrDeviceName = devName;

    //创建符号链接
    UNICODE_STRING symLinkName;
    RtlInitUnicodeString(&symLinkName, L"\\??\\UnrealDbg");
    pDevExt->ustrSymLinkName = symLinkName;
    ntStatus = IoCreateSymbolicLink(&symLinkName, &devName);
    if (!NT_SUCCESS(ntStatus))
    {
        IoDeleteDevice(pDevObj); //创建失败删除设备
        return ntStatus;
    }
    return STATUS_SUCCESS;
}

//删除设备
VOID _RemoveDevice(IN PDRIVER_OBJECT pDriver_Object)
{
    PDEVICE_OBJECT pNextObj;
    pNextObj = pDriver_Object->DeviceObject;
    while (pNextObj != NULL)
    {
        PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pNextObj->DeviceExtension;

        //删除符号链接
        UNICODE_STRING pLinkName = pDevExt->ustrSymLinkName;
        IoDeleteSymbolicLink(&pLinkName);
        pNextObj = pNextObj->NextDevice;
        IoDeleteDevice(pDevExt->pDevice);
    }
}

NTSTATUS InitDispatchRoutin(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    UNREFERENCED_PARAMETER(pDevObj);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
    NTSTATUS status = STATUS_SUCCESS;
    if (stack->MajorFunction == IRP_MJ_CREATE &&
        DbgkIsDriverRuntimeUnloading())
    {
        status = STATUS_DELETE_PENDING;
    }

    pIrp->IoStatus.Status = status;
    pIrp->IoStatus.Information = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return status;
}

namespace
{
    VOID ProtectLogAppendText(char* buffer, size_t capacity, size_t& used, const char* text)
    {
        if (text == nullptr)
        {
            text = "(null)";
        }

        while (*text != '\0' && used + 1 < capacity)
        {
            buffer[used++] = *text++;
        }
    }

    VOID ProtectLogAppendNumber(char* buffer, size_t capacity, size_t& used, unsigned __int64 value, ULONG base, BOOLEAN upperCase)
    {
        const char* digits = upperCase ? "0123456789ABCDEF" : "0123456789abcdef";
        char temp[32];
        ULONG count = 0;

        if (value == 0)
        {
            temp[count++] = '0';
        }
        else
        {
            while (value != 0 && count < sizeof(temp))
            {
                temp[count++] = digits[value % base];
                value /= base;
            }
        }

        while (count != 0 && used + 1 < capacity)
        {
            buffer[used++] = temp[--count];
        }
    }

    VOID ProtectLogAppendPaddedNumber(char* buffer, size_t capacity, size_t& used, ULONG value, ULONG width)
    {
        char temp[16];
        ULONG count = 0;

        do
        {
            temp[count++] = (char)('0' + (value % 10));
            value /= 10;
        } while (value != 0 && count < sizeof(temp));

        while (count < width && used + 1 < capacity)
        {
            buffer[used++] = '0';
            --width;
        }

        while (count != 0 && used + 1 < capacity)
        {
            buffer[used++] = temp[--count];
        }
    }

    // 把一行日志追加到驱动自有日志文件（PASSIVE_LEVEL 调用；每次独立句柄，无需锁）。
    VOID ProtectLogWriteToFile(const char* message)
    {
        UNICODE_STRING filePath;
        OBJECT_ATTRIBUTES objectAttributes;
        IO_STATUS_BLOCK ioStatus;
        HANDLE fileHandle = NULL;
        char line[768] = { 0 };
        size_t used = 0;
        LARGE_INTEGER systemTime;
        TIME_FIELDS timeFields;

        KeQuerySystemTime(&systemTime);
        ExSystemTimeToLocalTime(&systemTime, &systemTime);
        RtlTimeToTimeFields(&systemTime, &timeFields);

        ProtectLogAppendText(line, sizeof(line), used, "[");
        ProtectLogAppendPaddedNumber(line, sizeof(line), used, timeFields.Year, 4);
        ProtectLogAppendText(line, sizeof(line), used, "-");
        ProtectLogAppendPaddedNumber(line, sizeof(line), used, timeFields.Month, 2);
        ProtectLogAppendText(line, sizeof(line), used, "-");
        ProtectLogAppendPaddedNumber(line, sizeof(line), used, timeFields.Day, 2);
        ProtectLogAppendText(line, sizeof(line), used, " ");
        ProtectLogAppendPaddedNumber(line, sizeof(line), used, timeFields.Hour, 2);
        ProtectLogAppendText(line, sizeof(line), used, ":");
        ProtectLogAppendPaddedNumber(line, sizeof(line), used, timeFields.Minute, 2);
        ProtectLogAppendText(line, sizeof(line), used, ":");
        ProtectLogAppendPaddedNumber(line, sizeof(line), used, timeFields.Second, 2);
        ProtectLogAppendText(line, sizeof(line), used, ".");
        ProtectLogAppendPaddedNumber(line, sizeof(line), used, timeFields.Milliseconds, 3);
        ProtectLogAppendText(line, sizeof(line), used, "] ");
        ProtectLogAppendText(line, sizeof(line), used, message);
        ProtectLogAppendText(line, sizeof(line), used, "\r\n");

        RtlInitUnicodeString(&filePath, L"\\??\\C:\\DbgkSys10.log");
        InitializeObjectAttributes(&objectAttributes, &filePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        NTSTATUS status = ZwCreateFile(&fileHandle,
            FILE_APPEND_DATA | SYNCHRONIZE,
            &objectAttributes,
            &ioStatus,
            NULL,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_OPEN_IF,
            FILE_SYNCHRONOUS_IO_NONALERT,
            NULL,
            0);
        if (!NT_SUCCESS(status))
        {
            DbgPrint("[DbgkProtect] open log failed 0x%X: %s\n", status, message);
            return;
        }

        ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatus, line, (ULONG)used, NULL, NULL);
        ZwClose(fileHandle);
    }
}

// 保护路径专用日志：写入 C:\Logs\driver.xml（非 PASSIVE_LEVEL 或写入失败时退回 DbgPrint）。
// 驱动工程里的 <ntstrsafe.h> 会解析到 SDK 用户态实现（__stdio_common_vsprintf 无法链接），
// 因此这里自带最小格式化（支持本文件用到的 %s/%u/%d/%x/%X/%p），不依赖 CRT/ntstrsafe。
VOID ProtectLog(IN const char* format, ...)
{
    char buffer[512] = { 0 };
    size_t used = 0;
    va_list args;

    va_start(args, format);

    for (const char* p = format; p != nullptr && *p != '\0' && used + 1 < sizeof(buffer); ++p)
    {
        if (*p != '%')
        {
            buffer[used++] = *p;
            continue;
        }

        ++p;
        if (*p == '\0')
        {
            break;
        }

        BOOLEAN is64 = FALSE;
        if (*p == 'l' && *(p + 1) == 'l')
        {
            is64 = TRUE;
            p += 2;
        }
        else if (*p == 'l')
        {
            ++p;
        }

        switch (*p)
        {
        case 's':
            ProtectLogAppendText(buffer, sizeof(buffer), used, va_arg(args, const char*));
            break;
        case 'u':
            ProtectLogAppendNumber(buffer, sizeof(buffer), used, is64 ? va_arg(args, unsigned __int64) : (unsigned __int64)va_arg(args, unsigned int), 10, FALSE);
            break;
        case 'd':
        {
            int value = va_arg(args, int);
            if (value < 0)
            {
                if (used + 1 < sizeof(buffer))
                {
                    buffer[used++] = '-';
                }
                ProtectLogAppendNumber(buffer, sizeof(buffer), used, (unsigned __int64)(-(__int64)value), 10, FALSE);
            }
            else
            {
                ProtectLogAppendNumber(buffer, sizeof(buffer), used, (unsigned __int64)value, 10, FALSE);
            }
            break;
        }
        case 'x':
            ProtectLogAppendNumber(buffer, sizeof(buffer), used, is64 ? va_arg(args, unsigned __int64) : (unsigned __int64)va_arg(args, unsigned int), 16, FALSE);
            break;
        case 'X':
            ProtectLogAppendNumber(buffer, sizeof(buffer), used, is64 ? va_arg(args, unsigned __int64) : (unsigned __int64)va_arg(args, unsigned int), 16, TRUE);
            break;
        case 'p':
            ProtectLogAppendText(buffer, sizeof(buffer), used, "0x");
            ProtectLogAppendNumber(buffer, sizeof(buffer), used, (unsigned __int64)(ULONG_PTR)va_arg(args, void*), 16, TRUE);
            break;
        case '%':
            if (used + 1 < sizeof(buffer))
            {
                buffer[used++] = '%';
            }
            break;
        default:
            if (used + 2 < sizeof(buffer))
            {
                buffer[used++] = '%';
                buffer[used++] = *p;
            }
            break;
        }
    }

    va_end(args);
    buffer[used] = '\0';

    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        DbgPrint("[DbgkProtect] %s\n", buffer);
        return;
    }

    ProtectLogWriteToFile(buffer);
}

//初始化保护：注册调试器进程句柄保护回调（削掉外部对调试器的 PROCESS_VM_READ/WRITE）
VOID InitProtect(IN PDRIVER_OBJECT DriverObject)
{
    SetProcessCallbacks(DriverObject);
    //削掉外部对调试器进程的 PROCESS_VM_READ/WRITE
    outFile("进程保护已启用");

}

//卸载保护
VOID UnloadProtect()
{
    UnProcessCallbacks();
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
    Cr3ResolverReset();
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
    DbgPrint("打印进程名: %s  pid: %d\n", ptr_ImageFileName, PsGetProcessId((PEPROCESS)Process));
}

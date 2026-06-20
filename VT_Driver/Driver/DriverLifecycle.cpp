#include "DriverInternal.h"

namespace
{
#ifdef _DEBUG_Key_Log
    void StopInitOffsetLogThread()
    {
        InterlockedExchange(&g_StopInitOffsetLogThread, 1);

        if (g_InitOffsetLogThreadHandle != nullptr)
        {
            ZwWaitForSingleObject(g_InitOffsetLogThreadHandle, FALSE, nullptr);
            ZwClose(g_InitOffsetLogThreadHandle);
            g_InitOffsetLogThreadHandle = nullptr;
        }
    }
#endif

    bool IsAnyVcpuLaunched()
    {
        if (g_vmm_context.vcpu == nullptr || g_vmm_context.processor_count == 0)
        {
            return false;
        }

        for (unsigned int i = 0; i < g_vmm_context.processor_count; ++i)
        {
            if (g_vmm_context.vcpu[i].vcpu_status.vmm_launched != 0)
            {
                return true;
            }
        }

        return false;
    }
}

EXTERN_C
VOID Unload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

#ifdef _DEBUG_Key_Log
    StopInitOffsetLogThread();
#endif

    if (IsAnyVcpuLaunched())
    {
        if (!hvgt::ept_unhook())
        {
            outDebug("EPT 反向解绑未完全成功，继续执行 VMXOFF 清理。\n");
        }

        hvgt::vmoff(g_vmm_context.processor_count);
    }

    hv::disable_vmx_operation();
    free_vmm_context();

    KdPrint(("Driver Unload!!!\n"));
}

EXTERN_C
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    //InitializeSerialPort();
    sLog("[VT] start \n");
    outFile("========== VT加载开始：DriverEntry ==========");

    DriverObject->DriverUnload = Unload;
    //KdPrint(("DriverEntry!!!\n"));
    outDebug(("进入 DriverEntry!!!\n"));

#ifdef _DEBUG_Key_Log
    NTSTATUS threadStatus = PsCreateSystemThread(
        &g_InitOffsetLogThreadHandle,
        THREAD_ALL_ACCESS,
        nullptr,
        nullptr,
        nullptr,
        InitOffsetLogThread,
        nullptr);

    if (!NT_SUCCESS(threadStatus))
    {
        g_InitOffsetLogThreadHandle = nullptr;
        outDebug("InitOffset 日志线程创建失败：0x%X\n", threadStatus);
    }
    else
    {
        outDebug("InitOffset 日志线程创建成功\n");
    }
#endif

    NTSTATUS nStatus = STATUS_SUCCESS;

    outFile("---------- 阶段1：初始化内核符号 ----------");
    if (InitNtoskrnlSymbolsTable())
    {
        outFile("[成功] 内核符号初始化完成");
        outFile("---------- 阶段2：检查硬件虚拟化支持 ----------");
        if (!hv::virtualization_support())
        {
            outFile("[失败] 当前处理器不支持或未开启VMX虚拟化");
            outDebug("VMX operation is not supported on this processor.\n");
            return STATUS_UNSUCCESSFUL;
        }
        outFile("[成功] VMX虚拟化支持检查通过");

        outFile("---------- 阶段3：保存Guest初始全局状态 ----------");
        hv::InitGlobalVariables();
        outFile("[成功] Guest初始寄存器：CR0=0x%llX CR3=0x%llX CR4=0x%llX", g_guest_cr0, g_guest_cr3, g_guest_cr4);
        outFile("---------- 阶段4：启动VMM并进入VT ----------");
        if (vmm_init() == false)
        {
            outFile("[失败] VMM初始化失败，开始清理VT上下文");
            hv::disable_vmx_operation();
            free_vmm_context();
            outDebug("VMM 初始化失败");
            outFile("========== VT加载失败：VMM初始化失败 ==========");
            return STATUS_UNSUCCESSFUL;
        }
        outFile("[成功] VMM初始化完成，所有目标CPU已进入VT链路");
        outDebug("驱动加载成功!!!\n");
        outFile("========== VT加载成功：DriverEntry完成 ==========");
    }
    else
    {
        nStatus = STATUS_UNSUCCESSFUL;
        outDebug("驱动加载失败!!!\n");
        outFile("========== VT加载失败：内核符号初始化失败 ==========");
    }
    return nStatus;
}

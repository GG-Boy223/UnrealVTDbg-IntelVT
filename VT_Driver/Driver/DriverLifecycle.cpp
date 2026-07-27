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

    bool IsAnyVcpuActive()
    {
        if (g_vmm_context.vcpu == nullptr || g_vmm_context.processor_count == 0)
        {
            return false;
        }

        for (unsigned int i = 0; i < g_vmm_context.processor_count; ++i)
        {
            if (g_vmm_context.vcpu[i].vcpu_status.vmm_launched != 0 ||
                g_vmm_context.vcpu[i].vcpu_status.vmx_on != 0)
            {
                return true;
            }
        }

        return false;
    }
}

EXTERN_C
VOID Unload(PDRIVER_OBJECT DriverObject) // 驱动卸载入口，释放虚拟化相关资源
{
    UNREFERENCED_PARAMETER(DriverObject); // 当前未使用驱动对象指针

#ifdef _DEBUG_Key_Log
    StopInitOffsetLogThread(); // 先停止驱动内线程，避免卸载后线程继续执行已卸载代码
#endif

    if (IsAnyVcpuActive())
    {
        if (!hvgt::ept_unhook())
        {
            outDebug("EPT 反向解绑未完全成功，继续执行 VMXOFF 清理。\n");
        }

        if (!hvgt::vmoff(g_vmm_context.processor_count))
        {
            vmx_fatal_error(
                vmx_fatal_vmxoff_broadcast,
                g_vmm_context.processor_count,
                0);
        }
    }

    hv::disable_vmx_operation(); // 关闭全局VMX使能，防止后续路径误用VMX指令
    free_vmm_context(); // 释放VMM上下文与相关内存

    KdPrint(("Driver Unload!!!\n")); // 输出卸载完成提示
}

//对于vt驱动尽量少使用Windows内核的api函数
//请参考vmexit_handler函数
EXTERN_C // 以C链接方式导出入口，避免C++名称修饰
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) // 驱动入口，系统加载驱动时调用
{
    UNREFERENCED_PARAMETER(RegistryPath); // 标记未直接使用的参数，避免编译告警

    //InitializeSerialPort();
    sLog("[VT] start \n"); // 输出一条空行日志，用于分隔启动日志
    outFile("========== VT加载开始：DriverEntry ==========");

    g_vt_driver_image_base = DriverObject->DriverStart; // 记录驱动镜像范围，后续补映射Host关键代码页，不作为VT进入条件。
    g_vt_driver_image_size = DriverObject->DriverSize;
    outFile("[信息][DriverEntry] VT驱动镜像：Base=0x%p Size=0x%llX",
        g_vt_driver_image_base,
        g_vt_driver_image_size);

    DriverObject->DriverUnload = Unload; // 注册卸载回调，驱动卸载时执行清理
    //KdPrint(("DriverEntry!!!\n"));
    //KdPrint(("DriverEntry!!!\n")); // 内核调试输出，提示入口被调用
    outDebug(("进入 DriverEntry!!!\n")); // 内核调试输出，提示入口被调用

#ifdef _DEBUG_Key_Log
    // 启动 InitOffset 延后打印线程。
    // 这个线程只负责在安全上下文里输出由 InitOffset() 记录下来的结果，
    // 本身不参与任何 VMX root 热路径逻辑。
    //
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

    NTSTATUS nStatus = STATUS_SUCCESS; // 默认返回成功状态

    outFile("---------- 阶段1：初始化内核符号 ----------");
    if (InitNtoskrnlSymbolsTable()) // 初始化内核符号表，为后续偏移/符号解析提供基础
    {
        outFile("[成功] 内核符号初始化完成");
        outFile("---------- 阶段2：检查硬件虚拟化支持 ----------");
        if (!hv::virtualization_support())
        {
            outFile("[失败] 当前处理器不支持或未开启VMX虚拟化");
            outDebug("VMX operation is not supported on this processor.\n");
            return STATUS_UNSUCCESSFUL; // 返回失败状态
        }
        outFile("[成功] VMX虚拟化支持检查通过");

        outFile("---------- 阶段3：保存Guest初始全局状态 ----------");
        hv::InitGlobalVariables(); // 初始化全局虚拟化相关状态与数据，保证后续流程依赖的全局值有效
        outFile("[成功] Guest初始寄存器：CR0=0x%llX CR3=0x%llX CR4=0x%llX", g_guest_cr0, g_guest_cr3, g_guest_cr4);
        outFile("---------- 阶段4：启动VMM并进入VT ----------");
        if (vmm_init() == false) // 初始化并启动VMM，失败则进入清理，避免留下半初始化状态
        {
            outFile("[失败] VMM初始化失败，开始清理VT上下文");
            hv::disable_vmx_operation();
            free_vmm_context(); // 释放VMM上下文和已分配资源
            outDebug("VMM 初始化失败"); // 输出初始化失败提示
            outFile("========== VT加载失败：VMM初始化失败 ==========");
            return STATUS_UNSUCCESSFUL; // 返回失败状态
        }
        outFile("[成功] VMM初始化完成，所有目标CPU已进入VT链路");
        outDebug("驱动加载成功!!!\n"); // 输出驱动加载成功提示
        outFile("========== VT加载成功：DriverEntry完成 ==========");
    }
    else
    {
        nStatus = STATUS_UNSUCCESSFUL; // 记录失败状态码
        outDebug("驱动加载失败!!!\n"); // 输出驱动加载失败提示
        outFile("========== VT加载失败：内核符号初始化失败 ==========");
    }
    return nStatus; // 返回最终状态给系统
}

#include "HypervisorInternal.h"

namespace hv
{
    /// <summary>
    /// 检查 CPU 是否支持虚拟化
    /// </summary>
    /// <returns></returns>
    bool virtualization_support() // 检测 CPU 是否支持 VT-x
    {
        __cpuid_info cpuid = { 0 }; // 初始化 CPUID 结果缓存
        __cpuid(&cpuid.cpu_info[0], 1); // 读取 CPUID 叶 1，获取 CPU 特性标志
        return cpuid.cpuid_eax_01.feature_information_ecx.virtual_machine_extensions; // 返回 VMX 位，标识是否支持 VT-x
    }

    /// <summary>
    /// 关闭 VMX 操作模式
    /// </summary>
    /// <returns></returns>
    void disable_vmx_operation() // 清除 CR4.VMXE，禁止 VMX 操作
    {
        __try // 保护性异常处理，避免内核崩溃
        {
            __cr4 cr4 = { 0 }; // 准备 CR4 结构体用于修改 VMX 使能位
            // __ia32_feature_control_msr feature_msr = { 0 };
            cr4.all = __readcr4();
            cr4.vmx_enable = 0; // 清除 VMX 使能位，禁止进入 VMX 操作
            __writecr4(cr4.all); // 写回 CR4，立即生效
        }
        __except (1) // 捕获并吞掉异常，保证流程继续
        {
            // VMXE disable is a rollback path; keep cleanup moving without changing VT entry conditions.
        }
    }

    bool vmx_on(unsigned __int64 vmxon_phys_addr) // 执行 VMXON 指令进入 VMX 根模式
    {
        // 0 则表示操作成功
        unsigned char res = __vmx_on(&vmxon_phys_addr); // 执行 VMXON 进入 VMX 根模式
        if (res) // VMX 指令返回非 0 表示失败
        {
            return false;
        }

        return true;
    }

    bool vmx_vmclear(unsigned __int64 vmcs_phys) // 执行 VMCLEAR 清空指定 VMCS
    {
        // 0 则表示操作成功
        unsigned char res = __vmx_vmclear(&vmcs_phys); // 清理指定 VMCS，重置其状态
        if (res) // VMX 指令返回非 0 表示失败
        {
            return false;
        }

        return true;
    }

    bool vmx_vmptrld(unsigned __int64 vmcs_phys) // 执行 VMPTRLD 加载指定 VMCS
    {
        // 0 则表示操作成功
        unsigned char res = __vmx_vmptrld(&vmcs_phys); // 加载 VMCS 指针，使其成为当前活动 VMCS
        if (res) // VMX 指令返回非 0 表示失败
        {
            return false;
        }

        return true;
    }
}

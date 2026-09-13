#include "GlobalsInternal.h"

namespace hv
{
    bool load_vmcs_pointer(vmcs& vmcs_region)
    {
        // ✅ P0-001 修复：清零整个 VMCS 区域，避免随机数据导致 VM-Fail
        RtlSecureZeroMemory(&vmcs_region, sizeof(vmcs));

        ia32_vmx_basic_register vmx_basic; // VMX 基本能力结构
        vmx_basic.flags = __readmsr(IA32_VMX_BASIC); // 读取 VMX 基本能力 MSR

        // 3.24.2
        vmcs_region.revision_id = vmx_basic.vmcs_revision_id; // 写入 VMCS 版本号，硬件要求
        vmcs_region.shadow_vmcs_indicator = 0; // 禁用 shadow VMCS，使用普通 VMCS

        auto vmcs_phys = MmGetPhysicalAddress(&vmcs_region).QuadPart; // 获取 VMCS 物理地址
        NT_ASSERT(vmcs_phys % 0x1000 == 0); // VMCS 必须 4KB 对齐，否则 VMX 指令失败
        outFile("[信息][VMCS] 准备加载VMCS指针：VA=0x%p PA=0x%llX Revision=0x%X",
            &vmcs_region,
            vmcs_phys,
            vmx_basic.vmcs_revision_id);

        if (!hv::vmx_vmclear(vmcs_phys))
        {
            outFile("[失败][VMCS] VMCLEAR执行失败：VMCS_PA=0x%llX", vmcs_phys);
            return false;
        }
        outFile("[成功][VMCS] VMCLEAR执行完成：VMCS_PA=0x%llX", vmcs_phys);

        if (!hv::vmx_vmptrld(vmcs_phys))
        {
            outFile("[失败][VMCS] VMPTRLD执行失败：VMCS_PA=0x%llX", vmcs_phys);
            return false;
        }
        outFile("[成功][VMCS] VMPTRLD执行完成：VMCS_PA=0x%llX", vmcs_phys);

        return true;
    }

    //读取guest通用寄存器
    uint64_t read_guest_gpr(guest_context const* const ctx, uint64_t const gpr_idx)
    {
        if (gpr_idx == VMX_EXIT_QUALIFICATION_GENREG_RSP)
        {
            return hv::vmread(GUEST_RSP); // 直接从 VMCS 读取，保证与硬件一致
        }

        return ctx->gpr[gpr_idx]; // 其他寄存器从保存上下文读取
    }

    //写guest通用寄存器
    void write_guest_gpr(guest_context* const ctx, uint64_t const gpr_idx, uint64_t const value)
    {
        if (gpr_idx == VMX_EXIT_QUALIFICATION_GENREG_RSP)
        {
            vmwrite(GUEST_RSP, value); // 直接写入 VMCS，保证 guest RSP 正确
        }
        else
        {
            ctx->gpr[gpr_idx] = value; // 写回上下文供后续恢复
        }
    }

    vmx_interruptibility_state read_interruptibility_state()
    {
        vmx_interruptibility_state value; // VMX 阻塞状态结构
        value.flags = static_cast<uint32_t>(vmread(GUEST_INTERRUPTIBILITY_STATE)); // 读取 VMCS 中断阻塞字段
        return value; // 返回给调用方进行模拟/修正
    }

    void write_interruptibility_state(vmx_interruptibility_state const value)
    {
        hv::vmwrite(GUEST_INTERRUPTIBILITY_STATE, value.flags); // 写回阻塞字段，控制来宾中断阻塞行为
    }

    bool enter_vmx_operation(vmxon& vmxon_region)
    {
        if (!required_vmx_capabilities_supported())
        {
            outFile("[失败][VMXON] 当前逻辑处理器缺少所需EPT/VPID能力");
            return false;
        }

        // ✅ P0-001 修复：清零整个 VMXON 区域，避免随机数据导致 VMXON 失败
        RtlSecureZeroMemory(&vmxon_region, sizeof(vmxon));

        ia32_vmx_basic_register vmx_basic; // VMX 基本能力结构
        vmx_basic.flags = __readmsr(IA32_VMX_BASIC); // 读取 VMX 基本能力

        // 3.24.11.5
        vmxon_region.revision_id = vmx_basic.vmcs_revision_id; // 设置 VMXON 区域版本号
        vmxon_region.must_be_zero = 0; // 保留位清零，满足硬件要求

        auto vmxon_phys = MmGetPhysicalAddress(&vmxon_region).QuadPart; // 获取 VMXON 物理地址
        NT_ASSERT(vmxon_phys % 0x1000 == 0); // VMXON 区域必须 4KB 对齐
        outFile("[信息][VMXON] 准备执行VMXON：VA=0x%p PA=0x%llX Revision=0x%X IA32_VMX_BASIC=0x%llX",
            &vmxon_region,
            vmxon_phys,
            vmx_basic.vmcs_revision_id,
            vmx_basic.flags);

        if (!vmx_on(vmxon_phys))
        {
            outFile("[失败][VMXON] VMXON执行失败：VMXON_PA=0x%llX", vmxon_phys);
            return false;
        }

        outFile("[成功][VMXON] VMXON执行完成：VMXON_PA=0x%llX", vmxon_phys);

        return true;
    }

    void InitGlobalVariables()
    {
        g_guest_cr3 = __readcr3(); // 保存当前CR3（来宾页表根），便于后续恢复/调试
        __sgdt(&g_gdtr); // 保存当前CPU的GDT描述符，供退出VMX或异常恢复使用
        __sidt(&g_idtr); // 保存当前CPU的IDT描述符，供退出VMX或异常恢复使用
        g_guest_cr0 = __readcr0(); // 保存当前CR0，记录来宾控制寄存器状态
        g_guest_cr4 = __readcr4(); // 保存当前CR4，记录来宾控制寄存器状态
    }
}

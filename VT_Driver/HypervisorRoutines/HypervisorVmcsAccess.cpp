#include "HypervisorInternal.h"

namespace hv
{
    /// <summary>
    /// 读取 VMCS 字段
    /// </summary>
    /// <param name="vmcs_field">VMCS 字段编码</param>
    /// <returns></returns>
    unsigned __int64 vmread(unsigned __int64 vmcs_field) // 封装 VMREAD，读取指定 VMCS 字段
    {
        unsigned __int64 value; // 用于保存 VMREAD 读取的结果
        __vmx_vmread(vmcs_field, &value); // 执行 VMREAD，从当前 VMCS 读取指定字段
        return value; // 返回读取结果
    }

    /// <summary>
    /// 获取来宾当前特权级（CPL）
    /// </summary>
    /// <returns> 返回当前来宾特权级 </returns>
    unsigned __int8 get_guest_cpl() // 读取来宾当前 CPL（特权级）
    {
        uint32_t cs_access_rights = vmread(GUEST_CS_ACCESS_RIGHTS);
        return (cs_access_rights >> 5) & 3;

        //return vmread(GUEST_CS_SELECTOR) & 3; // 读取 GUEST_CS_SELECTOR 的低 2 位作为 CPL（特权级 0-3） 读取的是 RPL，不是 CPL
    }

    // 获取来宾的当前 CPL（特权级）
    uint16_t current_guest_cpl() // 通过 SS 段访问权限获取来宾 CPL
    {
        vmx_segment_access_rights ss; // 定义段访问权限结构体
        ss.flags = static_cast<uint32_t>(vmread(GUEST_SS_ACCESS_RIGHTS)); // 读取 SS 的访问权限字段并存入结构体
        return ss.descriptor_privilege_level; // 返回 DPL，即当前栈段的特权级
    }

    /// <summary>
    /// 计算指令给出的来宾内存地址（例如 INVPCID 使用的地址）
    /// </summary>
    /// <param name="vcpu">当前 vCPU 结构</param>
    /// <returns></returns>
    unsigned __int64 get_guest_address(__vcpu* vcpu) // 根据 VM-exit 指令信息计算来宾线性地址
    {
        __vmexit_instruction_information2 instruction_information; // 保存 VM-exit 指令信息（解码后的寻址方式）

        instruction_information.all = vcpu->vmexit_info.instruction_information; // 从 vcpu 中获取 VM-exit 的指令信息

        unsigned __int64 displacement = vcpu->vmexit_info.qualification; // 取出指令位移（例如内存操作的偏移）
        unsigned __int64 base_value = !instruction_information.base_reg_invalid ? *(&vcpu->vmexit_info.guest_registers->rax - instruction_information.base_reg) : 0; // 如果有基址寄存器则取其值
        unsigned __int64 index_value = !instruction_information.index_reg_invalid ? *(&vcpu->vmexit_info.guest_registers->rax - instruction_information.index_reg) : 0; // 如果有变址寄存器则取其值

        index_value = index_value * (1ULL << instruction_information.scaling); // 根据缩放因子计算最终变址值

        unsigned __int64 segment_base = hv::vmread(GUEST_ES_BASE + (instruction_information.segment_register << 1)); // 读取段寄存器基址（用于分段寻址）
        unsigned __int64 guest_address = displacement + base_value + index_value + segment_base; // 组合位移 + 基址 + 变址 + 段基址，得到线性地址

        return guest_address; // 返回计算得到的来宾地址
    }

    void set_mtf(bool set) // 开关 MTF，用于单步监控
    {
        __vmx_primary_processor_based_control primary_controls{ hv::vmread(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL) }; // 读取主处理器执行控制字段
        primary_controls.monitor_trap_flag = set; // 设置或清除监控陷阱标志(MTF)，用于触发单步 VM-exit
        hv::vmwrite(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL, primary_controls.all); // 将修改后的控制字段写回 VMCS
    }
}

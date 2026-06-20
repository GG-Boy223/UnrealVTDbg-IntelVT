#include "HypervisorInternal.h"

namespace hv
{
    unsigned __int64 vmread(unsigned __int64 vmcs_field)
    {
        unsigned __int64 value;
        __vmx_vmread(vmcs_field, &value);
        return value;
    }

    unsigned __int8 get_guest_cpl()
    {
        return vmread(GUEST_CS_SELECTOR) & 3;
    }

    uint16_t current_guest_cpl()
    {
        vmx_segment_access_rights ss;
        ss.flags = static_cast<uint32_t>(vmread(GUEST_SS_ACCESS_RIGHTS));
        return ss.descriptor_privilege_level;
    }

    unsigned __int64 get_guest_address(__vcpu* vcpu)
    {
        __vmexit_instruction_information2 instruction_information;

        instruction_information.all = vcpu->vmexit_info.instruction_information;

        unsigned __int64 displacement = vcpu->vmexit_info.qualification;
        unsigned __int64 base_value = !instruction_information.base_reg_invalid ? *(&vcpu->vmexit_info.guest_registers->rax - instruction_information.base_reg) : 0;
        unsigned __int64 index_value = !instruction_information.index_reg_invalid ? *(&vcpu->vmexit_info.guest_registers->rax - instruction_information.index_reg) : 0;

        index_value = index_value * (1ULL << instruction_information.scaling);

        unsigned __int64 segment_base = hv::vmread(GUEST_ES_BASE + (instruction_information.segment_register << 1));
        unsigned __int64 guest_address = displacement + base_value + index_value + segment_base;

        return guest_address;
    }

    void set_mtf(bool set)
    {
        __vmx_primary_processor_based_control primary_controls{ hv::vmread(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL) };
        primary_controls.monitor_trap_flag = set;
        hv::vmwrite(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL, primary_controls.all);
    }
}

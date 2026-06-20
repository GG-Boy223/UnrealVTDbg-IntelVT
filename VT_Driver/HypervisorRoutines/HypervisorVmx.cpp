#include "HypervisorInternal.h"

namespace hv
{
    bool virtualization_support()
    {
        __cpuid_info cpuid = { 0 };
        __cpuid(&cpuid.cpu_info[0], 1);
        return cpuid.cpuid_eax_01.feature_information_ecx.virtual_machine_extensions;
    }

    void disable_vmx_operation()
    {
        __try
        {
            __cr4 cr4 = { 0 };
            __ia32_feature_control_msr feature_msr = { 0 };
            cr4.all = __readcr4();
            cr4.vmx_enable = 0;
            __writecr4(cr4.all);
        }
        __except (1)
        {
        }
    }

    bool vmx_on(unsigned __int64 vmxon_phys_addr)
    {
        unsigned char res = __vmx_on(&vmxon_phys_addr);
        if (res)
        {
            return false;
        }

        return true;
    }

    bool vmx_vmclear(unsigned __int64 vmcs_phys)
    {
        unsigned char res = __vmx_vmclear(&vmcs_phys);
        if (res)
        {
            return false;
        }

        return true;
    }

    bool vmx_vmptrld(unsigned __int64 vmcs_phys)
    {
        unsigned char res = __vmx_vmptrld(&vmcs_phys);
        if (res)
        {
            return false;
        }

        return true;
    }
}

#include "GlobalsInternal.h"

namespace hv
{
    bool load_vmcs_pointer(vmcs& vmcs_region)
    {
        ia32_vmx_basic_register vmx_basic;
        vmx_basic.flags = __readmsr(IA32_VMX_BASIC);

        vmcs_region.revision_id = vmx_basic.vmcs_revision_id;
        vmcs_region.shadow_vmcs_indicator = 0;

        auto vmcs_phys = MmGetPhysicalAddress(&vmcs_region).QuadPart;
        NT_ASSERT(vmcs_phys % 0x1000 == 0);

        if (!hv::vmx_vmclear(vmcs_phys))
        {
            return false;
        }

        if (!hv::vmx_vmptrld(vmcs_phys))
        {
            return false;
        }

        return true;
    }

    uint64_t read_guest_gpr(guest_context const* const ctx, uint64_t const gpr_idx)
    {
        if (gpr_idx == VMX_EXIT_QUALIFICATION_GENREG_RSP)
        {
            return hv::vmread(GUEST_RSP);
        }

        return ctx->gpr[gpr_idx];
    }

    void write_guest_gpr(guest_context* const ctx, uint64_t const gpr_idx, uint64_t const value)
    {
        if (gpr_idx == VMX_EXIT_QUALIFICATION_GENREG_RSP)
        {
            vmwrite(GUEST_RSP, value);
        }
        else
        {
            ctx->gpr[gpr_idx] = value;
        }
    }

    vmx_interruptibility_state read_interruptibility_state()
    {
        vmx_interruptibility_state value;
        value.flags = static_cast<uint32_t>(vmread(GUEST_INTERRUPTIBILITY_STATE));
        return value;
    }

    void write_interruptibility_state(vmx_interruptibility_state const value)
    {
        hv::vmwrite(GUEST_INTERRUPTIBILITY_STATE, value.flags);
    }

    bool enter_vmx_operation(vmxon& vmxon_region)
    {
        ia32_vmx_basic_register vmx_basic;
        vmx_basic.flags = __readmsr(IA32_VMX_BASIC);

        vmxon_region.revision_id = vmx_basic.vmcs_revision_id;
        vmxon_region.must_be_zero = 0;

        auto vmxon_phys = MmGetPhysicalAddress(&vmxon_region).QuadPart;
        NT_ASSERT(vmxon_phys % 0x1000 == 0);

        if (!vmx_on(vmxon_phys))
        {
            return false;
        }

        _mm_mfence();
        invept_all_contexts_func();

        return true;
    }

    void InitGlobalVariables()
    {
        g_guest_cr3 = __readcr3();
        __sgdt(&g_gdtr);
        __sidt(&g_idtr);
        g_guest_cr0 = __readcr0();
        g_guest_cr4 = __readcr4();
    }
}

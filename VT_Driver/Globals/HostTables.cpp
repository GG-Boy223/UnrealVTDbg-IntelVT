#include "GlobalsInternal.h"

namespace hv
{
    ia32_vmx_procbased_ctls_register read_ctrl_proc_based()
    {
        ia32_vmx_procbased_ctls_register value;
        value.flags = vmread(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL);
        return value;
    }

    void write_ctrl_proc_based(ia32_vmx_procbased_ctls_register const value)
    {
        vmwrite(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL, value.flags);
    }

    void prepare_external_structures(__vcpu* const vcpu)
    {
        memset(&vcpu->msr_bitmap, 0, sizeof(vcpu->msr_bitmap));
        memset(&vcpu->host_tss, 0, sizeof(vcpu->host_tss));

        prepare_host_idt(vcpu->host_idt);
        prepare_host_gdt(vcpu->host_gdt, &vcpu->host_tss);
    }

    void inject_nmi()
    {
        inject_interruption(EXCEPTION_VECTOR_NMII, INTERRUPT_TYPE_NMI, 0, false);
    }

    cr0 read_effective_guest_cr0()
    {
        auto const mask = vmread(CR0_GUEST_HOST_MASK);

        cr0 cr0;
        cr0.flags = (vmread(CR0_READ_SHADOW) & mask) | (vmread(GUEST_CR0) & ~mask);

        return cr0;
    }

    cr4 read_effective_guest_cr4()
    {
        auto const mask = vmread(CR4_GUEST_HOST_MASK);

        cr4 cr4;
        cr4.flags = (vmread(CR4_READ_SHADOW) & mask) | (vmread(GUEST_CR4) & ~mask);

        return cr4;
    }

    void map_physical_memory(host_page_tables& pt)
    {
        auto& pml4e = pt.pml4[host_physical_memory_pml4_idx];
        pml4e.flags = 0;
        pml4e.present = 1;
        pml4e.write = 1;
        pml4e.supervisor = 0;
        pml4e.page_level_write_through = 0;
        pml4e.page_level_cache_disable = 0;
        pml4e.accessed = 0;
        pml4e.execute_disable = 0;
        pml4e.page_frame_number = MmGetPhysicalAddress(&pt.phys_pdpt).QuadPart >> 12;

        for (uint64_t i = 0; i < HOST_PHYSICAL_MEMORY_PD_COUNT; ++i)
        {
            auto& pdpte = pt.phys_pdpt[i];
            pdpte.flags = 0;
            pdpte.present = 1;
            pdpte.write = 1;
            pdpte.supervisor = 0;
            pdpte.page_level_write_through = 0;
            pdpte.page_level_cache_disable = 0;
            pdpte.accessed = 0;
            pdpte.execute_disable = 0;
            pdpte.page_frame_number = MmGetPhysicalAddress(&pt.phys_pds[i]).QuadPart >> 12;

            for (uint64_t j = 0; j < 512; ++j)
            {
                auto& pde = pt.phys_pds[i][j];
                pde.flags = 0;
                pde.present = 1;
                pde.write = 1;
                pde.supervisor = 0;
                pde.page_level_write_through = 0;
                pde.page_level_cache_disable = 0;
                pde.accessed = 0;
                pde.dirty = 0;
                pde.large_page = 1;
                pde.global = 0;
                pde.pat = 0;
                pde.execute_disable = 0;
                pde.page_frame_number = (i << 9) + j;
            }
        }
    }

    void map_physical_memory2(host_page_tables& pt)
    {
        auto& pml4e = pt.pml4[host_physical_memory_pml4_idx];
        pml4e.flags = 0;
        pml4e.present = 1;
        pml4e.write = 1;
        pml4e.supervisor = 0;
        pml4e.page_level_write_through = 0;
        pml4e.page_level_cache_disable = 0;
        pml4e.accessed = 0;
        pml4e.execute_disable = 0;
        pml4e.page_frame_number = MmGetPhysicalAddress(&pt.phys_pdpt).QuadPart >> 12;

        pdpte_64 pdpte = { 0 };
        pdpte.present = 1;
        pdpte.write = 1;
        pdpte.supervisor = 0;
        pdpte.page_level_write_through = 0;
        pdpte.page_level_cache_disable = 0;
        pdpte.accessed = 0;
        pdpte.execute_disable = 0;
        __stosq((unsigned __int64*)&pt.phys_pdpt[0], pdpte.flags, HOST_PHYSICAL_MEMORY_PD_COUNT);

        for (uint64_t i = 0; i < HOST_PHYSICAL_MEMORY_PD_COUNT; ++i)
        {
            pt.phys_pdpt[i].page_frame_number = GET_PFN(MmGetPhysicalAddress(&pt.phys_pds[i]).QuadPart);
        }

        pde_2mb_64 pde = { 0 };
        pde.present = 1;
        pde.write = 1;
        pde.supervisor = 0;
        pde.page_level_write_through = 0;
        pde.page_level_cache_disable = 0;
        pde.accessed = 0;
        pde.dirty = 0;
        pde.large_page = 1;
        pde.global = 0;
        pde.pat = 0;
        pde.execute_disable = 0;
        __stosq((unsigned __int64*)&pt.phys_pds[0], pde.flags, 512 * 512);

        for (uint64_t i = 0; i < HOST_PHYSICAL_MEMORY_PD_COUNT; ++i)
        {
            for (uint64_t j = 0; j < 512; ++j)
            {
                pt.phys_pds[i][j].page_frame_number = (i << 9) + j;
            }
        }
    }

    bool prepare_host_page_tables()
    {
        if (ghv.system_cr3.flags == 0 ||
            ghv.system_cr3.address_of_page_directory == 0)
        {
            outDebug("宿主页表的 system_cr3 无效。\n");
            return false;
        }

        auto& pt = ghv.host_page_tables;
        memset(&pt, 0, sizeof(pt));

        map_physical_memory(pt);

        PHYSICAL_ADDRESS pml4_address;
        pml4_address.QuadPart = ghv.system_cr3.address_of_page_directory << 12;

        auto const guest_pml4 = static_cast<pml4e_64*>(MmGetVirtualForPhysical(pml4_address));
        if (guest_pml4 == nullptr)
        {
            outDebug("MmGetVirtualForPhysical 返回空的 guest PML4。\n");
            return false;
        }

        outDebug("guest_pml4 地址：%p\n", guest_pml4);

        memcpy(&pt.pml4[256], &guest_pml4[256], sizeof(pml4e_64) * 256);
        return true;
    }
}

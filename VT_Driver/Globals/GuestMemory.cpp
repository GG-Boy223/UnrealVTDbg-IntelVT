#include "GlobalsInternal.h"

namespace hv
{
    uint64_t gva2gpa(cr3 const guest_cr3, void* const gva, size_t* const offset_to_next_page)
    {
        if (offset_to_next_page)
        {
            *offset_to_next_page = 0;
        }

        pml4_virtual_address const vaddr = { gva };
        auto const pml4 = reinterpret_cast<pml4e_64*>(host_physical_memory_base + (guest_cr3.address_of_page_directory << 12));
        auto const pml4e = pml4[vaddr.pml4_idx];

        if (!pml4e.present)
        {
            return 0;
        }

        auto const pdpt = reinterpret_cast<pdpte_64*>(host_physical_memory_base + (pml4e.page_frame_number << 12));
        auto const pdpte = pdpt[vaddr.pdpt_idx];

        if (!pdpte.present)
        {
            return 0;
        }

        if (pdpte.large_page)
        {
            pdpte_1gb_64 pdpte_1gb;
            pdpte_1gb.flags = pdpte.flags;

            auto const offset = (vaddr.pd_idx << 21) + (vaddr.pt_idx << 12) + vaddr.offset;

            if (offset_to_next_page)
            {
                *offset_to_next_page = 0x40000000 - offset;
            }

            return (pdpte_1gb.page_frame_number << 30) + offset;
        }

        auto const pd = reinterpret_cast<pde_64*>(host_physical_memory_base + (pdpte.page_frame_number << 12));
        auto const pde = pd[vaddr.pd_idx];

        if (!pde.present)
        {
            return 0;
        }

        if (pde.large_page)
        {
            pde_2mb_64 pde_2mb;
            pde_2mb.flags = pde.flags;

            auto const offset = (vaddr.pt_idx << 12) + vaddr.offset;

            if (offset_to_next_page)
            {
                *offset_to_next_page = 0x200000 - offset;
            }

            return (pde_2mb.page_frame_number << 21) + offset;
        }

        auto const pt = reinterpret_cast<pte_64*>(host_physical_memory_base + (pde.page_frame_number << 12));
        auto const pte = pt[vaddr.pt_idx];

        if (!pte.present)
        {
            return 0;
        }

        if (offset_to_next_page)
        {
            *offset_to_next_page = 0x1000 - vaddr.offset;
        }

        return (pte.page_frame_number << 12) + vaddr.offset;
    }

    void* gva2hva(cr3 const guest_cr3, void* const gva, size_t* const offset_to_next_page)
    {
        auto const gpa = gva2gpa(guest_cr3, gva, offset_to_next_page);
        if (!gpa)
        {
            return nullptr;
        }

        return host_physical_memory_base + gpa;
    }

    void* gva2hva(void* const gva, size_t* const offset_to_next_page)
    {
        cr3 guest_cr3;
        guest_cr3.flags = vmread(GUEST_CR3);
        return gva2hva(guest_cr3, gva, offset_to_next_page);
    }

    uint64_t get_physical_address(unsigned __int64 guest_cr3, _In_ PVOID BaseAddress)
    {
        if (!guest_cr3)
        {
            return NULL;
        }

        cr3 tmp_cr3;
        tmp_cr3.flags = guest_cr3;
        return gva2gpa(tmp_cr3, BaseAddress);
    }

    size_t read_guest_virtual_memory(cr3 const guest_cr3,
        void* const gva, void* const hva, size_t const size)
    {
        auto const src = reinterpret_cast<uint8_t*>(gva);
        auto const dst = reinterpret_cast<uint8_t*>(hva);

        size_t bytes_read = 0;

        while (bytes_read < size)
        {
            size_t src_remaining = 0;
            auto const curr_src = gva2hva(guest_cr3, src + bytes_read, &src_remaining);

            if (!curr_src)
            {
                return bytes_read;
            }

            auto const curr_size = min(size - bytes_read, src_remaining);

            host_exception_info e = { 0 };
            memcpy_safe(e, dst + bytes_read, curr_src, curr_size);

            if (e.exception_occurred)
            {
                return bytes_read;
            }

            bytes_read += curr_size;
        }

        return bytes_read;
    }

    size_t write_guest_virtual_memory(cr3 const guest_cr3,
        void* const gva, void* const hva, size_t const size)
    {
        size_t bytes_read = 0;
        auto const dst = reinterpret_cast<uint8_t*>(gva);
        auto const src = reinterpret_cast<uint8_t*>(hva);

        while (bytes_read < size)
        {
            size_t dst_remaining = 0;
            auto const curr_dst = gva2hva(guest_cr3, dst + bytes_read, &dst_remaining);

            if (!curr_dst)
            {
                return bytes_read;
            }

            auto const curr_size = min(size - bytes_read, dst_remaining);

            host_exception_info e = { 0 };
            memcpy_safe(e, curr_dst, src + bytes_read, curr_size);

            if (e.exception_occurred)
            {
                return bytes_read;
            }

            bytes_read += curr_size;
        }
        return bytes_read;
    }

    size_t read_guest_virtual_memory(void* const gva, void* const hva, size_t const size)
    {
        cr3 guest_cr3;
        guest_cr3.flags = vmread(GUEST_CR3);
        return read_guest_virtual_memory(guest_cr3, gva, hva, size);
    }

    size_t write_guest_virtual_memory(void* const gva, void* const hva, size_t const size)
    {
        cr3 guest_cr3;
        guest_cr3.flags = vmread(GUEST_CR3);
        return write_guest_virtual_memory(guest_cr3, gva, hva, size);
    }
}

#include "GlobalsInternal.h"

namespace hv
{
    mtrr_data read_mtrr_data()
    {
        mtrr_data mtrrs;

        mtrrs.cap.flags = __readmsr(IA32_MTRR_CAPABILITIES);
        mtrrs.def_type.flags = __readmsr(IA32_MTRR_DEF_TYPE);
        mtrrs.var_count = 0;

        for (uint32_t i = 0; i < mtrrs.cap.variable_range_count; ++i)
        {
            ia32_mtrr_physmask_register mask;
            mask.flags = __readmsr(IA32_MTRR_PHYSMASK0 + i * 2);

            if (!mask.valid)
            {
                continue;
            }

            mtrrs.variable[mtrrs.var_count].mask = mask;
            mtrrs.variable[mtrrs.var_count].base.flags = __readmsr(IA32_MTRR_PHYSBASE0 + i * 2);

            ++mtrrs.var_count;
        }

        return mtrrs;
    }

    static uint8_t calc_mtrr_mem_type(mtrr_data const& mtrrs, uint64_t const pfn)
    {
        if (!mtrrs.def_type.mtrr_enable)
        {
            return MEMORY_TYPE_UNCACHEABLE;
        }

        if (pfn < 0x100 && mtrrs.cap.fixed_range_supported && mtrrs.def_type.fixed_range_mtrr_enable)
        {
            return MEMORY_TYPE_UNCACHEABLE;
        }

        uint8_t curr_mem_type = MEMORY_TYPE_INVALID;

        for (uint32_t i = 0; i < mtrrs.var_count; ++i)
        {
            auto const base = mtrrs.variable[i].base.page_frame_number;
            auto const mask = mtrrs.variable[i].mask.page_frame_number;

            if ((pfn & mask) == (base & mask))
            {
                auto const type = static_cast<uint8_t>(mtrrs.variable[i].base.type);

                if (type == MEMORY_TYPE_UNCACHEABLE)
                {
                    return MEMORY_TYPE_UNCACHEABLE;
                }

                if (type < curr_mem_type)
                {
                    curr_mem_type = type;
                }
            }
        }

        if (curr_mem_type == MEMORY_TYPE_INVALID)
        {
            return mtrrs.def_type.default_memory_type;
        }

        return curr_mem_type;
    }

    uint8_t calc_mtrr_mem_type(mtrr_data const& mtrrs, uint64_t address, uint64_t size)
    {
        address &= ~0xFFFull;
        size = (size + 0xFFF) & ~0xFFFull;

        uint8_t curr_mem_type = MEMORY_TYPE_INVALID;

        for (uint64_t curr = address; curr < address + size; curr += 0x1000)
        {
            auto const type = calc_mtrr_mem_type(mtrrs, curr >> 12);

            if (type == MEMORY_TYPE_UNCACHEABLE)
            {
                return type;
            }

            if (type < curr_mem_type)
            {
                curr_mem_type = type;
            }
        }

        if (curr_mem_type == MEMORY_TYPE_INVALID)
        {
            return MEMORY_TYPE_UNCACHEABLE;
        }

        return curr_mem_type;
    }

    void set_ept_memory_type(__ept_state& ept_state, uint8_t const memory_type)
    {
        sLog("\n");
        if (ept_state.pml2_pages == nullptr)
        {
            return;
        }

        for (size_t i = 0; i < ept_state.mapped_pdpte_count; ++i)
        {
            for (size_t j = 0; j < 512; ++j)
            {
                auto& pde = ept_state.pml2_pages[i][j];

                if (pde.page_directory_entry.large_page)
                {
                    pde.page_directory_entry.memory_type = memory_type;
                }
                else
                {
                    auto const pt = reinterpret_cast<ept_pte*>(host_physical_memory_base + (pde.large_page.page_frame_number << 12));

                    for (size_t k = 0; k < 512; ++k)
                    {
                        pt[k].memory_type = memory_type;
                    }
                }
            }
        }
    }

    void update_ept_memory_type(__ept_state& ept_state)
    {
        sLog("\n");
        if (ept_state.pml2_pages == nullptr)
        {
            return;
        }

        auto const mtrrs = read_mtrr_data();

        for (size_t i = 0; i < ept_state.mapped_pdpte_count; ++i)
        {
            for (size_t j = 0; j < 512; ++j)
            {
                auto& pde = ept_state.pml2_pages[i][j];

                if (pde.page_directory_entry.large_page)
                {
                    // pde.page_directory_entry.memory_type = calc_mtrr_mem_type(mtrrs,
                    //     pde.page_directory_entry.page_frame_number << 21, 0x1000 << 9); // 旧逻辑：运行期MTRR刷新不区分RAM与MMIO空洞。
                    uint64_t const pde_physical_address =
                        static_cast<uint64_t>(pde.page_directory_entry.page_frame_number) << 21; // 位域先转成64位，避免MSVC在多行实参里解析异常。
                    pde.page_directory_entry.memory_type = ept::get_ept_memory_type_for_range(mtrrs,
                        ept_state,
                        pde_physical_address,
                        0x1000ull << 9);
                }
                else
                {
                    auto const pt = reinterpret_cast<ept_pte*>(host_physical_memory_base + (pde.large_page.page_frame_number << 12));

                    for (size_t k = 0; k < 512; ++k)
                    {
                        // pt[k].memory_type = calc_mtrr_mem_type(mtrrs, pt[k].page_frame_number << 12, 0x1000); // 旧逻辑：split页刷新后仍可能把MMIO设为WB。
                        uint64_t const pte_physical_address =
                            static_cast<uint64_t>(pt[k].page_frame_number) << 12; // 4KB页同样按64位物理地址计算，避免高物理地址截断。
                        pt[k].memory_type = ept::get_ept_memory_type_for_range(mtrrs, ept_state, pte_physical_address, 0x1000);
                    }
                }
            }
        }
    }
}

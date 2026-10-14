#include "GlobalsInternal.h"
#include <Ring0/Hvm/MtrrTypePolicy.h>

namespace hv
{
    static_assert(mtrr::type_policy::uncacheable == MEMORY_TYPE_UNCACHEABLE);
    static_assert(mtrr::type_policy::write_through == MEMORY_TYPE_WRITE_THROUGH);
    static_assert(mtrr::type_policy::write_back == MEMORY_TYPE_WRITE_BACK);
    static_assert(mtrr::type_policy::invalid == MEMORY_TYPE_INVALID);

    mtrr_data read_mtrr_data()
    {
        mtrr_data mtrrs;

        mtrrs.cap.flags = __readmsr(IA32_MTRR_CAPABILITIES); // 读取 MTRR 能力寄存器
        mtrrs.def_type.flags = __readmsr(IA32_MTRR_DEF_TYPE); // 读取默认内存类型寄存器
        mtrrs.var_count = 0; // 初始化有效条目计数

        for (uint32_t i = 0; i < mtrrs.cap.variable_range_count; ++i)
        {
            ia32_mtrr_physmask_register mask;
            mask.flags = __readmsr(IA32_MTRR_PHYSMASK0 + i * 2); // 读取掩码寄存器

            if (!mask.valid) // 无效范围跳过
            {
                continue;
            }

            mtrrs.variable[mtrrs.var_count].mask = mask; // 保存掩码
            mtrrs.variable[mtrrs.var_count].base.flags = __readmsr(IA32_MTRR_PHYSBASE0 + i * 2); // 保存基址

            ++mtrrs.var_count; // 统计有效条目
        }

        return mtrrs; // 返回 MTRR 数据
    }

    static uint8_t calc_mtrr_mem_type(mtrr_data const& mtrrs, uint64_t const pfn)
    {
        if (!mtrrs.def_type.mtrr_enable)
        {
            return MEMORY_TYPE_UNCACHEABLE;
        }

        // fixed range MTRRs
        // 固定范围MTRRs
        if (pfn < 0x100 && mtrrs.cap.fixed_range_supported && mtrrs.def_type.fixed_range_mtrr_enable)
        {
            return MEMORY_TYPE_UNCACHEABLE;
        }

        uint8_t curr_mem_type = MEMORY_TYPE_INVALID;

        for (uint32_t i = 0; i < mtrrs.var_count; ++i)
        {
            auto const base = mtrrs.variable[i].base.page_frame_number; // 可变范围基址 PFN
            auto const mask = mtrrs.variable[i].mask.page_frame_number; // 可变范围掩码 PFN

            if ((pfn & mask) == (base & mask))
            {
                auto const type = static_cast<uint8_t>(mtrrs.variable[i].base.type); // 读取该范围内存类型

                curr_mem_type = mtrr::type_policy::merge(curr_mem_type, type);
                if (curr_mem_type == MEMORY_TYPE_UNCACHEABLE)
                {
                    return MEMORY_TYPE_UNCACHEABLE;
                }
            }
        }

        // no MTRR covers the specified address
        //未被 MTRR 映射的地址范围应设置为默认类型
        if (curr_mem_type == MEMORY_TYPE_INVALID)
        {
            return mtrrs.def_type.default_memory_type; // 使用默认类型
        }

        return curr_mem_type;
    }

    uint8_t calc_mtrr_mem_type(mtrr_data const& mtrrs, uint64_t address, uint64_t size)
    {
        // base address must be on atleast a 4KB boundary
        // 基地址必须至少位于 4KB 边界上
        address &= ~0xFFFull; // 对齐地址到 4KB
        // minimum range size is 4KB
        // 最小范围大小为 4KB
        size = (size + 0xFFF) & ~0xFFFull; // 对齐大小到 4KB

        uint8_t curr_mem_type = MEMORY_TYPE_INVALID;

        for (uint64_t curr = address; curr < address + size; curr += 0x1000)
        {
            auto const type = calc_mtrr_mem_type(mtrrs, curr >> 12); // 逐页计算类型

            curr_mem_type = mtrr::type_policy::merge(curr_mem_type, type);
            if (curr_mem_type == MEMORY_TYPE_UNCACHEABLE)
            {
                return MEMORY_TYPE_UNCACHEABLE; // UC 优先级最高，直接返回
            }
        }

        // no MTRR covers the specified address
        //未被 MTRR 映射的地址范围应设置为默认类型
        if (curr_mem_type == MEMORY_TYPE_INVALID)
        {
            return MEMORY_TYPE_UNCACHEABLE;
        }

        return curr_mem_type;
    }

    // 对齐 HyperDbg EptGetMemoryType：基于 build_mtrr_map 得到的完整 MTRR 范围
    // （含 fixed/SMRR/variable）做单物理地址点查询。
    // 优先级（Intel SDM 12.11.4.1 MTRR Precedences）：
    //   - fixed-range 命中直接采用（优先于 variable-range）；
    //   - UC 一旦出现即最终 UC（最高优先级）；
    //   - WT 与 WB 重叠取 WT，但继续查找是否还有 UC 覆盖；
    //   - 无覆盖取 default_memory_type。
    uint8_t calc_mtrr_mem_type_for_address(uint64_t address)
    {
        auto const& info = g_vmm_context.mtrr_info;
        uint8_t target = MEMORY_TYPE_INVALID;

        for (unsigned int i = 0; i < info.enabled_memory_ranges; ++i)
        {
            auto const& range = info.memory_range[i];
            if (address < range.physcial_base_address || address >= range.physcial_end_address)
            {
                continue;
            }

            if (range.fixed_range)
            {
                target = range.memory_type;
                break;
            }

            if (target == MEMORY_TYPE_UNCACHEABLE)
            {
                target = range.memory_type;
                break;
            }

            if (target == MEMORY_TYPE_WRITE_THROUGH || range.memory_type == MEMORY_TYPE_WRITE_THROUGH)
            {
                if (target == MEMORY_TYPE_WRITE_BACK)
                {
                    target = MEMORY_TYPE_WRITE_THROUGH;
                    continue;
                }
            }

            target = range.memory_type;
        }

        if (target == MEMORY_TYPE_INVALID)
        {
            target = info.default_memory_type;
        }
        return target;
    }

    // 对齐 HyperDbg EptIsValidForLargePage：判断 2MB 大页是否跨 MTRR 范围边界。
    // 跨边界则该 2MB 页内存在多种内存类型，不能用单一类型描述，必须拆 4KB。
    bool is_valid_for_large_page(uint64_t address)
    {
        uint64_t const start = address;
        uint64_t const end = address + (LARGE_PAGE_SIZE - 1);
        auto const& info = g_vmm_context.mtrr_info;

        for (unsigned int i = 0; i < info.enabled_memory_ranges; ++i)
        {
            auto const& range = info.memory_range[i];
            if ((start <= range.physcial_end_address && end > range.physcial_end_address) ||
                (start < range.physcial_base_address && end >= range.physcial_base_address))
            {
                return false;
            }
        }
        return true;
    }

    // set the memory type in every EPT paging structure to the specified value
    // 将每个 EPT 分页结构中的内存类型设置为指定值将每个 EPT 分页结构中的内存类型设置为指定值
    void set_ept_memory_type(__ept_state& ept_state, uint8_t const memory_type)
    {
        sLog("\n");
        if (ept_state.pml2_pages == nullptr)
        {
            return;
        }

        for (size_t i = 0; i < ept_state.pml2_pages_count; ++i)
        {
            if (ept_state.pml2_pages[i] == nullptr)
            {
                continue; // 该 1GB 区域由 1GB EPT 大页条目直接映射，没有 PD 可遍历
            }

            for (size_t j = 0; j < 512; ++j)
            {
                auto& pde = ept_state.pml2_pages[i][j];

                // 2MB large page
                // 2MB 大页面
                if (pde.page_directory_entry.large_page)
                {
                    pde.page_directory_entry.memory_type = memory_type; // 直接设置 PDE 类型
                }
                else
                {
                    // PDE 指向一个 PT：用拆分链表按 PDE 指针定位 PML1，避免物理->虚拟反查
                    auto const pt = reinterpret_cast<ept_pte*>(ept::get_split_pml1_by_pde(ept_state, &pde));
                    if (pt == nullptr)
                    {
                        return;
                    }

                    // update the memory type for every PTE
                    // 更新每个 PTE 的内存类型
                    for (size_t k = 0; k < 512; ++k)
                    {
                        pt[k].memory_type = memory_type; // 逐项设置类型
                    }
                }
            }
        }
    }

    // update the memory types in the EPT paging structures based on the MTRRs.
    // 根据 MTRR 更新 EPT 分页结构中的内存类型。
    // this function should only be called from root-mode during vmx-operation.
    // 此函数应仅在 vmx-operation 期间从host调用。
    void update_ept_memory_type(__ept_state& ept_state)
    {
        sLog("\n");
        if (ept_state.pml2_pages == nullptr)
        {
            return;
        }

        for (size_t i = 0; i < ept_state.pml2_pages_count; ++i)
        {
            if (ept_state.pml2_pages[i] == nullptr)
            {
                continue; // 该 1GB 区域由 1GB EPT 大页条目直接映射，没有 PD 可遍历
            }

            for (size_t j = 0; j < 512; ++j)
            {
                auto& pde = ept_state.pml2_pages[i][j];

                // 2MB large page
                // 2MB 大页面
                if (pde.page_directory_entry.large_page)
                {
                    uint64_t const pde_physical_address =
                        static_cast<uint64_t>(pde.page_directory_entry.page_frame_number) << 21; // 位域先转成64位，避免MSVC在多行实参里解析异常。
                    pde.page_directory_entry.memory_type = calc_mtrr_mem_type_for_address(pde_physical_address);
                }
                else
                {
                    // PDE 指向一个 PT：用拆分链表按 PDE 指针定位 PML1，避免物理->虚拟反查
                    auto const pt = reinterpret_cast<ept_pte*>(ept::get_split_pml1_by_pde(ept_state, &pde));
                    if (pt == nullptr)
                    {
                        return;
                    }

                    // update the memory type for every PTE
                    // 更新每个 PTE 的内存类型
                    for (size_t k = 0; k < 512; ++k)
                    {
                        uint64_t const pte_physical_address =
                            static_cast<uint64_t>(pt[k].page_frame_number) << 12; // 4KB页同样按64位物理地址计算，避免高物理地址截断。
                        pt[k].memory_type = calc_mtrr_mem_type_for_address(pte_physical_address);
                    }
                }
            }
        }
    }
}

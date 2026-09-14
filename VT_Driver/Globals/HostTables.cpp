#include "GlobalsInternal.h"

namespace hv
{
    namespace
    {
        bool get_physical_mapping_va(uint64_t physical_address, void** virtual_address)
        {
            if (virtual_address == nullptr)
            {
                return false;
            }

            PHYSICAL_ADDRESS address;
            address.QuadPart = physical_address & ~0xfffull;
            uint8_t* mapped = static_cast<uint8_t*>(MmGetVirtualForPhysical(address));
            if (mapped == nullptr)
            {
                *virtual_address = nullptr;
                return false;
            }

            *virtual_address = mapped + (physical_address & 0xfffull);
            return true;
        }

        bool translate_host_virtual_address(void const* virtual_address, uint64_t* physical_address)
        {
            if (virtual_address == nullptr)
            {
                return false;
            }

            pml4_virtual_address const vaddr = { virtual_address };
            uint64_t const address_value = reinterpret_cast<uint64_t>(virtual_address);

            if (vaddr.pml4_idx == host_physical_memory_pml4_idx)
            {
                uint64_t const base = reinterpret_cast<uint64_t>(host_physical_memory_base);
                uint64_t const size = static_cast<uint64_t>(HOST_PHYSICAL_MEMORY_PD_COUNT) << 30;
                if (address_value < base || address_value - base >= size)
                {
                    return false;
                }

                if (physical_address != nullptr)
                {
                    *physical_address = address_value - base;
                }
                return true;
            }

            auto const& pml4e = ghv.host_page_tables.pml4[vaddr.pml4_idx];
            //判断该页是否存在
            //当P=1指示表或物理页面已加载到物理内存中
            if (!pml4e.present)
            {
                return false;
            }

            void* pdpt_va = nullptr;
            if (!get_physical_mapping_va(pml4e.page_frame_number << 12, &pdpt_va))
            {
                return false;
            }

            auto const pdpt = static_cast<pdpte_64*>(pdpt_va);
            auto const& pdpte = pdpt[vaddr.pdpt_idx];
            if (!pdpte.present)
            {
                return false;
            }

            if (pdpte.large_page)
            {
                pdpte_1gb_64 pdpte_1gb; // 1GB 大页结构
                pdpte_1gb.flags = pdpte.flags; // 复制标志位
                if (physical_address != nullptr)
                {
                    *physical_address =
                        (pdpte_1gb.page_frame_number << 30) +
                        (vaddr.pd_idx << 21) +
                        (vaddr.pt_idx << 12) +
                        vaddr.offset;
                }
                return true;
            }

            void* pd_va = nullptr;
            if (!get_physical_mapping_va(pdpte.page_frame_number << 12, &pd_va))
            {
                return false;
            }

            auto const pd = static_cast<pde_64*>(pd_va);
            auto const& pde = pd[vaddr.pd_idx];
            if (!pde.present)
            {
                return false;
            }

            if (pde.large_page)
            {
                pde_2mb_64 pde_2mb; // 2MB 大页结构
                pde_2mb.flags = pde.flags; // 复制标志位
                if (physical_address != nullptr)
                {
                    *physical_address =
                        (pde_2mb.page_frame_number << 21) +
                        (vaddr.pt_idx << 12) +
                        vaddr.offset;
                }
                return true;
            }

            void* pt_va = nullptr;
            if (!get_physical_mapping_va(pde.page_frame_number << 12, &pt_va))
            {
                return false;
            }

            auto const pt = static_cast<pte_64*>(pt_va);
            auto const& pte = pt[vaddr.pt_idx];
            if (!pte.present)
            {
                return false;
            }

            if (physical_address != nullptr)
            {
                *physical_address = (pte.page_frame_number << 12) + vaddr.offset;
            }
            return true;
        }

        bool validate_host_address(char const* name, void const* virtual_address)
        {
            uint64_t physical_address = 0;
            bool const ok = translate_host_virtual_address(virtual_address, &physical_address);
            outFile("%s[Host页表自检] %s VA=0x%llX PA=0x%llX",
                ok ? "[成功]" : "[失败]",
                name,
                reinterpret_cast<uint64_t>(virtual_address),
                ok ? physical_address : 0);
            return ok;
        }

        void free_host_dynamic_page_table_pages(host_page_tables& pt)
        {
            for (unsigned int i = 0; i < pt.dynamic_pdpt_count; ++i)
            {
                MmFreeContiguousMemory(pt.dynamic_pdpts[i]); // 动态PDPT页来自MmAllocateContiguousMemory，必须用对应释放函数。
                pt.dynamic_pdpts[i] = nullptr;
            }

            for (unsigned int i = 0; i < pt.dynamic_pd_count; ++i)
            {
                MmFreeContiguousMemory(pt.dynamic_pds[i]); // 动态PD页仅用于Host关键虚拟页补映射，释放时不影响Guest页表。
                pt.dynamic_pds[i] = nullptr;
            }

            for (unsigned int i = 0; i < pt.dynamic_pt_count; ++i)
            {
                MmFreeContiguousMemory(pt.dynamic_pts[i]); // 动态PT页释放后HOST_CR3不会再被使用，避免失败重试泄漏。
                pt.dynamic_pts[i] = nullptr;
            }

            pt.dynamic_pdpt_count = 0;
            pt.dynamic_pd_count = 0;
            pt.dynamic_pt_count = 0;
        }

        void* allocate_host_page_table_page(char const* level)
        {
            PHYSICAL_ADDRESS highest_address;
            highest_address.QuadPart = MAXULONG64;
            void* page = MmAllocateContiguousMemory(PAGE_SIZE, highest_address);
            if (page == nullptr)
            {
                outFile("[警告][Host页表覆盖] 分配%s页失败：仅记录，不阻断VT进入链路", level);
                return nullptr; // 失败返回空指针
            }

            RtlSecureZeroMemory(page, PAGE_SIZE);
            return page;
        }

        pdpte_64* find_dynamic_pdpt(host_page_tables& pt, unsigned __int16 pml4_idx)
        {
            for (unsigned int i = 0; i < pt.dynamic_pdpt_count; ++i)
            {
                if (pt.dynamic_pdpt_pml4_index[i] == pml4_idx)
                {
                    return static_cast<pdpte_64*>(pt.dynamic_pdpts[i]);
                }
            }

            return nullptr; // 失败返回空指针
        }

        pde_64* find_dynamic_pd(host_page_tables& pt, unsigned __int16 pml4_idx, unsigned __int16 pdpt_idx)
        {
            for (unsigned int i = 0; i < pt.dynamic_pd_count; ++i)
            {
                if (pt.dynamic_pd_pml4_index[i] == pml4_idx &&
                    pt.dynamic_pd_pdpt_index[i] == pdpt_idx)
                {
                    return static_cast<pde_64*>(pt.dynamic_pds[i]);
                }
            }

            return nullptr; // 失败返回空指针
        }

        pte_64* find_dynamic_pt(host_page_tables& pt, unsigned __int16 pml4_idx, unsigned __int16 pdpt_idx, unsigned __int16 pd_idx)
        {
            for (unsigned int i = 0; i < pt.dynamic_pt_count; ++i)
            {
                if (pt.dynamic_pt_pml4_index[i] == pml4_idx &&
                    pt.dynamic_pt_pdpt_index[i] == pdpt_idx &&
                    pt.dynamic_pt_pd_index[i] == pd_idx)
                {
                    return static_cast<pte_64*>(pt.dynamic_pts[i]);
                }
            }

            return nullptr; // 失败返回空指针
        }

        pdpte_64* get_or_create_dynamic_pdpt(host_page_tables& pt, unsigned __int16 pml4_idx)
        {
            if (auto const existing = find_dynamic_pdpt(pt, pml4_idx))
            {
                return existing;
            }

            if (pt.dynamic_pdpt_count >= HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT)
            {
                outFile("[警告][Host页表覆盖] 动态PDPT池耗尽：PML4=%u Capacity=%u", pml4_idx, HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT);
                return nullptr; // 失败返回空指针
            }

            auto const source_pml4e = pt.pml4[pml4_idx];
            auto const page = static_cast<pdpte_64*>(allocate_host_page_table_page("PDPT"));
            if (page == nullptr)
            {
                return nullptr; // 失败返回空指针
            }

            if (source_pml4e.present)
            {
                void* source_pdpt = nullptr;
                if (get_physical_mapping_va(source_pml4e.page_frame_number << 12, &source_pdpt))
                {
                    memcpy(page, source_pdpt, PAGE_SIZE); // 克隆原PDPT，保留该PML4槽已有的内核映射，只补缺页。
                }
                else
                {
                    outFile("[警告][Host页表覆盖] 原PDPT不可映射，使用空PDPT继续补关键页：PML4=%u", pml4_idx);
                }
            }

            auto& pml4e = pt.pml4[pml4_idx];
            // pml4e.flags = source_pml4e.flags; // 旧逻辑：只复制dwm.exe PML4，不为缺失关键页建立可控PDPT。
            pml4e.flags = source_pml4e.present ? source_pml4e.flags : 0;
            pml4e.present = 1;
            pml4e.write = 1;
            pml4e.supervisor = 0;
            pml4e.execute_disable = 0;
            pml4e.page_frame_number = MmGetPhysicalAddress(page).QuadPart >> 12;

            unsigned int const slot = pt.dynamic_pdpt_count++;
            pt.dynamic_pdpts[slot] = page;
            pt.dynamic_pdpt_pml4_index[slot] = pml4_idx;
            return page;
        }

        pde_64* get_or_create_dynamic_pd(host_page_tables& pt, unsigned __int16 pml4_idx, unsigned __int16 pdpt_idx, pdpte_64* pdpt)
        {
            if (auto const existing = find_dynamic_pd(pt, pml4_idx, pdpt_idx))
            {
                return existing;
            }

            if (pdpt == nullptr)
            {
                return nullptr; // 失败返回空指针
            }

            if (pt.dynamic_pd_count >= HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT)
            {
                outFile("[警告][Host页表覆盖] 动态PD池耗尽：PML4=%u PDPT=%u Capacity=%u", pml4_idx, pdpt_idx, HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT);
                return nullptr; // 失败返回空指针
            }

            auto const source_pdpte = pdpt[pdpt_idx];
            if (source_pdpte.present && source_pdpte.large_page)
            {
                return nullptr; // 1GB大页本身已覆盖该范围；这里不拆分原有大页，避免扩大回归面。
            }

            auto const page = static_cast<pde_64*>(allocate_host_page_table_page("PD"));
            if (page == nullptr)
            {
                return nullptr; // 失败返回空指针
            }

            if (source_pdpte.present)
            {
                void* source_pd = nullptr;
                if (get_physical_mapping_va(source_pdpte.page_frame_number << 12, &source_pd))
                {
                    memcpy(page, source_pd, PAGE_SIZE); // 克隆原PD，保留同一1GB范围内已有映射。
                }
                else
                {
                    outFile("[警告][Host页表覆盖] 原PD不可映射，使用空PD继续补关键页：PML4=%u PDPT=%u", pml4_idx, pdpt_idx);
                }
            }

            auto& pdpte = pdpt[pdpt_idx];
            // pdpte.flags = source_pdpte.flags; // 旧逻辑：缺页时只记录自检，不主动挂入可控PD。
            pdpte.flags = source_pdpte.present ? source_pdpte.flags : 0;
            pdpte.present = 1;
            pdpte.write = 1;
            pdpte.supervisor = 0;
            pdpte.large_page = 0;
            pdpte.execute_disable = 0; // 允许执行
            pdpte.page_frame_number = MmGetPhysicalAddress(page).QuadPart >> 12;

            unsigned int const slot = pt.dynamic_pd_count++;
            pt.dynamic_pds[slot] = page;
            pt.dynamic_pd_pml4_index[slot] = pml4_idx;
            pt.dynamic_pd_pdpt_index[slot] = pdpt_idx;
            return page;
        }

        pte_64* get_or_create_dynamic_pt(host_page_tables& pt, unsigned __int16 pml4_idx, unsigned __int16 pdpt_idx, unsigned __int16 pd_idx, pde_64* pd)
        {
            if (auto const existing = find_dynamic_pt(pt, pml4_idx, pdpt_idx, pd_idx))
            {
                return existing;
            }

            if (pd == nullptr)
            {
                return nullptr; // 失败返回空指针
            }

            if (pt.dynamic_pt_count >= HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT)
            {
                outFile("[警告][Host页表覆盖] 动态PT池耗尽：PML4=%u PDPT=%u PD=%u Capacity=%u", pml4_idx, pdpt_idx, pd_idx, HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT);
                return nullptr; // 失败返回空指针
            }

            auto const source_pde = pd[pd_idx];
            if (source_pde.present && source_pde.large_page)
            {
                return nullptr; // 2MB大页已覆盖该范围；不拆分原有大页。
            }

            auto const page = static_cast<pte_64*>(allocate_host_page_table_page("PT"));
            if (page == nullptr)
            {
                return nullptr; // 失败返回空指针
            }

            if (source_pde.present)
            {
                void* source_pt = nullptr;
                if (get_physical_mapping_va(source_pde.page_frame_number << 12, &source_pt))
                {
                    memcpy(page, source_pt, PAGE_SIZE); // 克隆原PT，保留同一2MB范围内已有页映射。
                }
                else
                {
                    outFile("[警告][Host页表覆盖] 原PT不可映射，使用空PT继续补关键页：PML4=%u PDPT=%u PD=%u", pml4_idx, pdpt_idx, pd_idx);
                }
            }

            auto& pde = pd[pd_idx];
            // pde.flags = source_pde.flags; // 旧逻辑：缺页时没有为4KB关键页挂入可控PT。
            pde.flags = source_pde.present ? source_pde.flags : 0;
            pde.present = 1; // 有效
            pde.write = 1; // 可写
            pde.supervisor = 0; // 内核访问
            pde.large_page = 0;
            pde.execute_disable = 0; // 允许执行
            pde.page_frame_number = MmGetPhysicalAddress(page).QuadPart >> 12;

            unsigned int const slot = pt.dynamic_pt_count++;
            pt.dynamic_pts[slot] = page;
            pt.dynamic_pt_pml4_index[slot] = pml4_idx;
            pt.dynamic_pt_pdpt_index[slot] = pdpt_idx;
            pt.dynamic_pt_pd_index[slot] = pd_idx;
            return page;
        }

        bool map_host_virtual_page(host_page_tables& pt, void const* virtual_page)
        {
            uint64_t existing_physical = 0;
            if (translate_host_virtual_address(virtual_page, &existing_physical))
            {
                return true;
            }

            pml4_virtual_address const vaddr = { virtual_page };
            if (vaddr.pml4_idx == host_physical_memory_pml4_idx)
            {
                outFile("[警告][Host页表覆盖] 目标VA落在物理直映槽，跳过补映射：VA=0x%llX PML4=%llu",
                    reinterpret_cast<uint64_t>(virtual_page),
                    vaddr.pml4_idx);
                return false;
            }

            auto const pdpt = get_or_create_dynamic_pdpt(pt, static_cast<unsigned __int16>(vaddr.pml4_idx));
            if (translate_host_virtual_address(virtual_page, &existing_physical))
            {
                return true; // 克隆PML4分支后已有大页/原映射覆盖，无需继续拆到4KB。
            }

            auto const pd = get_or_create_dynamic_pd(pt,
                static_cast<unsigned __int16>(vaddr.pml4_idx),
                static_cast<unsigned __int16>(vaddr.pdpt_idx),
                pdpt);
            if (translate_host_virtual_address(virtual_page, &existing_physical))
            {
                return true; // 克隆PDPT分支后已有大页/原映射覆盖，无需继续创建PT。
            }

            auto const pt_page = get_or_create_dynamic_pt(pt,
                static_cast<unsigned __int16>(vaddr.pml4_idx),
                static_cast<unsigned __int16>(vaddr.pdpt_idx),
                static_cast<unsigned __int16>(vaddr.pd_idx),
                pd);
            if (pt_page == nullptr)
            {
                return false;
            }

            if (!MmIsAddressValid(const_cast<void*>(virtual_page)))
            {
                outFile("[警告][Host页表覆盖] 目标VA当前无有效物理页，跳过补映射：VA=0x%llX",
                    reinterpret_cast<uint64_t>(virtual_page));
                return false;
            }

            PHYSICAL_ADDRESS physical_address = MmGetPhysicalAddress(const_cast<void*>(virtual_page));
            auto& pte = pt_page[vaddr.pt_idx];
            // if (!translate_host_virtual_address(virtual_page, &existing_physical)) { return false; } // 旧逻辑：只检查缺口，不补齐缺失PTE。
            pte.flags = 0;
            pte.present = 1;
            pte.write = 1;
            pte.supervisor = 0;
            pte.global = 0;
            pte.execute_disable = 0;
            pte.page_frame_number = physical_address.QuadPart >> 12;

            uint64_t mapped_physical = 0;
            if (!translate_host_virtual_address(virtual_page, &mapped_physical))
            {
                outFile("[警告][Host页表覆盖] 补映射后仍不可翻译：VA=0x%llX PA=0x%llX",
                    reinterpret_cast<uint64_t>(virtual_page),
                    physical_address.QuadPart);
                return false;
            }

            if ((mapped_physical & ~0xfffull) != (physical_address.QuadPart & ~0xfffull))
            {
                outFile("[警告][Host页表覆盖] 补映射PA不一致：VA=0x%llX ExpectPA=0x%llX ActualPA=0x%llX",
                    reinterpret_cast<uint64_t>(virtual_page),
                    physical_address.QuadPart,
                    mapped_physical);
                return false;
            }

            return true;
        }

        bool ensure_host_virtual_range(char const* name, void const* base, unsigned __int64 size)
        {
            if (base == nullptr || size == 0)
            {
                outFile("[警告][Host页表覆盖] %s范围为空：Base=0x%p Size=0x%llX，仅记录不阻断", name, base, size);
                return false;
            }

            uint64_t const start_value = reinterpret_cast<uint64_t>(base);
            if (start_value > MAXUINT64 - (size - 1))
            {
                outFile("[警告][Host页表覆盖] %s范围溢出：Base=0x%llX Size=0x%llX", name, start_value, size);
                return false;
            }

            auto& pt = ghv.host_page_tables; // 获取全局宿主页表对象
            uint64_t const start_page = start_value & ~0xfffull;
            uint64_t const last_page = (start_value + size - 1) & ~0xfffull;
            unsigned __int64 total_pages = 0;
            unsigned __int64 already_mapped_pages = 0;
            unsigned __int64 added_pages = 0;
            unsigned __int64 failed_pages = 0;

            for (uint64_t page = start_page;; page += PAGE_SIZE)
            {
                ++total_pages;

                uint64_t ignored_physical = 0;
                if (translate_host_virtual_address(reinterpret_cast<void const*>(page), &ignored_physical))
                {
                    ++already_mapped_pages;
                }
                else if (map_host_virtual_page(pt, reinterpret_cast<void const*>(page)))
                {
                    ++added_pages;
                }
                else
                {
                    ++failed_pages;
                }

                if (page == last_page || page > MAXUINT64 - PAGE_SIZE)
                {
                    break;
                }
            }

            outFile("[信息][Host页表覆盖] %s：Base=0x%llX Size=0x%llX Pages=%llu 已覆盖=%llu 新增=%llu 失败=%llu",
                name,
                start_value,
                size,
                total_pages,
                already_mapped_pages,
                added_pages,
                failed_pages);
            return failed_pages == 0;
        }
    }

    ia32_vmx_procbased_ctls_register read_ctrl_proc_based()
    {
        ia32_vmx_procbased_ctls_register value; // 本地结构：用于返回 Primary 控制寄存器字段
        value.flags = vmread(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL); // 读取 VMCS 中 Primary 控制位，获知当前 VM-Exit 策略
        return value; // 返回给调用方，用于修改或检查控制位
    }

    void write_ctrl_proc_based(ia32_vmx_procbased_ctls_register const value)
    {
        vmwrite(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL, value.flags); // 写回 Primary 控制位，实时影响 VM-Exit 行为
    }

    void prepare_external_structures(__vcpu* const vcpu)
    {
        memset(&vcpu->msr_bitmap, 0, sizeof(vcpu->msr_bitmap)); // 清空 MSR 位图：默认不拦截 MSR，提高性能，减少 VM-Exit
        // we don't care about anything that's in the TSS
        memset(&vcpu->host_tss, 0, sizeof(vcpu->host_tss)); // 清零 TSS：避免未初始化内容导致异常或不稳定

        prepare_host_idt(vcpu->host_idt); // 构造宿主 IDT：确保 VM-Exit 后中断可正确分派
        prepare_host_gdt(vcpu->host_gdt, &vcpu->host_tss); // 构造宿主 GDT：提供 CS/TR 等段描述符
    }

    void inject_nmi()
    {
        //vmentry_interrupt_information interrupt_info;
        //interrupt_info.flags = 0;
        //interrupt_info.vector = nmi;
        //interrupt_info.interruption_type = non_maskable_interrupt;
        //interrupt_info.deliver_error_code = 0;
        //interrupt_info.valid = 1;
        //vmwrite(VM_ENTRY_INTERRUPTION_INFO_FIELD, interrupt_info.flags);
        inject_interruption(EXCEPTION_VECTOR_NMII, INTERRUPT_TYPE_NMI, 0, false); // 通过 VM-Entry 字段注入 NMI，维持来宾中断语义
    }

    cr0 read_effective_guest_cr0()
    {
        // TODO: cache this value
        auto const mask = vmread(CR0_GUEST_HOST_MASK); // 读取 CR0 掩码：指示哪些位由 VMCS 控制读取

        // bits set to 1 in the mask are read from CR0, otherwise from the shadow
        cr0 cr0; // 组合结果的 CR0 结构
        cr0.flags = (vmread(CR0_READ_SHADOW) & mask) | (vmread(GUEST_CR0) & ~mask); // 掩码位从 shadow 取，其余从真实值取

        return cr0; // 返回来宾有效 CR0，用于指令模拟或校验
    }

    cr4 read_effective_guest_cr4()
    {
        // TODO: cache this value
        auto const mask = vmread(CR4_GUEST_HOST_MASK); // 读取 CR4 掩码：哪些位受 VMCS 控制

        // bits set to 1 in the mask are read from CR4, otherwise from the shadow
        cr4 cr4; // 组合结果的 CR4 结构
        cr4.flags = (vmread(CR4_READ_SHADOW) & mask) | (vmread(GUEST_CR4) & ~mask); // 按掩码组合 shadow 与真实值

        return cr4; // 返回来宾有效 CR4
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
            auto& pdpte = pt.phys_pdpt[i]; // PDPTE 条目
            pdpte.flags = 0; // 清零标志
            pdpte.present = 1; // 标记存在
            pdpte.write = 1;
            pdpte.supervisor = 0;
            pdpte.page_level_write_through = 0; // 禁止写穿
            pdpte.page_level_cache_disable = 0; // 允许缓存
            pdpte.accessed = 0; // 清零访问位
            pdpte.execute_disable = 0; // 允许执行
            pdpte.page_frame_number = MmGetPhysicalAddress(&pt.phys_pds[i]).QuadPart >> 12; // 指向对应 PD

            for (uint64_t j = 0; j < 512; ++j)
            {
                auto& pde = pt.phys_pds[i][j]; // PDE 条目
                pde.flags = 0; // 清零
                pde.present = 1; // 有效
                pde.write = 1; // 可写
                pde.supervisor = 0; // 内核访问
                pde.page_level_write_through = 0; // 禁止写穿
                pde.page_level_cache_disable = 0; // 允许缓存
                pde.accessed = 0; // 清零访问位
                pde.dirty = 0; // 清零脏位
                pde.large_page = 1;
                pde.global = 0;
                pde.pat = 0;
                pde.execute_disable = 0; // 允许执行
                pde.page_frame_number = (i << 9) + j; // 线性映射 PFN，形成物理直映
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
        pml4e.page_frame_number = MmGetPhysicalAddress(&pt.phys_pdpt).QuadPart >> 12; // 指向 PDPT

        pdpte_64 pdpte = { 0 }; // PDPTE 模板
        pdpte.present = 1; // 有效
        pdpte.write = 1;
        pdpte.supervisor = 0;
        pdpte.page_level_write_through = 0; // 禁止写穿
        pdpte.page_level_cache_disable = 0; // 允许缓存
        pdpte.accessed = 0; // 清零访问位
        pdpte.execute_disable = 0; // 允许执行
        __stosq((unsigned __int64*)&pt.phys_pdpt[0], pdpte.flags, HOST_PHYSICAL_MEMORY_PD_COUNT); // 批量初始化 PDPTE

        for (uint64_t i = 0; i < HOST_PHYSICAL_MEMORY_PD_COUNT; ++i)
        {
            pt.phys_pdpt[i].page_frame_number = GET_PFN(MmGetPhysicalAddress(&pt.phys_pds[i]).QuadPart); // 绑定 PD 的物理页帧
        }

        pde_2mb_64 pde = { 0 }; // PDE 模板
        pde.present = 1; // 有效
        pde.write = 1; // 可写
        pde.supervisor = 0; // 内核访问
        pde.page_level_write_through = 0; // 禁止写穿
        pde.page_level_cache_disable = 0; // 允许缓存
        pde.accessed = 0; // 清零访问位
        pde.dirty = 0; // 清零脏位
        pde.large_page = 1;
        pde.global = 0;
        pde.pat = 0;
        pde.execute_disable = 0; // 允许执行
        __stosq((unsigned __int64*)&pt.phys_pds[0], pde.flags, 512 * 512); // 批量初始化 PDE

        for (uint64_t i = 0; i < HOST_PHYSICAL_MEMORY_PD_COUNT; ++i)
        {
            for (uint64_t j = 0; j < 512; ++j)
            {
                pt.phys_pds[i][j].page_frame_number = (i << 9) + j; // 填充 PFN，形成线性映射
            }
        }
    }

    void cleanup_host_page_tables()
    {
        free_host_dynamic_page_table_pages(ghv.host_page_tables); // 释放关键页补映射使用的动态页表页。
        RtlSecureZeroMemory(&ghv.host_page_tables, sizeof(ghv.host_page_tables));
    }

    bool prepare_host_page_tables()
    {
        if (ghv.system_cr3.flags == 0 ||
            ghv.system_cr3.address_of_page_directory == 0)
        {
            outDebug("宿主页表的 system_cr3 无效。\n");
            return false;
        }

        auto& pt = ghv.host_page_tables; // 获取全局宿主页表对象
        free_host_dynamic_page_table_pages(pt); // 旧逻辑：重建前直接memset会丢失动态页表页指针；先释放避免失败重试泄漏。
        memset(&pt, 0, sizeof(pt)); // 清零整个页表，避免保留位导致 #GP

        map_physical_memory(pt); // 构建物理直映窗口，供 VMX root 使用

        //先获得kernel system进程的cr3的pml4的物理地址
        PHYSICAL_ADDRESS pml4_address; // 物理地址结构
        pml4_address.QuadPart = ghv.system_cr3.address_of_page_directory << 12; // CR3 PFN 左移得到 PML4 物理地址

        // kernel PML4 address
        // 由kernel pml4物理地址得到虚拟线性地址
        // 因为memcpy这些函数是操作虚拟地址的
        auto const guest_pml4 = static_cast<pml4e_64*>(MmGetVirtualForPhysical(pml4_address)); // 映射到 HVA，便于 memcpy
        if (guest_pml4 == nullptr)
        {
            outDebug("MmGetVirtualForPhysical 返回空的 guest PML4。\n");
            return false;
        }

        outDebug("guest_pml4 地址：%p\n", guest_pml4); // 调试输出：确认映射有效

        // copy the top half of the System pml4 (a.k.a. the kernel address space)
        // 复制system pml4 的后半部分（又称内核地址空间）
        // 将256左移39位得到0x800000000000
        //outDebug("&guest_pml4[256]: %p\n", &guest_pml4[256]);
        memcpy(&pt.pml4[256], &guest_pml4[256], sizeof(pml4e_64) * 256); // 复制内核半区映射，保证宿主能访问内核地址
        bool const basic_check_ok =
            validate_host_address("HostPML4", &pt.pml4) &&
            validate_host_address("物理直映基址", host_physical_memory_base);
        if (!basic_check_ok)
        {
            outFile("[失败][Host页表] 基于dwm.exe构建的独占Host页表基础映射不完整，停止使用该HOST_CR3");
            cleanup_host_page_tables();
            return false;
        }

        return true;
    }

    bool ensure_host_page_tables_for_vcpu(__vcpu const* vcpu)
    {
        if (vcpu == nullptr)
        {
            outFile("[失败][Host页表覆盖] VCPU为空，无法建立完整VM-exit执行环境");
            return false;
        }

        bool ok = true;
        ok = ensure_host_virtual_range("VT驱动镜像代码/数据", g_vt_driver_image_base, g_vt_driver_image_size) && ok;
        const void* host_kpcr = reinterpret_cast<const void*>(__readmsr(IA32_GS_BASE));
        ok = ensure_host_virtual_range("当前CPU KPCR", host_kpcr, PAGE_SIZE) && ok;
        ok = ensure_host_virtual_range("VCPU完整结构", vcpu, sizeof(*vcpu)) && ok;
        ok = ensure_host_virtual_range("IO Bitmap A", vcpu->vcpu_bitmaps.io_bitmap_a, PAGE_SIZE) && ok;
        ok = ensure_host_virtual_range("IO Bitmap B", vcpu->vcpu_bitmaps.io_bitmap_b, PAGE_SIZE) && ok;

        if (vcpu->ept_state != nullptr)
        {
            ok = ensure_host_virtual_range("EPT状态结构", vcpu->ept_state, sizeof(*vcpu->ept_state)) && ok;
            ok = ensure_host_virtual_range("EPTP结构", vcpu->ept_state->ept_pointer, sizeof(*vcpu->ept_state->ept_pointer)) && ok;
            if (vcpu->ept_state->ept_page_table != nullptr)
            {
                ok = ensure_host_virtual_range("EPT主页表结构", vcpu->ept_state->ept_page_table, sizeof(*vcpu->ept_state->ept_page_table)) && ok;
            }
            if (vcpu->ept_state->pml2_pages != nullptr && vcpu->ept_state->mapped_pdpte_count != 0)
            {
                ok = ensure_host_virtual_range("EPT PML2指针表",
                    vcpu->ept_state->pml2_pages,
                    static_cast<unsigned __int64>(vcpu->ept_state->mapped_pdpte_count) * sizeof(vcpu->ept_state->pml2_pages[0])) && ok;
            }
            if (vcpu->ept_state->extended_pml3_pages != nullptr && vcpu->ept_state->mapped_pml4_count > 1)
            {
                ok = ensure_host_virtual_range("EPT扩展PML3指针表",
                    vcpu->ept_state->extended_pml3_pages,
                    static_cast<unsigned __int64>(vcpu->ept_state->mapped_pml4_count - 1) * sizeof(vcpu->ept_state->extended_pml3_pages[0])) && ok;
            }
            if (vcpu->ept_state->pml2_pages != nullptr && vcpu->ept_state->mapped_pdpte_count != 0)
            {
                for (unsigned __int32 i = 0; i < vcpu->ept_state->mapped_pdpte_count; ++i)
                {
                    if (vcpu->ept_state->pml2_pages[i] != nullptr)
                    {
                        ok = ensure_host_virtual_range("EPT PD页",vcpu->ept_state->pml2_pages[i],PAGE_SIZE) && ok;
                    }
                }
            }
            if (vcpu->ept_state->extended_pml3_pages != nullptr && vcpu->ept_state->mapped_pml4_count > 1)
            {
                for (unsigned __int32 i = 0; i < vcpu->ept_state->mapped_pml4_count - 1; ++i)
                {
                    if (vcpu->ept_state->extended_pml3_pages[i] != nullptr)
                    {
                        ok = ensure_host_virtual_range("EPT扩展PDPT页",
                            vcpu->ept_state->extended_pml3_pages[i],
                            PAGE_SIZE) && ok;
                    }
                }
            }
            if (vcpu->ept_state->physical_memory_ranges != nullptr && vcpu->ept_state->physical_memory_range_count != 0)
            {
                ok = ensure_host_virtual_range("EPT物理内存范围缓存",
                    vcpu->ept_state->physical_memory_ranges,
                    static_cast<unsigned __int64>(vcpu->ept_state->physical_memory_range_count) * sizeof(vcpu->ept_state->physical_memory_ranges[0])) && ok;
            }
        }
        else
        {
            outFile("[失败][Host页表覆盖] CPU[%u] EPT状态为空，VM-exit无法安全访问EPT状态",
                vcpu->logical_processor_index);
            ok = false;
        }

        if (!ok)
        {
            outFile("[失败][Host页表覆盖] CPU[%u] Host关键页补映射不完整，禁止进入VMLAUNCH",
                vcpu->logical_processor_index);
            return false;
        }

        outFile("[成功][Host页表覆盖] CPU[%u] Host关键代码/数据页覆盖完成",
            vcpu->logical_processor_index);
        return true;
    }

    bool validate_host_page_tables_for_vcpu(__vcpu const* vcpu)
    {
        if (vcpu == nullptr)
        {
            outFile("[失败][Host页表自检] VCPU为空");
            return false;
        }

        bool ok = true;
        ok = validate_host_address("vm_exit入口", reinterpret_cast<void const*>(vm_exit)) && ok;
        ok = validate_host_address("当前CPU KPCR", reinterpret_cast<void const*>(__readmsr(IA32_GS_BASE))) && ok;
        ok = validate_host_address("VCPU结构", vcpu) && ok;
        ok = validate_host_address("Host栈底", &vcpu->host_stack[0]) && ok;
        ok = validate_host_address("Host栈顶", &vcpu->host_stack[VMM_STACK_SIZE - 1]) && ok;
        ok = validate_host_address("Host GDT", &vcpu->host_gdt[0]) && ok;
        ok = validate_host_address("Host IDT", &vcpu->host_idt[0]) && ok;
        ok = validate_host_address("Host TSS", &vcpu->host_tss) && ok;
        ok = validate_host_address("MSR Bitmap", &vcpu->msr_bitmap) && ok;
        if (!ok)
        {
            outFile("[失败][Host页表自检] CPU[%u] VM-exit宿主路径存在不可翻译地址，禁止进入VMLAUNCH",
                vcpu->logical_processor_index);
            return false;
        }

        outFile("[成功][Host页表自检] CPU[%u] VM-exit宿主路径关键地址全部可翻译",
            vcpu->logical_processor_index);
        return true;
    }
}
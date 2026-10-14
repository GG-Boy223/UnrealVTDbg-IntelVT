#include "GlobalsInternal.h"

namespace hv
{
    namespace
    {
        __vcpu* current_vcpu_from_fs_if_valid()
        {
            if (g_vmm_context.vcpu == nullptr || g_vmm_context.processor_count == 0)
            {
                return nullptr;
            }

            auto const begin = reinterpret_cast<unsigned __int64>(g_vmm_context.vcpu);
            auto const size = static_cast<unsigned __int64>(sizeof(__vcpu)) * g_vmm_context.processor_count;
            auto const candidate = _readfsbase_u64();
            if (candidate < begin || candidate - begin >= size ||
                (candidate - begin) % sizeof(__vcpu) != 0)
            {
                return nullptr;
            }

            return reinterpret_cast<__vcpu*>(candidate);
        }
    }

    bool get_physical_mapping_va(uint64_t physical_address, void** virtual_address)
    {
        if (virtual_address == nullptr)
        {
            return false;
        }

        // VMX root 使用独立的 Host PML4，只保证复制了内核半区和大页映射，
        // 不保证 Windows MmGetVirtualForPhysical 返回的具体内核 VA 一定被克隆。
        // RAM 地址优先走本项目自己构建、VMX root 必然可见的物理直映窗口；
        // 超出该窗口的高地址 RAM 再退回 Windows 映射。
        __vcpu* const current_vcpu = current_vcpu_from_fs_if_valid();
        if (current_vcpu != nullptr &&
            current_vcpu->ept_state != nullptr &&
            current_vcpu->ept_state->physical_memory_ranges != nullptr &&
            current_vcpu->ept_state->physical_memory_range_count != 0)
        {
            if (!ept::is_physical_range_ram(*current_vcpu->ept_state, physical_address, 1))
            {
                *virtual_address = nullptr;
                return false;
            }

            constexpr unsigned __int64 host_direct_map_limit =
                static_cast<unsigned __int64>(HOST_PHYSICAL_MEMORY_PD_COUNT) << 30;
            if (physical_address < host_direct_map_limit)
            {
                *virtual_address = host_physical_memory_base + physical_address;
                return true;
            }
        }

        PHYSICAL_ADDRESS address;
        address.QuadPart = physical_address & ~0xfffull;
        uint8_t* mapped = static_cast<uint8_t*>(MmGetVirtualForPhysical(address));
        if (mapped == nullptr)
        {
            *virtual_address = nullptr;
            return false;
        }

        uint8_t* const result = mapped + (physical_address & 0xfffull);

        // VM-exit 路径可能高于 DISPATCH_LEVEL，不能调用 MmIsAddressValid。
        // 来宾页表页必须来自缓存的 RAM 范围；先用该范围拒绝空洞 PFN，
        // 再返回宿主物理映射，避免对非分页区空洞解引用触发 0x50。
        if (current_vcpu != nullptr &&
            current_vcpu->ept_state != nullptr &&
            current_vcpu->ept_state->physical_memory_ranges != nullptr &&
            current_vcpu->ept_state->physical_memory_range_count != 0)
        {
            if (!ept::is_physical_range_ram(*current_vcpu->ept_state, physical_address, 1))
            {
                *virtual_address = nullptr;
                return false;
            }

            *virtual_address = result;
            return true;
        }

        // 非 VM-exit 上下文（初始化、IOCTL 等）最多在 DISPATCH_LEVEL，
        // 使用 Windows 提供的地址有效性检查再解引用。
        if (KeGetCurrentIrql() <= DISPATCH_LEVEL && !MmIsAddressValid(result))
        {
            *virtual_address = nullptr;
            return false;
        }

        if (KeGetCurrentIrql() > DISPATCH_LEVEL)
        {
            *virtual_address = nullptr;
            return false;
        }

        *virtual_address = result;
        return true;
    }

    namespace
    {
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
            outFile("[失败][Host页表] 基于System进程构建的独占Host页表基础映射不完整，停止使用该HOST_CR3");
            cleanup_host_page_tables();
            return false;
        }

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

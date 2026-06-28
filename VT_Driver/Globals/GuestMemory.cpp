#include "GlobalsInternal.h"

namespace hv
{
    uint64_t gva2gpa(cr3 const guest_cr3, void* const gva, size_t* const offset_to_next_page)
    {
        if (offset_to_next_page)
        {
            *offset_to_next_page = 0; // 初始化为 0，避免调用方读取未定义值
        }

        pml4_virtual_address const vaddr = { gva }; // 解析虚拟地址为 PML4/PDPT/PD/PT/Offset
        // guest PML4
        // 由于我们将所有的物理地址映射在了host pt.pml4[255]开始的地方
        // 故我们需要控制GPA pml4_idx从host pml4[255]处开始
        // 从host pt.pml4[255]处开始
        auto const pml4 = reinterpret_cast<pml4e_64*>(host_physical_memory_base + (guest_cr3.address_of_page_directory << 12)); // 通过 CR3 取 PML4
        auto const pml4e = pml4[vaddr.pml4_idx]; // 获取 PML4E

        //判断该页是否存在
        //当P=1指示表或物理页面已加载到物理内存中
        if (!pml4e.present)
        {
            return 0;
        }

        // guest PDPT
        // 因为vm里的所以地址，对于host来说都是虚拟地址
        // 所以我们在host里仍然是将gpa的地址当做线性地址来解析
        // 我们仍需要将线性地址的pml4_idx从pml4[255]处开始
        auto const pdpt = reinterpret_cast<pdpte_64*>(host_physical_memory_base + (pml4e.page_frame_number << 12)); // 通过 PML4E 定位 PDPT
        auto const pdpte = pdpt[vaddr.pdpt_idx]; // 获取 PDPTE

        if (!pdpte.present)
        {
            return 0;
        }

        if (pdpte.large_page)
        {
            pdpte_1gb_64 pdpte_1gb; // 1GB 大页结构
            pdpte_1gb.flags = pdpte.flags; // 复制标志位

            auto const offset = (vaddr.pd_idx << 21) + (vaddr.pt_idx << 12) + vaddr.offset; // 计算 1GB 页内偏移

            if (offset_to_next_page)
            {
                *offset_to_next_page = 0x40000000 - offset; // 返回到下一个 1GB 边界的剩余字节
            }

            return (pdpte_1gb.page_frame_number << 30) + offset; // 返回 GPA
        }

        // guest PD
        auto const pd = reinterpret_cast<pde_64*>(host_physical_memory_base + (pdpte.page_frame_number << 12)); // 通过 PDPTE 定位 PD
        auto const pde = pd[vaddr.pd_idx]; // 获取 PDE

        if (!pde.present)
        {
            return 0;
        }

        if (pde.large_page)
        {
            pde_2mb_64 pde_2mb; // 2MB 大页结构
            pde_2mb.flags = pde.flags; // 复制标志位

            auto const offset = (vaddr.pt_idx << 12) + vaddr.offset; // 计算 2MB 页内偏移

            if (offset_to_next_page)
            {
                *offset_to_next_page = 0x200000 - offset; // 返回到下一个 2MB 边界的剩余字节
            }

            return (pde_2mb.page_frame_number << 21) + offset; // 返回 GPA
        }

        // guest PT
        auto const pt = reinterpret_cast<pte_64*>(host_physical_memory_base + (pde.page_frame_number << 12)); // 通过 PDE 定位 PT
        auto const pte = pt[vaddr.pt_idx]; // 获取 PTE

        if (!pte.present)
        {
            return 0;
        }

        if (offset_to_next_page)
        {
            *offset_to_next_page = 0x1000 - vaddr.offset; // 返回到下一个 4KB 边界的剩余字节
        }

        //(pte.page_frame_number << 12) 4KB物理页的起始地址 + offset则得到具体的物理地址
        return (pte.page_frame_number << 12) + vaddr.offset; // 返回 GPA
    }

    void* gva2hva(cr3 const guest_cr3, void* const gva, size_t* const offset_to_next_page)
    {
        auto const gpa = gva2gpa(guest_cr3, gva, offset_to_next_page); // 先转换 GVA->GPA
        if (!gpa)
        {
            return nullptr; // 失败返回空指针
        }

        return host_physical_memory_base + gpa; // 利用物理直映返回 HVA
    }

    void* gva2hva(void* const gva, size_t* const offset_to_next_page)
    {
        cr3 guest_cr3; // 临时 CR3
        guest_cr3.flags = vmread(GUEST_CR3); // 从 VMCS 读取当前来宾 CR3
        return gva2hva(guest_cr3, gva, offset_to_next_page); // 调用带 CR3 版本
    }

    //将GVA转换为GPA
    uint64_t get_physical_address(unsigned __int64 guest_cr3, _In_ PVOID BaseAddress)
    {
        if (!guest_cr3)
        {
            return NULL; // CR3 无效时无法转换
        }

        cr3 tmp_cr3; // 临时 CR3 结构
        tmp_cr3.flags = guest_cr3; // 赋值 CR3
        return gva2gpa(tmp_cr3, BaseAddress); // 转换地址
    }

    // attempt to read the memory at the specified guest virtual address from root-mode
    size_t read_guest_virtual_memory(cr3 const guest_cr3,
        void* const gva, void* const hva, size_t const size)
    {
        // the GVA that we're reading from
        auto const src = reinterpret_cast<uint8_t*>(gva); // 源 GVA 指针
        // the HVA that we're writing to
        // 这个是hva
        auto const dst = reinterpret_cast<uint8_t*>(hva); // 目标缓冲 HVA

        size_t bytes_read = 0;

        while (bytes_read < size)
        {
            size_t src_remaining = 0; // 当前页剩余可读字节数
            // translate the guest virtual address to a host virtual address
            // 将guest虚拟地址映射到host虚拟地址
            // 如果跨页curr_src将会指向下一个页
            auto const curr_src = gva2hva(guest_cr3, src + bytes_read, &src_remaining); // 当前页 HVA

            // paged out
            if (!curr_src)
            {
                return bytes_read;
            }

            // the maximum allowed size that we can read at once with the translated HVA
            auto const curr_size = min(size - bytes_read, src_remaining); // 本次读取字节数

            host_exception_info e = { 0 }; // 异常捕获结构
            memcpy_safe(e, dst + bytes_read, curr_src, curr_size); // 安全拷贝，防止访问无效导致崩溃

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
        size_t bytes_read = 0; // 已写入字节计数
        // 这个是gva
        auto const dst = reinterpret_cast<uint8_t*>(gva); // 目标 GVA
        // 这个是hva
        auto const src = reinterpret_cast<uint8_t*>(hva); // 源 HVA

        while (bytes_read < size)
        {
            size_t dst_remaining = 0; // 当前页剩余可写字节数
            // remaining返回页面的剩余字节数
            // 如果跨页curr_dst将会指向下一个页
            auto const curr_dst = gva2hva(guest_cr3, dst + bytes_read, &dst_remaining); // 当前页 HVA

            // this means that the target memory isn't paged in. there's nothing
            // we can do about that since we're not currently in that process's context.
            // 这意味着目标内存未被调入页。我们对此无能为力，因为我们目前不在该进程的上下文中。
            if (!curr_dst)
            {
                return bytes_read;
            }

            auto const curr_size = min(size - bytes_read, dst_remaining); // 本次写入字节数

            host_exception_info e = { 0 }; // 异常捕获结构
            memcpy_safe(e, curr_dst, src + bytes_read, curr_size); // 安全拷贝

            if (e.exception_occurred)
            {
                return bytes_read;
            }

            bytes_read += curr_size;
        }
        return bytes_read;
    }

    // attempt to read the memory at the specified guest virtual address from root-mode
    // 读取guest中当前进程的虚拟内存
    size_t read_guest_virtual_memory(void* const gva, void* const hva, size_t const size)
    {
        cr3 guest_cr3; // 临时 CR3
        guest_cr3.flags = vmread(GUEST_CR3); // 读取当前来宾 CR3
        return read_guest_virtual_memory(guest_cr3, gva, hva, size); // 转发到带 CR3 版本
    }

    // 写入guest中当前进程的虚拟内存
    size_t write_guest_virtual_memory(void* const gva, void* const hva, size_t const size)
    {
        cr3 guest_cr3; // 临时 CR3
        guest_cr3.flags = vmread(GUEST_CR3); // 读取当前来宾 CR3
        return write_guest_virtual_memory(guest_cr3, gva, hva, size); // 转发到带 CR3 版本
    }
}

#pragma once

#include "GlobalsVmmContextTypes.h"

namespace hv // Hypervisor 命名空间
{

    struct trap_frame // 宿主中断 trap frame（汇编中构造）
    {
        // TODO: SSE registers...

        // general-purpose registers
        union
        {
            uint64_t rax; // RAX
            uint32_t eax; // EAX
            uint16_t ax; // AX
            uint8_t  al; // AL
        };
        union
        {
            uint64_t rcx; // RCX
            uint32_t ecx; // ECX
            uint16_t cx; // CX
            uint8_t  cl; // CL
        };
        union
        {
            uint64_t rdx; // RDX
            uint32_t edx; // EDX
            uint16_t dx; // DX
            uint8_t  dl; // DL
        };
        union
        {
            uint64_t rbx; // RBX
            uint32_t ebx; // EBX
            uint16_t bx; // BX
            uint8_t  bl; // BL
        };
        union
        {
            uint64_t rbp; // RBP
            uint32_t ebp; // EBP
            uint16_t bp; // BP
            uint8_t  bpl; // BPL
        };
        union
        {
            uint64_t rsi; // RSI
            uint32_t esi; // ESI
            uint16_t si; // SI
            uint8_t  sil; // SIL
        };
        union
        {
            uint64_t rdi; // RDI
            uint32_t edi; // EDI
            uint16_t di; // DI
            uint8_t  dil; // DIL
        };
        union
        {
            uint64_t r8; // R8
            uint32_t r8d; // R8D
            uint16_t r8w; // R8W
            uint8_t  r8b; // R8B
        };
        union
        {
            uint64_t r9; // R9
            uint32_t r9d; // R9D
            uint16_t r9w; // R9W
            uint8_t  r9b; // R9B
        };
        union
        {
            uint64_t r10; // R10
            uint32_t r10d; // R10D
            uint16_t r10w; // R10W
            uint8_t  r10b; // R10B
        };
        union
        {
            uint64_t r11; // R11
            uint32_t r11d; // R11D
            uint16_t r11w; // R11W
            uint8_t  r11b; // R11B
        };
        union
        {
            uint64_t r12; // R12
            uint32_t r12d; // R12D
            uint16_t r12w; // R12W
            uint8_t  r12b; // R12B
        };
        union
        {
            uint64_t r13; // R13
            uint32_t r13d; // R13D
            uint16_t r13w; // R13W
            uint8_t  r13b; // R13B
        };
        union
        {
            uint64_t r14; // R14
            uint32_t r14d; // R14D
            uint16_t r14w; // R14W
            uint8_t  r14b; // R14B
        };
        union
        {
            uint64_t r15; // R15
            uint32_t r15d; // R15D
            uint16_t r15w; // R15W
            uint8_t  r15b; // R15B
        };

        // interrupt vector
        uint8_t vector; // 中断向量号

        // _MACHINE_FRAME
        uint64_t error; // 错误码
        uint64_t rip; // RIP
        uint64_t cs; // CS
        uint64_t rflags; // RFLAGS
        uint64_t rsp; // RSP
        uint64_t ss; // SS
    };

    // remember to update this value in interrupt-handlers.asm
    static_assert(sizeof(trap_frame) == (0x78 + 0x38)); // 确保与汇编构造一致

    // structure that gets filled out when a host exception occurs
    struct host_exception_info // 宿主异常信息
    {
        // whether an exception occurred or not
        // 是否发生异常
        bool exception_occurred; // 异常标志

        // interrupt vector
        uint64_t vector; // 异常向量号

        // error code
        uint64_t error; // 异常错误码
    };

    struct host_page_tables // 宿主页表结构（直接映射物理内存）
    {
        // array of PML4 entries that point to a PDPT
        alignas(0x1000) pml4e_64 pml4[512]; // PML4 表

        // PDPT for mapping physical memory
        alignas(0x1000) pdpte_64 phys_pdpt[512]; // PDPT 表

        // PDs for mapping physical memory
        alignas(0x1000) pde_2mb_64 phys_pds[HOST_PHYSICAL_MEMORY_PD_COUNT][512]; // PDE 表

        unsigned __int32 dynamic_pdpt_count; // Host关键虚拟页补映射：已克隆/新建的PDPT页数量
        unsigned __int32 dynamic_pd_count; // Host关键虚拟页补映射：已克隆/新建的PD页数量
        unsigned __int32 dynamic_pt_count; // Host关键虚拟页补映射：已克隆/新建的PT页数量
        void* dynamic_pdpts[HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT]; // 按PML4索引保存的动态PDPT页
        unsigned __int16 dynamic_pdpt_pml4_index[HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT]; // dynamic_pdpts对应的PML4索引
        void* dynamic_pds[HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT]; // 按PML4/PDPT索引保存的动态PD页
        unsigned __int16 dynamic_pd_pml4_index[HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT]; // dynamic_pds对应的PML4索引
        unsigned __int16 dynamic_pd_pdpt_index[HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT]; // dynamic_pds对应的PDPT索引
        void* dynamic_pts[HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT]; // 按PML4/PDPT/PD索引保存的动态PT页
        unsigned __int16 dynamic_pt_pml4_index[HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT]; // dynamic_pts对应的PML4索引
        unsigned __int16 dynamic_pt_pdpt_index[HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT]; // dynamic_pts对应的PDPT索引
        unsigned __int16 dynamic_pt_pd_index[HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT]; // dynamic_pts对应的PD索引
    };

    struct hypervisor // Hypervisor 全局对象
    {
        // host page tables that are shared between vcpus
        alignas(0x1000) host_page_tables host_page_tables; // 宿主页表

        // pointer to the System process
        uint8_t* system_eprocess; // System 进程 EPROCESS

        // kernel CR3 value of the System process
        // System进程的内核 CR3 值
        cr3 system_cr3; // System CR3

        // windows specific offsets D:
        uint64_t kprocess_directory_table_base_offset; // KPROCESS.DirectoryTableBase 偏移
        uint64_t eprocess_unique_process_id_offset; // EPROCESS.UniqueProcessId 偏移
        uint64_t eprocess_image_file_name; // EPROCESS.ImageFileName 偏移
        uint64_t kpcr_pcrb_offset; // KPCR.Prcb 偏移
        uint64_t kprcb_current_thread_offset; // KPRCB.CurrentThread 偏移
        uint64_t kthread_apc_state_offset; // KTHREAD.ApcState 偏移
        uint64_t kapc_state_process_offset; // KAPC_STATE.Process 偏移
    };

    // global instance of the hypervisor
    extern hypervisor ghv; // Hypervisor 全局实例

    // selectors for the host GDT
    extern segment_selector host_cs_selector; // 宿主 CS 选择子
    extern segment_selector host_tr_selector; // 宿主 TR 选择子
    extern uint64_t host_physical_memory_pml4_idx; // 宿主物理内存映射 PML4 索引
    extern uint8_t* host_physical_memory_base; // 宿主物理内存映射基址
}

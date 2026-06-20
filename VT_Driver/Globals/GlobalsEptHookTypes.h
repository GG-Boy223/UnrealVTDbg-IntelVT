#pragma once

#include "GlobalsEptPageTypes.h"

struct __ept_hooked_function_info // 单个函数 hook 记录
{
    //
    // Linked list entires for each function hook.
    //
    LIST_ENTRY hooked_function_list; // 函数 hook 链表节点

    //
    // Pointer to page with our hooked functions
    // 伪造页内容
    // 指向4KB物理页
    //
    unsigned __int8* fake_page_contents; // 伪页内容指针

    //
    // Size of hook
    //
    unsigned __int64 hook_size; // hook 覆盖长度

    //欲设置断点的物理地址
    unsigned __int64 breakpoint_address; // 断点地址（物理）

    //用途
    int Options; // hook 选项/用途

    unsigned __int64 LoopUserMode; // 用户态回环地址

    unsigned __int64 LoopKernelMode; // 内核态回环地址

    BrokenThreadEntry CurrentBrokenThread; // 触发断点线程状态

    //
    // Virtual address of function
    // 函数的虚拟地址
    //
    void* virtual_address; // 原始函数虚拟地址

    //事件的处理函数
    void* handler_function; // hook 的处理函数

    //
    // Address to first trampoline used to call original function
    // 用于调用原始函数的
    //
    unsigned __int8* first_trampoline_address; // trampoline 地址（回原函数）

    //
    // Address of code cave which is used to jmp to our hooked function
    //
    void* second_trampoline_address; // 代码洞 trampoline
};

//hook页 描述符
struct __ept_hooked_page_info // 被 hook 的页面信息
{
    //
    // Page with our hooked functions
    // 伪造页的内容存放hook代码
    // 4KB物理页
    //
    DECLSPEC_ALIGN(PAGE_SIZE) unsigned __int8 fake_page_contents[PAGE_SIZE]; // 伪页内容缓冲

    //
    // Linked list entires for each page hook.
    // 记录被hook的页面
    //
    LIST_ENTRY hooked_page_list; // 挂钩页链表节点

    //
    // Linked list entries for each function hook
    // 记录被hook的函数
    //
    LIST_ENTRY hooked_functions_list; // 挂钩函数链表

    //
    // The base address of the page. Used to find this structure in the list of page hooks
    // 源函数的页帧号
    //
    unsigned __int64 pfn_of_hooked_page; // 被 hook 的页帧号

    //
    // The base address of the page with fake contents. Used to swap page with fake contents
    // 伪造页的页帧号
    //
    unsigned __int64 pfn_of_fake_page_contents; // 伪页页帧号

    //
    // The page entry in the page tables that this page is targetting.
    //
    __ept_pte* entry_address; // 指向 EPT PTE 的地址

    //
    // The original page entry
    // 原页
    //
    __ept_pte original_entry; // 原始 PTE

    //
    // The changed page entry
    // 伪造页
    //
    __ept_pte changed_entry; // 替换后的 PTE

    __ept_dynamic_split* owner_split; // 新增：该页面所在 split 的归属指针，用于撤销时引用计数-1并尝试合并

    int Options; // hook 选项

    int ID; // watch ID

    bool isBp; // 是否为断点触发

    bool isInt3; // 是否为 int3 断点

    int last_watch_id; // 最近一次命中的 watch ID，减少同页高频 VM-exit 全表扫描

    bool fake_page_synced; // fake page 是否已与 guest 原页完成整页同步

    bool fake_page_needs_full_sync; // 需要在下次 MTF 收尾时重新整页同步 fake page
};

union __ept_violation // EPT 违规信息字段
{
    unsigned __int64 all; // 전체 值
    struct
    {
        /**
         * [Bit 0] Set if the access causing the EPT violation was a data read.
         */
        unsigned __int64 read_access : 1; // 读访问导致

        /**
         * [Bit 1] Set if the access causing the EPT violation was a data write.
         */
        unsigned __int64 write_access : 1; // 写访问导致

        /**
         * [Bit 2] Set if the access causing the EPT violation was an instruction fetch.
         */
        unsigned __int64 execute_access : 1; // 指令取址导致

        /**
         * [Bit 3] The logical-AND of bit 0 in the EPT paging-structure entries used to translate the guest-physical address of the
         * access causing the EPT violation (indicates whether the guest-physical address was readable).
         */
        unsigned __int64 ept_readable : 1; // EPT 允许读（逻辑与）

        /**
         * [Bit 4] The logical-AND of bit 1 in the EPT paging-structure entries used to translate the guest-physical address of the
         * access causing the EPT violation (indicates whether the guest-physical address was writeable).
         */
        unsigned __int64 ept_writeable : 1; // EPT 允许写（逻辑与）

        /**
         * [Bit 5] The logical-AND of bit 2 in the EPT paging-structure entries used to translate the guest-physical address of the
         * access causing the EPT violation.
         * If the "mode-based execute control for EPT" VM-execution control is 0, this indicates whether the guest-physical address
         * was executable. If that control is 1, this indicates whether the guest-physical address was executable for
         * supervisor-mode linear addresses.
         */
        unsigned __int64 ept_executable : 1; // EPT 允许执行（逻辑与）

        /**
         * [Bit 6] If the "mode-based execute control" VM-execution control is 0, the value of this bit is undefined. If that
         * control is 1, this bit is the logical-AND of bit 10 in the EPT paging-structures entries used to translate the
         * guest-physical address of the access causing the EPT violation. In this case, it indicates whether the guest-physical
         * address was executable for user-mode linear addresses.
         */
        unsigned __int64 ept_executable_for_usermode : 1; // EPT 用户态可执行

        /**
         * [Bit 7] Set if the guest linear-address field is valid. The guest linear-address field is valid for all EPT violations
         * except those resulting from an attempt to load the guest PDPTEs as part of the execution of the MOV CR instruction.
         */
        unsigned __int64 valid_guest_linear_address : 1; // GLA 字段有效

        /**
         * [Bit 8] If bit 7 is 1:
         * - Set if the access causing the EPT violation is to a guest-physical address that is the translation of a linear
         * address.
         * - Clear if the access causing the EPT violation is to a paging-structure entry as part of a page walk or the update of
         * an accessed or dirty bit.
         * Reserved if bit 7 is 0 (cleared to 0).
         */
        unsigned __int64 caused_by_translation : 1; // 是否由页表翻译导致

        /**
         * [Bit 9] This bit is 0 if the linear address is a supervisor-mode linear address and 1 if it is a user-mode linear
         * address. Otherwise, this bit is undefined.
         *
         * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
         *          CR0.PG = 0, the translation of every linear address is a user-mode linear address and thus this bit will be 1.)
         */
        unsigned __int64 usermode_linear_address : 1; // 线性地址是否为用户态

        /**
         * [Bit 10] This bit is 0 if paging translates the linear address to a read-only page and 1 if it translates to a
         * read/write page. Otherwise, this bit is undefined
         *
         * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
         *          CR0.PG = 0, every linear address is read/write and thus this bit will be 1.)
         */
        unsigned __int64 readable_writable_page : 1; // 是否为可读写页

        /**
         * [Bit 11] This bit is 0 if paging translates the linear address to an executable page and 1 if it translates to an
         * execute-disable page. Otherwise, this bit is undefined.
         *
         * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
         *          CR0.PG = 0, CR4.PAE = 0, or MSR_IA32_EFER.NXE = 0, every linear address is executable and thus this bit will be 0.)
         */
        unsigned __int64 execute_disable_page : 1; // 是否为不可执行页

        /**
         * [Bit 12] NMI unblocking due to IRET.
         */
        unsigned __int64 nmi_unblocking : 1; // IRET 导致的 NMI 解除
        unsigned __int64 reserved1 : 51; // 保留位
    };
};

struct __ept_state // EPT 状态（每 VCPU 一份）
{
    LIST_ENTRY hooked_page_list;  //hook页列表   被hook的页面会被加入到该列表
    __eptp* ept_pointer; // EPTP 指针
    __vmm_ept_page_table* ept_page_table;  //ept页表
    __ept_pde** pml2_pages; // flat PDPT 索引到 PD 页的映射，支持超过 512GB 的 GPA
    __ept_pdpte_page** extended_pml3_pages; // PML4[1..] 对应的动态 PDPT 页
    unsigned __int32 mapped_pml4_count; // 已填充的 PML4 条目数
    unsigned __int32 mapped_pdpte_count; // 已填充的 flat PDPTE/PD 页数量
    __ept_hooked_page_info* page_to_change; // 当前需要切换的页（用于 MTF）
    bool mtf_pending; // page_to_change 是否属于当前活跃 MTF 窗口
    bool mtf_restore_interruptibility; // MTF 收尾时是否需要恢复 guest interruptibility
    unsigned __int64 mtf_saved_interruptibility; // 临时 blocking 前的 guest interruptibility
};

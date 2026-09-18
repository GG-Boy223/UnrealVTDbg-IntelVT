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

    volatile long removing; // 删除事务已开始,禁止热路径建立新引用

    volatile long hot_reference_count; // 当前 VM-exit 热路径持有的引用
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

    volatile long mtf_reference_count; // 当前持有该页的 VCPU pending-MTF 引用数

    volatile long removing; // 删除事务已开始，禁止建立新的 pending-MTF 引用

    volatile long hot_reference_count; // 当前 VM-exit 热路径持有的引用

    volatile long functions_lock; // hooked_functions_list 并发保护锁
};

union __ept_violation // EPT 违规信息字段
{
    unsigned __int64 all; // 전체 值
    struct
    {
        /**
         * [Bit 0] Set if the access causing the EPT violation was a data read.
         */
        // /**
        // * [Bit 0] Set if the access causing the EPT violation was a data read.
        // */
        unsigned __int64 read_access : 1; // 读访问导致

        /**
         * [Bit 1] Set if the access causing the EPT violation was a data write.
         */
        // /**
        // * [Bit 1] Set if the access causing the EPT violation was a data write.
        // */
        unsigned __int64 write_access : 1; // 写访问导致

        /**
         * [Bit 2] Set if the access causing the EPT violation was an instruction fetch.
         */
        // /**
        // * [Bit 2] Set if the access causing the EPT violation was an instruction fetch.
        // */
        unsigned __int64 execute_access : 1; // 指令取址导致

        /**
         * [Bit 3] The logical-AND of bit 0 in the EPT paging-structure entries used to translate the guest-physical address of the
         * access causing the EPT violation (indicates whether the guest-physical address was readable).
         */
        // /**
        // * [Bit 3] The logical-AND of bit 0 in the EPT paging-structure entries used to translate the guest-physical address of the
        // * access causing the EPT violation (indicates whether the guest-physical address was readable).
        // */
        unsigned __int64 ept_readable : 1; // EPT 允许读（逻辑与）

        /**
         * [Bit 4] The logical-AND of bit 1 in the EPT paging-structure entries used to translate the guest-physical address of the
         * access causing the EPT violation (indicates whether the guest-physical address was writeable).
         */
        // /**
        // * [Bit 4] The logical-AND of bit 1 in the EPT paging-structure entries used to translate the guest-physical address of the
        // * access causing the EPT violation (indicates whether the guest-physical address was writeable).
        // */
        unsigned __int64 ept_writeable : 1; // EPT 允许写（逻辑与）

        /**
         * [Bit 5] The logical-AND of bit 2 in the EPT paging-structure entries used to translate the guest-physical address of the
         * access causing the EPT violation.
         * If the "mode-based execute control for EPT" VM-execution control is 0, this indicates whether the guest-physical address
         * was executable. If that control is 1, this indicates whether the guest-physical address was executable for
         * supervisor-mode linear addresses.
         */
        // /**
        // * [Bit 5] The logical-AND of bit 2 in the EPT paging-structure entries used to translate the guest-physical address of the
        // * access causing the EPT violation.
        // * If the "mode-based execute control for EPT" VM-execution control is 0, this indicates whether the guest-physical address
        // * was executable. If that control is 1, this indicates whether the guest-physical address was executable for
        // * supervisor-mode linear addresses.
        // */
        unsigned __int64 ept_executable : 1; // EPT 允许执行（逻辑与）

        /**
         * [Bit 6] If the "mode-based execute control" VM-execution control is 0, the value of this bit is undefined. If that
         * control is 1, this bit is the logical-AND of bit 10 in the EPT paging-structures entries used to translate the
         * guest-physical address of the access causing the EPT violation. In this case, it indicates whether the guest-physical
         * address was executable for user-mode linear addresses.
         */
        // /**
        // * [Bit 6] If the "mode-based execute control" VM-execution control is 0, the value of this bit is undefined. If that
        // * control is 1, this bit is the logical-AND of bit 10 in the EPT paging-structures entries used to translate the
        // * guest-physical address of the access causing the EPT violation. In this case, it indicates whether the guest-physical
        // * address was executable for user-mode linear addresses.
        // */
        unsigned __int64 ept_executable_for_usermode : 1; // EPT 用户态可执行

        /**
         * [Bit 7] Set if the guest linear-address field is valid. The guest linear-address field is valid for all EPT violations
         * except those resulting from an attempt to load the guest PDPTEs as part of the execution of the MOV CR instruction.
         */
        // /**
        // * [Bit 7] Set if the guest linear-address field is valid. The guest linear-address field is valid for all EPT violations
        // * except those resulting from an attempt to load the guest PDPTEs as part of the execution of the MOV CR instruction.
        // */
        unsigned __int64 valid_guest_linear_address : 1; // GLA 字段有效

        /**
         * [Bit 8] If bit 7 is 1:
         * - Set if the access causing the EPT violation is to a guest-physical address that is the translation of a linear
         * address.
         * - Clear if the access causing the EPT violation is to a paging-structure entry as part of a page walk or the update of
         * an accessed or dirty bit.
         * Reserved if bit 7 is 0 (cleared to 0).
         */
        // /**
        // * [Bit 8] If bit 7 is 1:
        // * - Set if the access causing the EPT violation is to a guest-physical address that is the translation of a linear
        // * address.
        // * - Clear if the access causing the EPT violation is to a paging-structure entry as part of a page walk or the update of
        // * an accessed or dirty bit.
        // * Reserved if bit 7 is 0 (cleared to 0).
        // */
        unsigned __int64 caused_by_translation : 1; // 是否由页表翻译导致

        /**
         * [Bit 9] This bit is 0 if the linear address is a supervisor-mode linear address and 1 if it is a user-mode linear
         * address. Otherwise, this bit is undefined.
         *
         * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
         *          CR0.PG = 0, the translation of every linear address is a user-mode linear address and thus this bit will be 1.)
         */
        // /**
        // * [Bit 9] This bit is 0 if the linear address is a supervisor-mode linear address and 1 if it is a user-mode linear
        // * address. Otherwise, this bit is undefined.
        // *
        // * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
        // *          CR0.PG = 0, the translation of every linear address is a user-mode linear address and thus this bit will be 1.)
        // */
        unsigned __int64 usermode_linear_address : 1; // 线性地址是否为用户态

        /**
         * [Bit 10] This bit is 0 if paging translates the linear address to a read-only page and 1 if it translates to a
         * read/write page. Otherwise, this bit is undefined
         *
         * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
         *          CR0.PG = 0, every linear address is read/write and thus this bit will be 1.)
         */
        // /**
        // * [Bit 10] This bit is 0 if paging translates the linear address to a read-only page and 1 if it translates to a
        // * read/write page. Otherwise, this bit is undefined
        // *
        // * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
        // *          CR0.PG = 0, every linear address is read/write and thus this bit will be 1.)
        // */
        unsigned __int64 readable_writable_page : 1; // 是否为可读写页

        /**
         * [Bit 11] This bit is 0 if paging translates the linear address to an executable page and 1 if it translates to an
         * execute-disable page. Otherwise, this bit is undefined.
         *
         * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
         *          CR0.PG = 0, CR4.PAE = 0, or MSR_IA32_EFER.NXE = 0, every linear address is executable and thus this bit will be 0.)
         */
        // /**
        // * [Bit 11] This bit is 0 if paging translates the linear address to an executable page and 1 if it translates to an
        // * execute-disable page. Otherwise, this bit is undefined.
        // *
        // * @remarks If bit 7 is 1, bit 8 is 1, and the processor supports advanced VM-exit information for EPT violations. (If
        // *          CR0.PG = 0, CR4.PAE = 0, or MSR_IA32_EFER.NXE = 0, every linear address is executable and thus this bit will be 0.)
        // */
        unsigned __int64 execute_disable_page : 1; // 是否为不可执行页

        /**
         * [Bit 12] NMI unblocking due to IRET.
         */
        // /**
        // * [Bit 12] NMI unblocking due to IRET.
        // */
        unsigned __int64 nmi_unblocking : 1; // IRET 导致的 NMI 解除
        unsigned __int64 reserved1 : 51; // 保留位
    };
};

// 非 RAM（MMIO / 地址空洞）区域按需映射使用的 PD 页池条目。
// PD 页在 EPT 构建期一次性分配，并在这里缓存物理页帧号：
// EPT violation 处理运行在来宾 IRQL 上，只能做纯内存写入，
// 不允许再调用 MmGetPhysicalAddress 或任何内核分配接口。
struct __ept_mmio_pd_page
{
    __ept_pde* entries; // 512 个 2MB EPT 条目（页对齐）
    unsigned __int64 page_frame_number; // PD 页自身的物理页帧号（构建期缓存）
};

struct __ept_state // EPT 状态（每 VCPU 一份）
{
    LIST_ENTRY hooked_page_list;  //hook页列表   被hook的页面会被加入到该列表
    volatile long hooked_page_list_lock; // hooked_page_list 并发访问锁
    __eptp* ept_pointer; // EPTP 指针
    __vmm_ept_page_table* ept_page_table;  //ept页表
    __ept_pde** pml2_pages; // flat PDPT 索引到 PD 页的映射，支持超过 512GB 的 GPA
    __ept_pdpte_page** extended_pml3_pages; // PML4[1..] 对应的动态 PDPT 页
    struct __ept_physical_memory_range
    {
        unsigned __int64 base; // RAM 起始物理地址，来自 MmGetPhysicalMemoryRanges。
        unsigned __int64 end; // RAM 结束物理地址（开区间），用于区分 RAM 与 MMIO 空洞。
    }*physical_memory_ranges; // 缓存 RAM 范围，VM-exit 路径不能再调用 Windows API 查询。
    unsigned __int32 physical_memory_range_count; // RAM 范围数量，0 表示保留旧 MTRR-only 行为。
    unsigned __int32 mapped_pml4_count; // 已填充的 PML4 条目数
    unsigned __int32 mapped_pdpte_count; // 已填充的 flat PDPTE/PD 页数量
    __ept_mmio_pd_page* mmio_pd_pool; // 非RAM区域按需映射的PD页池（仅CPU不支持1GB EPT大页时分配）
    unsigned __int32 mmio_pd_pool_capacity; // 池容量（页数）；初始化期间保存已成功分配的页数，供失败清理使用
    unsigned __int32 mmio_pd_pool_used; // 已取用页数，只由持有该EPT的VCPU在VM-exit路径递增
    bool ept_1gb_pages_supported; // 本CPU是否支持1GB EPT大页（构建期查询IA32_VMX_EPT_VPID_CAP[17]并缓存）
    unsigned __int64 mmio_mapped_region_count; // 运行期补映射过的非RAM区域数量（VM-exit内只累加，日志在安全上下文输出）
    unsigned __int64 last_mmio_mapped_guest_physical; // 最近一次补映射的GPA，用于安全上下文诊断输出
    unsigned __int64 mmio_mapping_failure_count; // 按需映射失败次数（超出覆盖范围/页表状态异常/PD池耗尽）
    __ept_hooked_page_info* page_to_change; // 当前需要切换的页（用于 MTF）
    bool mtf_pending; // page_to_change 是否属于当前活跃 MTF 窗口
    unsigned __int64 unhandled_ept_violation_count; // 同一 GPA 页帧上连续未匹配 EPT violation 的累计次数（达上限即致命错误）
    unsigned __int64 last_unhandled_ept_violation_guest_physical; // 最近一次未匹配 violation 的 GPA 页帧（低 12 位已清零，用于按页累计）
    unsigned __int64 last_unhandled_ept_violation_qualification; // 最近一次未匹配 violation 的 qualification（仅留作现场记录，不参与计数比较）
    unsigned __int64 last_unhandled_ept_violation_guest_rip; // 最近一次未匹配 violation 的 guest RIP（仅留作现场记录）
};

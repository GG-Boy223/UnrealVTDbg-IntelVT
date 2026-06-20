#pragma once

#include "GlobalsVmxMtrrTypes.h"

typedef struct _MDL_MAP // MDL 映射信息
{
    PMDL mdl; // MDL 对象
    PVOID MappedAddress; // 映射后的虚拟地址
    PVOID VirtualAddress; // 原始虚拟地址
}MDL_MAP, * PMDL_MAP; // MDL 映射结构与指针别名

union __eptp // EPTP 寄存器布局
{
    unsigned __int64 all; // 전체 值
    struct
    {
        unsigned __int64 memory_type : 3;  //2:0
        unsigned __int64 page_walk_length : 3;  //5:3
        unsigned __int64 dirty_and_aceess_enabled : 1;  //6
        unsigned __int64 reserved_0 : 5;  //11:7
        unsigned __int64 pml4_address : 36;  //47:12
        unsigned __int64 reserved_1 : 16;  //63:48
    };
};

// See Table 28-1.
union __ept_pml4e // EPT PML4E 条目
{
    unsigned __int64 all; // 전체 값
    struct
    {
        unsigned __int64 read : 1; // bit 0
        unsigned __int64 write : 1; // bit 1
        unsigned __int64 execute : 1; // bit 2
        unsigned __int64 reserved1 : 5; // bit 7:3 (Must be Zero)
        unsigned __int64 accessed : 1; // bit 8
        unsigned __int64 ignored1 : 1; // bit 9
        unsigned __int64 execute_for_usermode : 1; // bit 10
        unsigned __int64 ignored2 : 1; // bit 11
        unsigned __int64 page_frame_number : 36; // bit (N-1):12 or Page-Frame-Number
        unsigned __int64 reserved2 : 4; // bit 51:N
        unsigned __int64 ignored3 : 12; // bit 63:52
    };
};

// See Table 28-3
union __ept_pdpte // EPT PDPTE 条目
{
    unsigned __int64 all; // 전체 값
    struct
    {
        unsigned __int64 read : 1; // bit 0
        unsigned __int64 write : 1; // bit 1
        unsigned __int64 execute : 1; // bit 2
        unsigned __int64 reserved1 : 5; // bit 7:3 (Must be Zero)
        unsigned __int64 accessed : 1; // bit 8
        unsigned __int64 ignored1 : 1; // bit 9
        unsigned __int64 execute_for_usermode : 1; // bit 10
        unsigned __int64 ignored2 : 1; // bit 11
        unsigned __int64 page_frame_number : 36; // bit (N-1):12 or Page-Frame-Number
        unsigned __int64 reserved2 : 4; // bit 51:N
        unsigned __int64 ignored3 : 12; // bit 63:52
    };
};

// See Table 28-5
union __ept_pde // EPT PDE（支持大页/小页）
{
    unsigned __int64 all; // 전체 值
    struct
    {
        unsigned __int64 read : 1; // bit 0
        unsigned __int64 write : 1; // bit 1
        unsigned __int64 execute : 1; // bit 2
        unsigned __int64 reserved1 : 5; // bit 7:3 (Must be Zero)
        unsigned __int64 accessed : 1; // bit 8
        unsigned __int64 ignored1 : 1; // bit 9
        unsigned __int64 execute_for_usermode : 1; // bit 10
        unsigned __int64 ignored2 : 1; // bit 11
        unsigned __int64 page_frame_number : 36; // bit (N-1):12 or Page-Frame-Number
        unsigned __int64 reserved2 : 4; // bit 51:N
        unsigned __int64 ignored3 : 12; // bit 63:52
    }large_page; // 大页视图
    struct
    {
        unsigned __int64 read : 1; // 读权限
        unsigned __int64 write : 1; // 写权限
        unsigned __int64 execute : 1; // 执行权限
        unsigned __int64 memory_type : 3; // EPT 内存类型
        unsigned __int64 ignore_pat : 1; // 是否忽略 PAT
        unsigned __int64 large_page : 1;  //ps位 大页面
        unsigned __int64 accessed : 1; // 访问标志
        unsigned __int64 dirty : 1; // 脏页标志
        unsigned __int64 execute_for_usermode : 1; // 用户态执行
        unsigned __int64 reserved1 : 10; // 保留
        unsigned __int64 page_frame_number : 27; // PFN
        unsigned __int64 reserved2 : 15; // 保留
        unsigned __int64 suppressve : 1; // 抑制 #VE
    }page_directory_entry;  //2mb的结构
};

// See Table 28-6
union __ept_pte // EPT PTE 条目
{
    unsigned __int64 all; // 전체 值
    struct
    {
        unsigned __int64 read : 1; // bit 0
        unsigned __int64 write : 1; // bit 1
        unsigned __int64 execute : 1; // bit 2
        unsigned __int64 ept_memory_type : 3; // bit 5:3 (EPT Memory type)
        unsigned __int64 ignore_pat : 1; // bit 6
        unsigned __int64 ignored1 : 1; // bit 7
        unsigned __int64 accessed_flag : 1; // bit 8
        unsigned __int64 dirty_flag : 1; // bit 9
        unsigned __int64 execute_for_usermode : 1; // bit 10
        unsigned __int64 ignored2 : 1; // bit 11
        unsigned __int64 page_frame_number : 36; // bit (N-1):12 or Page-Frame-Number
        unsigned __int64 reserved : 4; // bit 51:N
        unsigned __int64 ignored3 : 11; // bit 62:52
        unsigned __int64 suppress_ve : 1; // bit 63
    };
};

//动态分割
struct __ept_dynamic_split // 将 2MB PDE 拆分为 512 个 PTE
{
    DECLSPEC_ALIGN(PAGE_SIZE) __ept_pte pml1[512];  // 页表 PT（512 * 4KB = 2MB）

    __ept_pde* entry; // 指向被拆分的原 PDE 条目，后续合并回 2MB 页时要写回这里

    LIST_ENTRY dynamic_split_list; // 预留链表节点（当前方案不依赖此链表）

    volatile long ref_count; // 运行期引用计数：有多少"hook/watch 页面记录"正在使用这个 split

    bool reclaimable_runtime_split; // true=这是运行期为 hook/watch 临时拆出来的 split，可回收
    // false=初始化阶段因 MTRR 边界拆出的 split，不做自动合并回收
};

struct __ept_pdpte_page // 单个 PDPT 页，覆盖 512GB 的 GPA 空间
{
    DECLSPEC_ALIGN(PAGE_SIZE) __ept_pdpte entries[512];
};

struct __ept_pde_page // 单个 PD 页，覆盖 1GB 的 GPA 空间
{
    DECLSPEC_ALIGN(PAGE_SIZE) __ept_pde entries[512];
};

//数组最大为512即2^9次方
struct __vmm_ept_page_table // EPT 页表（PML4/PDPTE/PDE）
{
    DECLSPEC_ALIGN(PAGE_SIZE) __ept_pml4e pml4[512]; // PML4 表

    DECLSPEC_ALIGN(PAGE_SIZE) __ept_pdpte pml3[512]; // PDPTE 表

    DECLSPEC_ALIGN(PAGE_SIZE) __ept_pde pml2[EPT_PD_COUNT][512]; // PDE 表（分组）
};

#pragma once

#ifndef _GLOBALS_CONSTANTS_H
#define _GLOBALS_CONSTANTS_H

//16位md5哈希摘要
#define EPTW_WRITE             0xE943BC6264401591 // EPT 监视类型：写访问（用于区分不同的监视方式）
#define EPTW_READWRITE         0x03F03258ADC870FC // EPT 监视类型：读/写访问（用于区分不同的监视方式）
#define EPTW_EXECUTE           0xB68C0804641E71ED // EPT 监视类型：执行访问（用于区分不同的监视方式）

#define EPTO_VIRTUAL_BREAKPOINT    1 // EPT hook 选项：虚拟断点类型（不修改来宾代码）
#define EPTO_HOOK_FUNCTION         2 // EPT hook 选项：函数 hook 类型（切换到伪页执行）

#define EPT_PD_COUNT 512 // EPT 中 PML2（PDE）数组的数量，覆盖 512 * 2MB = 1GB 的 PDPTE 段
// 不支持 1GB EPT 大页（IA32_VMX_EPT_VPID_CAP[17]=0）的 CPU 上，非 RAM（MMIO/地址空洞）区域
// 改为首次访问时按需映射：每个 1GB 区域需要一个 PD 页，这里是每 vCPU 预分配的 PD 页池容量。
// 128 页 = 128 个独立 1GB MMIO 窗口；池必须在 VMLAUNCH 之前分配完成，
// 因为 EPT violation 处理运行在来宾 IRQL 上，不允许在根模式调用可能阻塞的内核分配接口。
#define EPT_MMIO_PD_POOL_PAGE_COUNT 128
// #define HOST_PHYSICAL_MEMORY_PD_COUNT 64 // 旧逻辑：Host物理直映只覆盖64GB，高内存平台VMX root访问页表/内存时可能越界
#define HOST_PHYSICAL_MEMORY_PD_COUNT 512 // 宿主物理内存映射使用的 PDE 组数量（512 * 1GB = 512GB）
#define HOST_DYNAMIC_PAGE_TABLE_POOL_COUNT 1024 // Host关键虚拟页补映射页表池容量，仅扩大覆盖，不作为VT进入条件

#define VMM_TAG 'vtmm' // 内核池分配的标记，用于内存跟踪
#define VMM_STACK_SIZE 0x6000 // VMM 使用的宿主栈大小（用于 VM-Exit 处理）

//2mb pde
#define LARGE_PAGE_SIZE 0x200000 // 大页大小（2MB），用于 PDE 大页映射

#define MASK_32BITS 0xffffffff // 32 位掩码常量
#define HOST_GDT_DESCRIPTOR_COUNT  4 // 宿主 GDT 的描述符数量
#define HOST_IDT_DESCRIPTOR_COUNT  256 // 宿主 IDT 的描述符数量（完整 256 向量）

#define CPU_BASED_MONITOR_TRAP_FLAG 0x08000000 // MTF（Monitor Trap Flag）控制位（Primary Controls）

#define EPTWATCHLISTSIZE  256 // EPT 监视断点的全局表容量

#endif // !_GLOBALS_CONSTANTS_H

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
#define HOST_PHYSICAL_MEMORY_PD_COUNT 64 // 宿主物理内存映射使用的 PDE 组数量（64 * 1GB = 64GB）  //64

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

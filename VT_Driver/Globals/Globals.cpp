#include "Globals/GlobalsInternal.h"

__pseudo_descriptor64 g_gdtr = { 0 }; // 全局保存来宾 GDTR：卸载/异常恢复时需要把 GDT 恢复，避免段错误
__pseudo_descriptor64 g_idtr = { 0 }; // 全局保存来宾 IDTR：退出 VMX 或崩溃恢复时可重置中断表
unsigned __int64 g_guest_cr0; // 全局保存来宾 CR0：恢复控制寄存器，保持系统一致性
unsigned __int64 g_guest_cr3; // 全局保存来宾 CR3：恢复地址空间，防止访问错误地址
unsigned __int64 g_guest_cr4; // 全局保存来宾 CR4：恢复 VMX/SMEP/SMAP 等关键控制位
void* g_vt_driver_image_base;
unsigned __int64 g_vt_driver_image_size;
__vmm_context g_vmm_context; // VMM 全局上下文：集中存储所有 VCPU、EPT、池管理等核心状态
uint16_t guest_vpid = 1; // 来宾 VPID：当前仅单 VM，固定为 1，便于 TLB 管理和 INVVPID
int eptWatchListSize; // 监视表大小：保留字段（当前未用），为未来动态扩展监视表做准备
int eptWatchListPos; // 监视表位置游标：保留字段（当前未用），未来可用于快速分配
volatile long eptWatchList_lock = 0; // 监视表自旋锁：多核并发访问时防止竞态与数据破坏
volatile long eptWatchTransaction_lock = 0; // 安装/删除事务锁：重操作可在表锁外串行执行

volatile long g_vmx_initialized_count = 0; // 已成功初始化 VMX 的 CPU 数量（当前仅递增、无读取方，待清理）

EPTWatchEntry eptWatchList[EPTWATCHLISTSIZE]; // 全局监视断点数组：保存所有 EPT watch 条目，供所有 CPU 共享

PFN_PSGETNEXTPROCESS PsGetNextProcess; // PsGetNextProcess 函数指针：运行时解析，避免静态导入在不同系统版本失效

namespace ethread_offset // ETHREAD 结构偏移集合
{
    size_t Tcb; // ETHREAD.Tcb 偏移：用于从 ETHREAD 解析线程控制块
    size_t CrossThreadFlags; // ETHREAD.CrossThreadFlags 偏移：判断线程状态/标志
    //size_t Cid; // ETHREAD.Cid 偏移：读取线程/进程 ID 信息
    size_t Cid = static_cast<size_t>(0xFFFFFFFF); // 方案 2：启动时明确标记为“未就绪”，等待 Win10 sys 通过 InitOffset 下发
    size_t RundownProtect; // ETHREAD.RundownProtect 偏移：判断线程对象是否仍有效
}

namespace hv
{
    // selectors for the host GDT
    // host的gdt选择器
    segment_selector host_cs_selector = { 0, 0, 1 }; // 宿主 CS 选择子：指向 GDT 中代码段，确保 VM-Exit 后可执行
    segment_selector host_tr_selector = { 0, 0, 2 }; // 宿主 TR 选择子：指向 GDT 中 TSS 段，支持中断/任务切换
    //将host的物理地址映射在pml4[255]处
    uint64_t host_physical_memory_pml4_idx = 255; // 选择 PML4 高位索引作为物理内存直映窗口，避开来宾地址空间
    // directly access physical memory by using [base + offset]
    // 指示有效的4级分页从pml4[255]开始
    uint8_t* host_physical_memory_base = reinterpret_cast<uint8_t*>((uint64_t)255 << (9 + 9 + 9 + 12)); // 计算直映基址：通过此地址 + GPA 访问物理内存
    hypervisor ghv; // Hypervisor 全局实例：保存宿主页表、System CR3 与 Windows 偏移
}

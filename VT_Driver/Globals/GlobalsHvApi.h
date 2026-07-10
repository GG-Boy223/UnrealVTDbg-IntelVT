#pragma once

#include "GlobalsHvRuntimeTypes.h"

namespace hv // Hypervisor 命名空间
{
    EXTERN_C
    {
        void interrupt_handler_0(); // #DE 处理入口
        void interrupt_handler_1(); // #DB 处理入口
        void interrupt_handler_2(); // NMI 处理入口
        void interrupt_handler_3(); // #BP 处理入口
        void interrupt_handler_4(); // #OF 处理入口
        void interrupt_handler_5(); // #BR 处理入口
        void interrupt_handler_6(); // #UD 处理入口
        void interrupt_handler_7(); // #NM 处理入口
        void interrupt_handler_8(); // #DF 处理入口
        void interrupt_handler_10(); // #TS 处理入口
        void interrupt_handler_11(); // #NP 处理入口
        void interrupt_handler_12(); // #SS 处理入口
        void interrupt_handler_13(); // #GP 处理入口
        void interrupt_handler_14(); // #PF 处理入口
        void interrupt_handler_16(); // #MF 处理入口
        void interrupt_handler_17(); // #AC 处理入口
        void interrupt_handler_18(); // #MC 处理入口
        void interrupt_handler_19(); // #XM 处理入口
        void interrupt_handler_20(); // #VE 处理入口
        void interrupt_handler_30(); // 安全异常入口
    }

    EXTERN_C
    {
        // defined in vm-launch.asm
        bool vm_launch(); // 执行 VMLAUNCH

        // defined in vm-exit.asm
        void vm_exit(); // VM-Exit 入口

        void handle_host_interrupt(trap_frame* const frame); // 宿主中断统一处理

        // 具有异常处理的xsetbv
        void xsetbv_safe(host_exception_info& e, uint32_t idx, uint64_t value); // 安全执行 XSETBV

        // 具有异常处理的 rdmsr
        uint64_t rdmsr_safe(host_exception_info& e, uint32_t msr); // 安全执行 RDMSR

        // 具有异常处理的wrmsr
        void wrmsr_safe(host_exception_info& e, uint32_t msr, uint64_t value);        // 安全执行 WRMSR

        // 具有异常处理的 memcpy
        void memcpy_safe(host_exception_info& e, void* dst, void const* src, size_t size); // 安全内存拷贝
    }

    ia32_vmx_procbased_ctls_register read_ctrl_proc_based(); // 读取 Primary 控制寄存器
    void write_ctrl_proc_based(ia32_vmx_procbased_ctls_register const value);    // 写入 Primary 控制寄存器

    void prepare_host_idt(segment_descriptor_interrupt_gate_64* const idt); // 构建宿主 IDT

    // initialize the host GDT and populate every descriptor
    // 初始化host GDT并填充每个描述符
    void prepare_host_gdt(
        segment_descriptor_32* const gdt,
        task_state_segment_64 const* const tss); // 构建宿主 GDT/TSS

    void prepare_external_structures(__vcpu* const vcpu); // 初始化宿主外部结构（GDT/IDT/TSS）
    bool prepare_host_page_tables(); // 初始化宿主页表
    bool ensure_host_page_tables_for_vcpu(__vcpu const* vcpu); // 扩大Host页表覆盖，确保VM-exit关键代码/数据页可翻译
    bool validate_host_page_tables_for_vcpu(__vcpu const* vcpu); // 验证VM-exit宿主路径必需地址在HOST_CR3下可翻译
    void cleanup_host_page_tables(); // 释放Host页表动态补映射页

    void inject_nmi(); // 注入 NMI 到 guest

    cr0 read_effective_guest_cr0(); // 获取 guest 有效 CR0
    cr4 read_effective_guest_cr4(); // 获取 guest 有效 CR4
    bool load_vmcs_pointer(vmcs& vmcs_region); // 加载 VMCS 指针

    //读取guest通用寄存器
    uint64_t read_guest_gpr(guest_context const* const ctx, uint64_t const gpr_idx); // 读取 guest GPR

    //写guest通用寄存器
    void write_guest_gpr(guest_context* const ctx, uint64_t const gpr_idx, uint64_t const value); // 写 guest GPR

    vmx_interruptibility_state read_interruptibility_state(); // 读取中断阻塞状态
    void write_interruptibility_state(vmx_interruptibility_state const value); // 写入中断阻塞状态
    bool enter_vmx_operation(vmxon& vmxon_region); // 执行 VMXON 进入 VMX operation

    // read MTRR data into a single structure
    mtrr_data read_mtrr_data(); // 读取 MTRR 配置

    // calculate the MTRR memory type for the given physical memory range
    uint8_t calc_mtrr_mem_type(mtrr_data const& mtrrs, uint64_t address, uint64_t size); // 计算范围内内存类型

    // set the memory type in every EPT paging structure to the specified value
    // 将每个 EPT 分页结构中的内存类型设置为指定值将每个 EPT 分页结构中的内存类型设置为指定值
    void set_ept_memory_type(__ept_state& ept_state, uint8_t const memory_type); // 统一设置 EPT 内存类型

    void update_ept_memory_type(__ept_state& ept_state); // 按 MTRR 更新 EPT 内存类型

    //将guest虚拟地址转为guest物理地址
    uint64_t gva2gpa(cr3 guest_cr3, void* gva, size_t* offset_to_next_page = nullptr); // GVA->GPA

    //将GVA转换为GPA
    uint64_t get_physical_address(unsigned __int64 guest_cr3, _In_ PVOID BaseAddress); // 便捷 GVA->GPA

    // attempt to read the memory at the specified guest virtual address from root-mode
    size_t read_guest_virtual_memory(cr3 const guest_cr3,
        void* const gva, void* const hva, size_t const size); // 读取 guest 内存（指定 CR3）

    // attempt to read the memory at the specified guest virtual address from root-mode
    // 读取guest中当前进程的虚拟内存
    size_t read_guest_virtual_memory(void* const gva, void* const hva, size_t const size); // 读取 guest 内存（当前 CR3）

    size_t write_guest_virtual_memory(cr3 const guest_cr3,
        void* const gva, void* const hva, size_t const size); // 写入 guest 内存（指定 CR3）

    // 写入guest中当前进程的虚拟内存
    size_t write_guest_virtual_memory(void* const gva, void* const hva, size_t const size); // 写入 guest 内存（当前 CR3）

    // get the ETHREAD of the current guest
    // 获取guest里的当前线程对象
    size_t current_guest_ethread(); // 获取 guest 当前线程对象

    //返回触发的断点给guest调试器
    bool get_breakpoint_detected(__vcpu* vcpu, PBREAKPOINT_DETECTED vmcallinfo); // 返回断点触发信息

    //清理断点删除/继续运行后的 VT 侧临时状态，不修改监视表
    bool clear_breakpoint_state(__vcpu* vcpu, void* vmcallinfo); // 清理断点残留状态
    bool mark_pending_mtf_page(__vcpu* vcpu, __ept_hooked_page_info* hooked_page_info); // 标记一个活跃 MTF 页，避免覆盖未收尾状态
    void clear_pending_mtf_metadata(__ept_state* ept_state); // 清除 MTF 元数据
    void clear_pending_mtf_metadata(__vcpu* vcpu); // 清除 VCPU 的 MTF 元数据

    //向guest注入#DB事件
    void inject_single_step(__vcpu* vcpu); // 注入单步异常

    //获取空闲的id
    int getIdleWatchID(); // 获取空闲监视 ID
    void InitGlobalVariables(); // 初始化全局保存的寄存器状态
}

typedef PEPROCESS(__fastcall* PFN_PSGETNEXTPROCESS)(_In_ PEPROCESS Process); // PsGetNextProcess 函数指针类型

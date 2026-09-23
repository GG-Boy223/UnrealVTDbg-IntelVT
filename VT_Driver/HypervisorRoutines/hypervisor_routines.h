#pragma once // 防止头文件被多次包含

#ifndef _HYPERVISOR_ROUTINES_H // 头文件保护宏防止重复包含
#define _HYPERVISOR_ROUTINES_H // 结束保护


enum __syscall_type
{
	SYSCALL_NT, // 代表 NT 系统调用入口(nasated ntoskrnl 调用)
	SYSCALL_WIN32K // 代表 Win32K 系统调用入口(桌面/窗口系统调用)
};

namespace hv // Hypervisor 核心例程命名空间
{
	void set_mtf(bool set); // 设置/清除 Monitor Trap Flag，触发单步 VM-Exit 用于调试断点

	/// <summary>
	/// Check if cpu support virtualization
	/// </summary>
	/// <returns></returns>
	bool virtualization_support(); // 检测 CPU 是否支持 VT-x/VMX 虚拟化功能

	/// <summary>
	/// Check capabilities required by the current EPT/VPID implementation.
	/// </summary>
	bool required_vmx_capabilities_supported();

	/// <summary>
	/// Disable vmx operation
	/// </summary>
	/// <returns></returns>
	void disable_vmx_operation(); // 关闭 VMX 操作，清除 CR4.VMXE 以退出虚拟化模式

	/// <summary>
	/// Read vmcs field
	/// </summary>
	/// <param name="vmcs_field"></param>
	/// <returns></returns>
	unsigned __int64 vmread(unsigned __int64 vmcs_field); // 读取指定 VMCS 字段

	/// <summary>
	/// Dump whole vmcs structure
	/// </summary>
	void dump_vmcs(); // 打印/转储 VMCS 全部字段，用于调试

	/// <summary>
	/// Set 1 msr in msr bitmap
	/// </summary>
	/// <param name="msr"> Msr number </param>
	/// <param name="vcpu"> Pointer to current vcpu </param>
	/// <param name="read"> If set vmexit occur on reading this msr </param>
	/// <param name="write"> If set vmexit occur on writing to this msr </param>
	/// <param name="value"> If true set msr bit else clear </param>
	void set_msr_bitmap(vmx_msr_bitmap& bitmap, uint32_t const msr, bool const enable_exiting); // 设置 MSR 位图中指定 MSR 的位，控制是否触发 VM-Exit

	/// <summary>
	/// Set or unset bit in io port bitmap
	/// </summary>
	/// <param name="io_port"> IO port which you want to set</param>
	/// <param name="vcpu"> Pointer to current vcpu </param>
	/// <param name="value"> If true then set bit else unset bit</param>
	void set_io_bitmap(unsigned __int16 io_port, __vcpu* vcpu, bool value); // 设置 I/O 位图中指定端口的位，控制读写是否触发 VM-Exit

	/// <summary>
	/// 获取当前guest的权限
	/// </summary>
	/// <returns> Return current guest privilage level</returns>
	unsigned __int8 get_guest_cpl(); // 获取当前 CPL，0=内核态 3=用户态

	//获取guest当前cpl
	uint16_t current_guest_cpl(); // 获取当前 CPL，同上更详细的权限等级

	/// <summary>
	/// Swap cr3 with current process dtb
	/// </summary>
	/// <returns> old cr3 </returns>
	unsigned __int64 swap_context(); // 切换当前进程 CR3，返回旧 CR3
	void swap_context(__vcpu* vcpu, unsigned __int64 new_cr3); // 更新 vcpu->target_cr3 并执行切换

	/// <summary>
	/// Restore cr3
	/// </summary>
	/// <param name="old_cr3"></param>
	void restore_context(__vcpu* vcpu); // 恢复 vcpu->target_cr3 到之前的切换前状态

	/// <summary>
	/// Check if address is canonicial (level 4 paging)
	/// </summary>
	/// <param name="address"></param>
	/// <returns></returns>
	bool is_address_canonical(unsigned __int64 address); // 检查地址是否为规范地址 canonical address

	/// <summary>
	/// Get system directory table base
	/// </summary>
	/// <returns></returns>
	unsigned __int64 get_system_directory_table_base(); // 获取 System 进程 CR3 即内核目录表基址

	/// <summary>
	/// Inject interrupt/exception to guest system
	/// </summary>
	/// <param name="vector"></param>
	/// <param name="type"></param>
	/// <param name="error_code"></param>
	/// <param name="deliver_error_code"></param>
	void inject_interruption(unsigned __int32 vector, unsigned __int32 type, unsigned __int32 error_code, bool deliver_error_code); // 向 Guest 注入中断/异常

	/// <summary>
	/// Inject an interrupt/exception together with its guest debug-register state.
	/// </summary>
	void inject_interruption_with_debug_state(
		unsigned __int32 vector,
		unsigned __int32 type,
		unsigned __int32 error_code,
		bool deliver_error_code,
		unsigned __int64 debug_register_state);

	/// <summary>
	/// Queue a VM-entry event when the VMCS injected-event field is already occupied.
	/// </summary>
	void queue_pending_vm_entry_event(
		__vcpu* vcpu,
		unsigned __int32 interrupt_info,
		unsigned __int32 instruction_length,
		unsigned __int32 error_code,
		unsigned __int64 debug_register_state,
		bool has_debug_register_state);

	/// <summary>
	/// Pop one queued VM-entry event.
	/// </summary>
	bool pop_pending_vm_entry_event(
		__vcpu* vcpu,
		unsigned __int32* interrupt_info,
		unsigned __int32* instruction_length,
		unsigned __int32* error_code,
		unsigned __int64* debug_register_state,
		bool* has_debug_register_state);

	/// <summary>
	/// Inject the oldest queued event if the VM-entry injected-event field is free.
	/// </summary>
	void drain_pending_vm_entry_event(__vcpu* vcpu);

	/// <summary>
	/// Keep MTF armed while queued events still need a following VM exit.
	/// </summary>
	void ensure_pending_vm_entry_event_progress(__vcpu* vcpu);

	/// <summary>
	/// Write to reset io port to perform hard reset
	/// </summary>
	void hard_reset(); // 通过 I/O 端口触发硬复位

	/// <summary>
	/// Used to get address passed by user in inpvcid
	/// </summary>
	/// <param name="guest_registers"></param>
	/// <returns></returns>
	unsigned __int64 get_guest_address(__vcpu* vcpu); // 从 VM-Exit 传递的信息中获取 Guest 地址

	/// <summary>
	/// Write to vmcs field
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="vmcs_field">Field number</param>
	/// <param name="value"> Value </param>
	template <typename T>
	void vmwrite(unsigned __int64 vmcs_field, T value)
	{
		//0表示执行成功
		unsigned char res = __vmx_vmwrite(vmcs_field, (unsigned __int64)value); // 写 VMCS 字段
		if (res) // VMX 执行失败 0 表示成功
		{

		}
	}

	bool vmx_on(unsigned __int64 vmxon_phys_addr); // 执行 VMXON 指令，进入 VMX operation

	bool vmx_vmclear(unsigned __int64 vmcs_phys); // 执行 VMCLEAR 初始化 VMCS

	bool vmx_vmptrld(unsigned __int64 vmcs_phys); // 执行 VMPTRLD 加载 VMCS 指针
}

#endif // !_HYPERVISOR_ROUTINES_H // 结束头文件保护

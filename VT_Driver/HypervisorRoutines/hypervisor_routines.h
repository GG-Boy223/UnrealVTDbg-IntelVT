#pragma once // 确保该头文件只被编译器包含一次，防止重复定义

#ifndef _HYPERVISOR_ROUTINES_H // 头文件保护宏开始，避免多次包含导致重复定义
#define _HYPERVISOR_ROUTINES_H // 定义保护宏


enum __syscall_type
{
	SYSCALL_NT, // 表示 NT 系统调用类型（ntoskrnl 相关）
	SYSCALL_WIN32K // 表示 Win32K 系统调用类型（图形/窗口子系统相关）
};

namespace hv // Hypervisor 低层例程命名空间
{
	void set_mtf(bool set); // 设置或清除 Monitor Trap Flag（单步 VM-Exit 的控制位）

	/// <summary>
	/// Check if cpu support virtualization
	/// </summary>
	/// <returns></returns>
	bool virtualization_support(); // 检查 CPU 是否支持 VT-x（VMX）虚拟化

	/// <summary>
	/// Disable vmx operation
	/// </summary>
	/// <returns></returns>
	void disable_vmx_operation(); // 关闭 VMX 操作（清除 CR4.VMXE 等），退出虚拟化模式

	/// <summary>
	/// Read vmcs field
	/// </summary>
	/// <param name="vmcs_field"></param>
	/// <returns></returns>
	unsigned __int64 vmread(unsigned __int64 vmcs_field); // 读取指定 VMCS 字段

	/// <summary>
	/// Dump whole vmcs structure
	/// </summary>
	void dump_vmcs(); // 打印/导出 VMCS 全部字段（调试用）

	/// <summary>
	/// Set 1 msr in msr bitmap
	/// </summary>
	/// <param name="msr"> Msr number </param>
	/// <param name="vcpu"> Pointer to current vcpu </param>
	/// <param name="read"> If set vmexit occur on reading this msr </param>
	/// <param name="write"> If set vmexit occur on writing to this msr </param>
	/// <param name="value"> If true set msr bit else clear </param>
	void set_msr_bitmap(vmx_msr_bitmap& bitmap, uint32_t const msr, bool const enable_exiting); // 设置 MSR 位图：控制某个 MSR 访问是否触发 VM-Exit

	/// <summary>
	/// Set or unset bit in io port bitmap
	/// </summary>
	/// <param name="io_port"> IO port which you want to set</param>
	/// <param name="vcpu"> Pointer to current vcpu </param>
	/// <param name="value"> If true then set bit else unset bit</param>
	void set_io_bitmap(unsigned __int16 io_port, __vcpu* vcpu, bool value); // 设置 I/O 位图：控制某个端口读写是否触发 VM-Exit

	/// <summary>
	/// 返回当前guest的特权级
	/// </summary>
	/// <returns> Return current guest privilage level</returns>
	unsigned __int8 get_guest_cpl(); // 获取来宾当前 CPL（0=内核，3=用户）

	//获取guest的当前cpl
	uint16_t current_guest_cpl(); // 获取来宾当前 CPL（从段描述符访问权限计算）

	/// <summary>
	/// Swap cr3 with current process dtb
	/// </summary>
	/// <returns> old cr3 </returns>
	unsigned __int64 swap_context(); // 切换到当前进程 CR3，返回旧 CR3
	void swap_context(__vcpu* vcpu, unsigned __int64 new_cr3); // 设置 vcpu->target_cr3（延迟切换）

	/// <summary>
	/// Restore cr3
	/// </summary>
	/// <param name="old_cr3"></param>
	void restore_context(__vcpu* vcpu); // 恢复 vcpu->target_cr3，结束临时切换

	/// <summary>
	/// Check if address is canonicial (level 4 paging)
	/// </summary>
	/// <param name="address"></param>
	/// <returns></returns>
	bool is_address_canonical(unsigned __int64 address); // 判断地址是否为合法的 canonical 地址

	/// <summary>
	/// Get system directory table base
	/// </summary>
	/// <returns></returns>
	unsigned __int64 get_system_directory_table_base(); // 获取 System 进程 CR3（用于内核地址空间）

	/// <summary>
	/// Inject interrupt/exception to guest system
	/// </summary>
	/// <param name="vector"></param>
	/// <param name="type"></param>
	/// <param name="error_code"></param>
	/// <param name="deliver_error_code"></param>
	void inject_interruption(unsigned __int32 vector, unsigned __int32 type, unsigned __int32 error_code, bool deliver_error_code); // 向来宾注入中断/异常

	/// <summary>
	/// Write to reset io port to perform hard reset
	/// </summary>
	void hard_reset(); // 通过 I/O 端口触发硬件复位

	/// <summary>
	/// Used to get address passed by user in inpvcid
	/// </summary>
	/// <param name="guest_registers"></param>
	/// <returns></returns>
	unsigned __int64 get_guest_address(__vcpu* vcpu); // 从 VM-Exit 指令信息中解析来宾线性地址

	/// <summary>
	/// Write to vmcs field
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="vmcs_field">Field number</param>
	/// <param name="value"> Value </param>
	template <typename T>
	void vmwrite(unsigned __int64 vmcs_field, T value)
	{
		//0则表示操作成功
		unsigned char res = __vmx_vmwrite(vmcs_field, (unsigned __int64)value); // 写 VMCS 字段
		if (res)
		{

		}
	}

	bool vmx_on(unsigned __int64 vmxon_phys_addr); // 执行 VMXON 指令，进入 VMX operation

	bool vmx_vmclear(unsigned __int64 vmcs_phys); // 执行 VMCLEAR，清空 VMCS

	bool vmx_vmptrld(unsigned __int64 vmcs_phys); // 执行 VMPTRLD，加载 VMCS 指针
}

#endif // !_HYPERVISOR_ROUTINES_H // 头文件保护宏结束

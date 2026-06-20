#include "VmmInternal.h"

//调节控制寄存器 cr4 cr0来启用vmx模式
void adjust_control_registers() // 按IA32_VMX_CRx_FIXED限制设置CR0/CR4以允许VMX，避免VMXON失败
{
	__cr4 cr4;
	__cr0 cr0;
	__cr_fixed cr_fixed;

	_disable(); // 关闭中断，避免在修改控制寄存器时被打断导致不一致
	cr_fixed.all = __readmsr(IA32_VMX_CR0_FIXED0); // 读取CR0必须置1的位，满足VMX硬件约束
	cr0.all = __readcr0(); // 读取当前CR0作为基线
	cr0.all |= cr_fixed.split.low; // 强制置1的位写入CR0，避免VMXON触发#GP
	cr_fixed.all = __readmsr(IA32_VMX_CR0_FIXED1); // 读取CR0必须为0的位掩码
	cr0.all &= cr_fixed.split.low; // 清除不允许为1的位，满足VMX约束
	__writecr0(cr0.all); // 写回CR0使其满足VMX要求
	cr_fixed.all = __readmsr(IA32_VMX_CR4_FIXED0); // 读取CR4必须置1的位
	cr4.all = __readcr4(); // 读取当前CR4
	cr4.all |= cr_fixed.split.low; // 强制置1的位写入CR4，满足VMX硬件要求
	cr_fixed.all = __readmsr(IA32_VMX_CR4_FIXED1); // 读取CR4必须为0的位掩码
	cr4.all &= cr_fixed.split.low; // 清除不允许为1的位
	__writecr4(cr4.all); // 写回CR4使其满足VMX要求
	_enable(); // 恢复中断，避免影响系统响应

	//设置IA32_FEATURE_CONTROL寄存器的bit0 bit2支持开启vmx模式
	__ia32_feature_control_msr feature_msr = { 0 };
	feature_msr.all = __readmsr(IA32_FEATURE_CONTROL); // 读取VMX功能控制MSR，判断是否允许VMXON

	if (feature_msr.lock == 0) // 未锁定时允许配置VMX开关，否则写入会被拒绝
	{
		feature_msr.vmxon_outside_smx = 1; // 允许在非SMX模式下执行VMXON，避免被MSR阻止
		feature_msr.lock = 1; // 锁定位，防止后续被修改导致状态不一致

		__writemsr(IA32_FEATURE_CONTROL, feature_msr.all); // 写回MSR使设置生效
	}
}

//初始化逻辑处理器并启动当前vmcs管理的虚拟机
void init_logical_processor2(unsigned int iter) // 在指定CPU上完成VMXON/VMCS配置并启动来宾，保证每核一致
{
	unsigned __int64 processor_number = iter;

	__vcpu* vcpu = &g_vmm_context.vcpu[processor_number]; // 获取该CPU对应的VCPU结构作为操作对象

	//调节控制寄存器 cr4 cr0来启用vmx模式
	adjust_control_registers(); // 满足VMX进入条件，否则VMXON可能失败

	if (!hv::enter_vmx_operation(vcpu->vmxon))  //进入vmx模式
	{
		LogError("Failed to put vcpu %d into VMX operation.\n", processor_number); // VMXON失败
	 // 无法进入VMX根模式，后续VMCS操作无效
	}


	if (!hv::load_vmcs_pointer(vcpu->vmcs)) // 让VMCS成为当前活动VMCS，后续vmwrite/vmread生效
	{
		LogError("load_vmcs_pointer失败.\n", processor_number); // VMCS加载失败
		//return false; // 无法继续配置VMCS
	}

	//创建host的idt和gdt
	hv::prepare_external_structures(vcpu); // 构造宿主GDT/IDT/TSS等必需结构，确保VM-Exit能正确返回宿主
	vcpu->vcpu_status.vmx_on = true; // 标记该VCPU已进入VMX根模式，供状态判断
	LogInfo("vcpu %d is now in VMX operation.\n", processor_number); // 输出调试信息

	//配置vmcs区域
	fill_vmcs(vcpu, 0); // 填充VMCS控制字段/来宾与宿主状态，决定VM-Entry/Exit行为
	vcpu->vcpu_status.vmm_launched = true; // 标记已准备启动来宾，用于卸载流程判断

	//从GUEST_RIP指定的位置继续执行
	//运行vm虚拟机
	if (!hv::vm_launch())
	{
		vcpu->vmexit_info.instruction_error = hv::vmread(VM_INSTRUCTION_ERROR); // 读取VM指令错误码，定位失败原因
		LogError("Vmlaunch failed error: %d", vcpu->vmexit_info.instruction_error); // 输出失败原因
		vcpu->vcpu_status.vmm_launched = false; // 回滚启动标记，防止卸载误判
		vcpu->vcpu_status.vmx_on = false; // 回滚VMX状态
		__vmx_off();  //退出vmx模式，避免处于不一致状态
		//return false; // 启动失败
	}
	//return true; // VM成功启动
}

bool initalize_vcpu(unsigned int iter) // 按CPU索引初始化并启动对应VCPU，集中处理每核初始化流程
{
	init_logical_processor2(iter); // 将初始化工作委托给逻辑处理器初始化流程

	return true;
}

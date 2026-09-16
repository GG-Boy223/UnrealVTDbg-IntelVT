#include "VmmInternal.h"
#include "../ASM/AsmCallset.h"

namespace
{
	void vmxoff_after_failed_initialization(__vcpu* vcpu, unsigned __int64 failure_phase)
	{
		unsigned __int64 rflags = 0;
		if (!__vmx_off_checked(&rflags))
		{
			vmx_fatal_error(
				vmx_fatal_vmxoff_instruction,
				failure_phase,
				rflags);
		}

		vcpu->vcpu_status.vmx_on = false;
	}
}

//调节控制寄存器 cr4 cr0来启用vmx模式
void adjust_control_registers() // 按IA32_VMX_CRx_FIXED限制设置CR0/CR4以允许VMX，避免VMXON失败
{
	__cr4 cr4 = { };
	__cr0 cr0 = { };
	__cr_fixed cr_fixed = { };
	const bool interrupts_were_enabled = (__readeflags() & (1ull << 9)) != 0;

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
	if (interrupts_were_enabled)
	{
		_enable(); // 只恢复进入本函数前的 IF 状态，不改变调用者的中断语义。
	}

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
bool init_logical_processor2(unsigned int iter) // 在指定CPU上完成VMXON/VMCS配置并启动来宾，保证每核一致
{
	//DbgBreakPoint();
	unsigned __int64 processor_number = iter;

	__vcpu* vcpu = &g_vmm_context.vcpu[processor_number]; // 获取该CPU对应的VCPU结构作为操作对象
	int cpu_info[4] = {};
	__cpuid(cpu_info, 1);
	unsigned int stepping = cpu_info[0] & 0xf;
	unsigned int model = (cpu_info[0] >> 4) & 0xf;
	unsigned int family = (cpu_info[0] >> 8) & 0xf;
	unsigned int extended_model = (cpu_info[0] >> 16) & 0xf;
	unsigned int extended_family = (cpu_info[0] >> 20) & 0xff;
	unsigned int display_family = family == 0xf ? family + extended_family : family;
	unsigned int display_model = (family == 0x6 || family == 0xf) ? model + (extended_model << 4) : model;

	LogInfo("Initializing VMX: index=%u group=%u number=%u family=0x%X model=0x%X stepping=0x%X",
		iter,
		vcpu->processor_number.Group,
		vcpu->processor_number.Number,
		display_family,
		display_model,
		stepping);
	outFile("========== CPU[%u] VT进入链路开始：Group=%u Number=%u Family=0x%X Model=0x%X Stepping=0x%X ==========",
		iter,
		vcpu->processor_number.Group,
		vcpu->processor_number.Number,
		display_family,
		display_model,
		stepping);

	// EPT 必须在目标核上构建：IA32_VMX_EPT_VPID_CAP（1GB 大页）、CPUID.80000008H（MAXPHYADDR）
	// 与 MTRR 都是每逻辑处理器状态；这一步要在 VMXON 之前完成，后面的 Host 页表补映射与 VMCS 填充都依赖它。
	if (!init_vcpu_ept(vcpu))
	{
		outFile("[失败][CPU%u] EPT构建失败，停止进入VT", iter);
		return false;
	}

	//调节控制寄存器 cr4 cr0来启用vmx模式
	outFile("[进行中][CPU%u] 调整CR0/CR4与IA32_FEATURE_CONTROL，准备满足VMXON硬件条件", iter);

	// ✅ P0-003 修复：使用自旋锁保护关键的 VMX 初始化操作
	spinlock::lock(&g_vmx_init_lock);

	//调节控制寄存器 cr4 cr0来启用vmx模式
	adjust_control_registers(); // 满足VMX进入条件，否则VMXON可能失败
	outFile("[成功][CPU%u] VMX控制寄存器调整完成：CR0=0x%llX CR4=0x%llX", iter, __readcr0(), __readcr4());

	outFile("[进行中][CPU%u] 开始执行VMXON：VMXON_VA=0x%p", iter, &vcpu->vmxon);
	if (!hv::enter_vmx_operation(vcpu->vmxon))  //进入vmx模式
	{
		LogError("Failed to put vcpu %d into VMX operation.\n", processor_number); // VMXON失败
		outFile("[失败][CPU%u] VMXON失败，停止后续VMCS操作", iter);
		spinlock::unlock(&g_vmx_init_lock); // 释放锁
		return false; // 无法进入VMX根模式，后续VMCS操作无效
	}

	vcpu->vcpu_status.vmx_on = true; // 标记该VCPU已进入VMX根模式，供状态判断
	InterlockedIncrement(&g_vmx_initialized_count); // 原子递增成功初始化计数

	spinlock::unlock(&g_vmx_init_lock); // 释放锁

	outFile("[成功][CPU%u] VMXON成功，当前CPU已进入VMX Root", iter);

	outFile("[进行中][CPU%u] 开始加载VMCS：VMCS_VA=0x%p", iter, &vcpu->vmcs);
	if (!hv::load_vmcs_pointer(vcpu->vmcs)) // 让VMCS成为当前活动VMCS，后续vmwrite/vmread生效
	{
		LogError("load_vmcs_pointer失败.\n", processor_number); // VMCS加载失败
		vmxoff_after_failed_initialization(vcpu, 1); // 仅在确认VMXOFF成功后清除状态并允许外层释放资源
		outFile("[失败][CPU%u] VMCLEAR/VMPTRLD失败，已执行VMXOFF回滚", iter);
		return false; // 无法继续配置VMCS
	}
	outFile("[成功][CPU%u] VMCS加载完成", iter);

	//创建host的idt和gdt
	outFile("[进行中][CPU%u] 开始准备Host GDT/IDT/TSS结构", iter);
	//创建host的idt和gdt
	hv::prepare_external_structures(vcpu); // 构造宿主GDT/IDT/TSS等必需结构，确保VM-Exit能正确返回宿主
	LogInfo("vcpu %d is now in VMX operation.\n", processor_number); // 输出调试信息
	outFile("[成功][CPU%u] Host GDT/IDT/TSS结构准备完成", iter);

	// if (!hv::validate_host_page_tables_for_vcpu(vcpu)) // 旧逻辑：只靠自检发现缺页，无法主动补齐Host CR3下的VM-exit关键页。
	if (!hv::ensure_host_page_tables_for_vcpu(vcpu))
	{
		outFile("[失败][CPU%u] Host关键页补映射不完整，回滚当前CPU的VMXON状态", iter);
		vmxoff_after_failed_initialization(vcpu, 2);
		return false;
	}

	if (!hv::validate_host_page_tables_for_vcpu(vcpu))
	{
		outFile("[失败][CPU%u] Host页表自检未通过，回滚当前CPU的VMXON状态", iter);
		vmxoff_after_failed_initialization(vcpu, 3);
		return false;
	}

	//配置vmcs区域
	outFile("[进行中][CPU%u] 开始填充VMCS控制域、Guest状态和Host状态", iter);
	//配置vmcs区域
	fill_vmcs(vcpu, 0); // 填充VMCS控制字段/来宾与宿主状态，决定VM-Entry/Exit行为
	outFile("[成功][CPU%u] VMCS填充完成", iter);
	vcpu->vcpu_status.vmm_launched = true; // 标记已准备启动来宾，用于卸载流程判断

	//从GUEST_RIP指定的位置继续执行
	//运行vm虚拟机
	outFile("[进行中][CPU%u] 开始执行VMLAUNCH", iter);
	if (!hv::vm_launch())
	{
		vcpu->vmexit_info.instruction_error = hv::vmread(VM_INSTRUCTION_ERROR); // 读取VM指令错误码，定位失败原因
		LogError("Vmlaunch failed error: %d", vcpu->vmexit_info.instruction_error); // 输出失败原因
		vcpu->vcpu_status.vmm_launched = false; // 回滚启动标记，防止卸载误判
		vmxoff_after_failed_initialization(vcpu, 4);
		outFile("[失败][CPU%u] VMLAUNCH失败：VM_INSTRUCTION_ERROR=%llu，已执行VMXOFF回滚",
			iter,
			vcpu->vmexit_info.instruction_error);
		return false; // 启动失败
	}

	outFile("========== CPU[%u] VT进入链路完成：VMLAUNCH返回成功 ==========", iter);
	return true; // VM成功启动
}

bool initalize_vcpu(unsigned int iter) // 按CPU索引初始化并启动对应VCPU，集中处理每核初始化流程
{
	return init_logical_processor2(iter); // 将初始化工作委托给逻辑处理器初始化流程
}

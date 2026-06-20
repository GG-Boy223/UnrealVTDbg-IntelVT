#include "VmExitMsr.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../hypervisor_routines.h"
#include "../Vmcs/VmcsApi.h"
#include "../interrupt.h"
#include "../AsmCallset.h"
#include "../msr.h"
#include "VmExitCommon.h"

// 处理 RDMSR 指令引发的 VM-exit，根据需要读取或伪装 MSR，最后返回到 EDX:EAX
void vmexit_msr_read_handler(__vcpu* vcpu)
{
	__msr msr;
	uint32_t msr_index = static_cast<uint32_t>(vcpu->vmexit_info.guest_registers->rcx);

	switch (msr_index)
	{
	case IA32_FEATURE_CONTROL:
	{
		__ia32_feature_control_msr feature_msr = { 0 };
		feature_msr.all = __readmsr(IA32_FEATURE_CONTROL);
		feature_msr.lock = 1;
		feature_msr.vmxon_inside_smx = 0;
		feature_msr.vmxon_outside_smx = 0;
		feature_msr.senter_local = 0;
		feature_msr.senter_global = 0;
		msr.all = feature_msr.all;
		break;
	}
	default:
	{
		hv::host_exception_info e;
		msr.all = hv::rdmsr_safe(e, msr_index);

		if (e.exception_occurred)
		{
			hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			return;
		}
		break;
	}
	}

	vcpu->vmexit_info.guest_registers->edx = msr.high;
	vcpu->vmexit_info.guest_registers->eax = msr.low;

	set_hide_vm_exit_overhead(vcpu, true);
	adjust_rip(vcpu);
}

// 处理 WRMSR 指令引发的 VM-exit，安全写入到 MSR，涉及 MTRR 时同步 EPT 内存类型
void vmexit_msr_write_handler(__vcpu* vcpu)
{
	sLog("\n");
	uint32_t msr_index = static_cast<uint32_t>(vcpu->vmexit_info.guest_registers->rcx);

	__msr msr;
	msr.high = vcpu->vmexit_info.guest_registers->edx;
	msr.low = vcpu->vmexit_info.guest_registers->eax;

	{
		hv::host_exception_info e;
		hv::wrmsr_safe(e, msr_index, msr.all);

		if (e.exception_occurred)
		{
			hv::inject_interruption(EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT,
				INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, true);
			return;
		}
	}

	// MTRR 影响内存类型，如果修改后需同步 EPT
	if (msr_index == IA32_MTRR_DEF_TYPE || msr_index == IA32_MTRR_FIX64K_00000 ||
	    msr_index == IA32_MTRR_FIX16K_80000 || msr_index == IA32_MTRR_FIX16K_A0000 ||
	    (msr_index >= IA32_MTRR_FIX4K_C0000 && msr_index <= IA32_MTRR_FIX4K_F8000) ||
	    (msr_index >= IA32_MTRR_PHYSBASE0 && msr_index <= IA32_MTRR_PHYSBASE0 + 511))
	{
		if (!hv::read_effective_guest_cr0().cache_disable)
			hv::update_ept_memory_type(*vcpu->ept_state);

		_mm_mfence();
		invept_all_contexts_func();
	}

	set_hide_vm_exit_overhead(vcpu, true);
	adjust_rip(vcpu);
}

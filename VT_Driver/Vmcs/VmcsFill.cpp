#include "VmcsInternal.h"

// 填充 VMCS：包含控制字段、来宾字段与宿主字段
void fill_vmcs(__vcpu* vcpu, void* guest_rsp) // 填充 VMCS
{
    __exception_bitmap exception_bitmap = { 0 }; // 异常位图结构
    __vmx_basic_msr vmx_basic = { 0 }; // VMX 基本能力 MSR
    __vmx_entry_control entry_controls = { 0 }; // VM-entry 控制
    __vmx_exit_control exit_controls = { 0 }; // VM-exit 控制
    __vmx_pinbased_control_msr pinbased_controls = { 0 }; // 引脚控制
    __vmx_primary_processor_based_control primary_controls = { 0 }; // 主控制
    __vmx_secondary_processor_based_control secondary_controls = { 0 }; // 次级控制

    vmx_basic.all = __readmsr(IA32_VMX_BASIC); // 读取 VMX 基本能力

    set_entry_control(entry_controls); // 设置 VM-entry 控制
    set_exit_control(exit_controls); // 设置 VM-exit 控制
    // VM 执行控制字段
    set_primary_controls(primary_controls); // 设置主控制
    set_secondary_controls(secondary_controls); // 设置次级控制
    set_pinbased_control_msr(pinbased_controls); // 设置引脚控制
    // 设置异常位图
    set_exception_bitmap(exception_bitmap); // 设置异常拦截

#ifndef _MINIMAL // 非精简模式
    memset(vcpu->vcpu_bitmaps.msr_bitmap, 0xff, PAGE_SIZE); // 初始化 MSR 位图为全拦截
#endif

    vcpu->vmcs_effective_controls.pinbased = ajdust_controls(pinbased_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_PINBASED_CTLS : IA32_VMX_PINBASED_CTLS);
    vcpu->vmcs_effective_controls.primary = ajdust_controls(primary_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_PROCBASED_CTLS : IA32_VMX_PROCBASED_CTLS);
    vcpu->vmcs_effective_controls.secondary = ajdust_controls(secondary_controls.all, IA32_VMX_PROCBASED_CTLS2);
    vcpu->vmcs_effective_controls.exit = ajdust_controls(exit_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_EXIT_CTLS : IA32_VMX_EXIT_CTLS);
    vcpu->vmcs_effective_controls.entry = ajdust_controls(entry_controls.all, vmx_basic.true_controls ? IA32_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS);

    __vmx_primary_processor_based_control effective_primary = { 0 };
    __vmx_secondary_processor_based_control effective_secondary = { 0 };
    effective_primary.all = vcpu->vmcs_effective_controls.primary;
    effective_secondary.all = vcpu->vmcs_effective_controls.secondary;
    unsigned __int64 ept_vpid_cap = 0;
    if (effective_secondary.enable_ept || effective_secondary.enable_vpid)
    {
        ept_vpid_cap = __readmsr(IA32_VMX_EPT_VPID_CAP);
    }

    LogInfo("VMCS effective controls: pin=0x%llX primary=0x%llX secondary=0x%llX exit=0x%llX entry=0x%llX EPT_VPID_CAP=0x%llX",
        vcpu->vmcs_effective_controls.pinbased,
        vcpu->vmcs_effective_controls.primary,
        vcpu->vmcs_effective_controls.secondary,
        vcpu->vmcs_effective_controls.exit,
        vcpu->vmcs_effective_controls.entry,
        ept_vpid_cap);
    outFile("[信息][VMCS控制位] 硬件调整后的有效控制位：PIN=0x%llX PRIMARY=0x%llX SECONDARY=0x%llX EXIT=0x%llX ENTRY=0x%llX EPT_VPID_CAP=0x%llX",
        vcpu->vmcs_effective_controls.pinbased,
        vcpu->vmcs_effective_controls.primary,
        vcpu->vmcs_effective_controls.secondary,
        vcpu->vmcs_effective_controls.exit,
        vcpu->vmcs_effective_controls.entry,
        ept_vpid_cap);

    hv::set_msr_bitmap(vcpu->msr_bitmap, IA32_FEATURE_CONTROL, true); // 监视 IA32_FEATURE_CONTROL
    hv::vmwrite<unsigned __int64>(PIN_BASED_VM_EXEC_CONTROL, vcpu->vmcs_effective_controls.pinbased);
    hv::vmwrite<unsigned __int64>(PRIMARY_PROCESSOR_BASED_VM_EXEC_CONTROL, vcpu->vmcs_effective_controls.primary);
    hv::vmwrite<unsigned __int64>(SECONDARY_PROCESSOR_BASED_VM_EXEC_CONTROL, vcpu->vmcs_effective_controls.secondary);
    hv::vmwrite<unsigned __int64>(VM_EXIT_CONTROLS, vcpu->vmcs_effective_controls.exit);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_CONTROLS, vcpu->vmcs_effective_controls.entry);
    // VMCS 链接指针初始化为全 1，表示无链接 VMCS
    hv::vmwrite<unsigned __int64>(VMCS_LINK_POINTER, MAXULONG64); // 设置为推荐值
    // 设置异常位图
    hv::vmwrite(EXCEPTION_BITMAP, exception_bitmap.all); // 写入异常位图
    // 配置 #PF 错误码匹配（这里设置为全部触发 VM-exit）
    hv::vmwrite<unsigned int>(PAGE_FAULT_ERROR_CODE_MASK, 0); // 错误码掩码
    hv::vmwrite<unsigned int>(PAGE_FAULT_ERROR_CODE_MATCH, 0); // 错误码匹配值

    if (effective_primary.use_msr_bitmaps == true)
    {
        hv::vmwrite(MSR_BITMAP_ADDRESS, MmGetPhysicalAddress(&vcpu->msr_bitmap).QuadPart); // 写入 MSR 位图地址
        outFile("[成功][VMCS控制位] 已写入MSR Bitmap：PA=0x%llX", MmGetPhysicalAddress(&vcpu->msr_bitmap).QuadPart);
    }

    if (effective_primary.use_io_bitmaps == true)
    {
        hv::vmwrite(IO_BITMAP_A_ADDRESS, vcpu->vcpu_bitmaps.io_bitmap_a_physical); // 写入 I/O 位图 A
        hv::vmwrite(IO_BITMAP_B_ADDRESS, vcpu->vcpu_bitmaps.io_bitmap_b_physical); // 写入 I/O 位图 B
        outFile("[成功][VMCS控制位] 已写入IO Bitmap：A_PA=0x%llX B_PA=0x%llX",
            vcpu->vcpu_bitmaps.io_bitmap_a_physical,
            vcpu->vcpu_bitmaps.io_bitmap_b_physical);
    }

    if (effective_secondary.enable_vpid == true)
    {
        hv::vmwrite(VIRTUAL_PROCESSOR_ID, guest_vpid); // 设置 VPID
        outFile("[成功][VMCS控制位] 已写入VPID：Value=%u", guest_vpid);
    }
    else
    {
        outFile("[信息][VMCS控制位] 有效控制位未启用VPID，跳过VPID写入");
    }

    if (effective_secondary.enable_ept == true)
    {
        hv::vmwrite(EPT_POINTER, vcpu->ept_state->ept_pointer->all); // 写入 EPTP
        LogInfo("VMCS EPTP=0x%llX VPID=%s", vcpu->ept_state->ept_pointer->all, effective_secondary.enable_vpid ? "enabled" : "disabled");
        outFile("[成功][VMCS控制位] 已写入EPTP：EPTP=0x%llX VPID=%s",
            vcpu->ept_state->ept_pointer->all,
            effective_secondary.enable_vpid ? "已启用" : "未启用");
    }
    else
    {
        outFile("[信息][VMCS控制位] 有效控制位未启用EPT，跳过EPTP写入");
    }

    // 清空 VM-exit MSR 载入列表
    hv::vmwrite<unsigned __int64>(VM_EXIT_MSR_LOAD_COUNT, 0); // 载入数量为 0
    hv::vmwrite<unsigned __int64>(VM_EXIT_MSR_LOAD_ADDR, 0); // 载入地址为 0
    // 清空 VM-exit MSR 保存列表与 VM-entry MSR 载入列表,避免陈旧地址被硬件使用。
    hv::vmwrite<unsigned __int64>(VM_EXIT_MSR_STORE_COUNT, 0);
    hv::vmwrite<unsigned __int64>(VM_EXIT_MSR_STORE_ADDR, 0);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_MSR_LOAD_COUNT, 0);
    hv::vmwrite<unsigned __int64>(VM_ENTRY_MSR_LOAD_ADDR, 0);
    // 清空 VM-entry 中断注入字段
    hv::vmwrite<unsigned __int64>(VM_ENTRY_INTERRUPTION_INFO_FIELD, 0); // 中断信息为 0
    hv::vmwrite<unsigned __int64>(VM_ENTRY_EXCEPTION_ERROR_CODE, 0); // 错误码为 0
    hv::vmwrite<unsigned __int64>(VM_ENTRY_INSTRUCTION_LENGTH, 0); // 指令长度为 0

    // 填充来宾与宿主字段
    fill_vmcs_guest_fields(vcpu, guest_rsp); // 设置来宾状态
    fill_vmcs_host_fields(vcpu); // 设置宿主状态
}

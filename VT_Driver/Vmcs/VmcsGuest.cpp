#include "VmcsInternal.h"

void fill_vmcs_guest_fields(__vcpu* vcpu, void* guest_rsp)
{
    __pseudo_descriptor64 gdtr = { 0 };
    __pseudo_descriptor64 idtr = { 0 };

    __sgdt(&gdtr);
    __sidt(&idtr);

    hv::vmwrite<unsigned __int64>(GUEST_GDTR_LIMIT, gdtr.limit);
    hv::vmwrite<unsigned __int64>(GUEST_IDTR_LIMIT, idtr.limit);
    hv::vmwrite<unsigned __int64>(GUEST_GDTR_BASE, gdtr.base_address);
    hv::vmwrite<unsigned __int64>(GUEST_IDTR_BASE, idtr.base_address);

    hv::vmwrite<unsigned __int64>(GUEST_CS_SELECTOR, __read_cs());
    hv::vmwrite<unsigned __int64>(GUEST_SS_SELECTOR, __read_ss());
    hv::vmwrite<unsigned __int64>(GUEST_DS_SELECTOR, __read_ds());
    hv::vmwrite<unsigned __int64>(GUEST_ES_SELECTOR, __read_es());
    hv::vmwrite<unsigned __int64>(GUEST_FS_SELECTOR, __read_fs());
    hv::vmwrite<unsigned __int64>(GUEST_GS_SELECTOR, __read_gs());
    hv::vmwrite<unsigned __int64>(GUEST_TR_SELECTOR, __read_tr());
    hv::vmwrite<unsigned __int64>(GUEST_LDTR_SELECTOR, __read_ldtr());

    hv::vmwrite<unsigned __int64>(GUEST_CS_BASE, segment_base(gdtr, __read_cs()));
    hv::vmwrite<unsigned __int64>(GUEST_SS_BASE, segment_base(gdtr, __read_ss()));
    hv::vmwrite<unsigned __int64>(GUEST_DS_BASE, segment_base(gdtr, __read_ds()));
    hv::vmwrite<unsigned __int64>(GUEST_ES_BASE, segment_base(gdtr, __read_es()));
    hv::vmwrite<unsigned __int64>(GUEST_FS_BASE, __readmsr(IA32_FS_BASE));
    hv::vmwrite<unsigned __int64>(GUEST_GS_BASE, __readmsr(IA32_GS_BASE));
    hv::vmwrite<unsigned __int64>(GUEST_TR_BASE, segment_base(gdtr, __read_tr()));
    hv::vmwrite<unsigned __int64>(GUEST_LDTR_BASE, segment_base(gdtr, __read_ldtr()));

    hv::vmwrite<unsigned __int64>(GUEST_CS_LIMIT, __segmentlimit(__read_cs()));
    hv::vmwrite<unsigned __int64>(GUEST_SS_LIMIT, __segmentlimit(__read_ss()));
    hv::vmwrite<unsigned __int64>(GUEST_DS_LIMIT, __segmentlimit(__read_ds()));
    hv::vmwrite<unsigned __int64>(GUEST_ES_LIMIT, __segmentlimit(__read_es()));
    hv::vmwrite<unsigned __int64>(GUEST_FS_LIMIT, __segmentlimit(__read_fs()));
    hv::vmwrite<unsigned __int64>(GUEST_GS_LIMIT, __segmentlimit(__read_gs()));
    hv::vmwrite<unsigned __int64>(GUEST_TR_LIMIT, __segmentlimit(__read_tr()));
    hv::vmwrite<unsigned __int64>(GUEST_LDTR_LIMIT, __segmentlimit(__read_ldtr()));

    hv::vmwrite<unsigned __int64>(GUEST_CS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_cs()).flags);
    hv::vmwrite<unsigned __int64>(GUEST_SS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_ss()).flags);
    hv::vmwrite<unsigned __int64>(GUEST_DS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_ds()).flags);
    hv::vmwrite<unsigned __int64>(GUEST_ES_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_es()).flags);
    hv::vmwrite<unsigned __int64>(GUEST_FS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_fs()).flags);
    hv::vmwrite<unsigned __int64>(GUEST_GS_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_gs()).flags);
    hv::vmwrite<unsigned __int64>(GUEST_TR_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_tr()).flags);
    hv::vmwrite<unsigned __int64>(GUEST_LDTR_ACCESS_RIGHTS, segment_access((unsigned __int8*)gdtr.base_address, __read_ldtr()).flags);

    hv::vmwrite<unsigned __int64>(GUEST_CR0, __readcr0());
    hv::vmwrite<unsigned __int64>(CR0_READ_SHADOW, __readcr0());
    hv::vmwrite<unsigned __int64>(CR0_GUEST_HOST_MASK, 0xFFFFFFFF'FFFFFFFF);
    hv::vmwrite<unsigned __int64>(GUEST_CR3, __readcr3());
    hv::vmwrite<unsigned __int64>(CR3_TARGET_COUNT, 1);
    hv::vmwrite<unsigned __int64>(CR3_TARGET_VALUE0, hv::get_system_directory_table_base());
    hv::vmwrite<unsigned __int64>(GUEST_CR4, __readcr4());
    hv::vmwrite<unsigned __int64>(CR4_READ_SHADOW, __readcr4() & ~0x2000);
    hv::vmwrite<unsigned __int64>(CR4_GUEST_HOST_MASK, 0x2000);

    hv::vmwrite<unsigned __int64>(GUEST_DR7, __readdr(7));
    hv::vmwrite<unsigned __int64>(GUEST_RFLAGS, __readeflags());
    hv::vmwrite<void*>(GUEST_RSP, 0);
    hv::vmwrite<void*>(GUEST_RIP, 0);

    hv::vmwrite<unsigned __int64>(GUEST_DEBUG_CONTROL, 0);
    hv::vmwrite<unsigned __int64>(GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
    hv::vmwrite<unsigned __int64>(GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));
    hv::vmwrite<unsigned __int64>(GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
    hv::vmwrite<unsigned __int64>(GUEST_PAT, __readmsr(IA32_PAT));
    hv::vmwrite<unsigned __int64>(GUEST_PERF_GLOBAL_CONTROL, __readmsr(IA32_PERF_GLOBAL_CTRL));
    hv::vmwrite<unsigned __int64>(GUEST_EFER, __readmsr(IA32_EFER));

    hv::vmwrite<unsigned __int64>(GUEST_ACTIVITY_STATE, vmx_active);
    hv::vmwrite<unsigned __int64>(GUEST_INTERRUPTIBILITY_STATE, 0);
    hv::vmwrite<unsigned __int64>(GUEST_SM_BASE, 0);
    hv::vmwrite<unsigned __int64>(GUEST_PENDING_DEBUG_EXCEPTION, 0);
    hv::vmwrite<unsigned __int64>(GUEST_VMX_PREEMPTION_TIMER_VALUE, MAXULONG64);
}

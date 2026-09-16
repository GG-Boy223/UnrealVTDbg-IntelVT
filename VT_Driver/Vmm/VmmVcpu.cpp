#include "VmmInternal.h"

//分配vcpu结构内存
bool init_vcpu(__vcpu* vcpu) // 为单个VCPU分配位图和EPT状态等基础资源，满足VMCS依赖
{
	outFile("---------- VCPU基础资源初始化开始：VCPU=0x%p ----------", vcpu);

	//vcpu->vmm_stack = allocate_pool<void*>(VMM_STACK_SIZE);
	//if (vcpu->vmm_stack == nullptr)
	//{
	//	LogError("vmm stack could not be allocated");
	//	return false;
	//}
	//RtlSecureZeroMemory(vcpu->vmm_stack, VMM_STACK_SIZE);

	vcpu->vcpu_bitmaps.io_bitmap_a = allocate_page_aligned_pool<unsigned __int8*>(PAGE_SIZE); // 分配IO位图A（端口0-0x7FFF），用于控制IO拦截
	if (vcpu->vcpu_bitmaps.io_bitmap_a == nullptr)
	{
		outFile("[失败][VCPU] IO Bitmap A分配失败：VCPU=0x%p", vcpu);
		return false; // 返回失败避免后续VMCS引用空指针
	}
	RtlSecureZeroMemory(vcpu->vcpu_bitmaps.io_bitmap_a, PAGE_SIZE); // 清零位图，默认不拦截IO，减少性能损耗
	vcpu->vcpu_bitmaps.io_bitmap_a_physical = MmGetPhysicalAddress(vcpu->vcpu_bitmaps.io_bitmap_a).QuadPart; // 记录物理地址供VMCS使用
	outFile("[成功][VCPU] IO Bitmap A分配完成：VA=0x%p PA=0x%llX", vcpu->vcpu_bitmaps.io_bitmap_a, vcpu->vcpu_bitmaps.io_bitmap_a_physical);

	vcpu->vcpu_bitmaps.io_bitmap_b = allocate_page_aligned_pool<unsigned __int8*>(PAGE_SIZE); // 分配IO位图B（端口0x8000-0xFFFF）
	if (vcpu->vcpu_bitmaps.io_bitmap_b == nullptr)
	{
		outFile("[失败][VCPU] IO Bitmap B分配失败：VCPU=0x%p", vcpu);
		return false; // 返回失败避免后续VMCS引用空指针
	}
	RtlSecureZeroMemory(vcpu->vcpu_bitmaps.io_bitmap_b, PAGE_SIZE); // 清零位图，默认不拦截IO
	vcpu->vcpu_bitmaps.io_bitmap_b_physical = MmGetPhysicalAddress(vcpu->vcpu_bitmaps.io_bitmap_b).QuadPart; // 记录物理地址
	outFile("[成功][VCPU] IO Bitmap B分配完成：VA=0x%p PA=0x%llX", vcpu->vcpu_bitmaps.io_bitmap_b, vcpu->vcpu_bitmaps.io_bitmap_b_physical);

	//
	// Allocate ept state structure
	//
	vcpu->ept_state = allocate_pool<__ept_state>(); // 分配EPT状态结构，保存EPT页表与钩子列表
	if (vcpu->ept_state == nullptr)
	{
		outFile("[失败][VCPU] EPT状态结构分配失败：VCPU=0x%p", vcpu);
		return false; // 返回失败避免无EPT运行
	}
	RtlSecureZeroMemory(vcpu->ept_state, sizeof(__ept_state)); // 清零EPT状态，避免残留指针
	InitializeListHead(&vcpu->ept_state->hooked_page_list); // 初始化EPT hook列表头，便于后续管理
	vcpu->ept_state->hooked_page_list_lock = 0; // 初始化每VCPU的EPT hook列表锁。
	outFile("[成功][VCPU] EPT状态结构分配完成：VA=0x%p", vcpu->ept_state);

	RtlSecureZeroMemory(&vcpu->host_tss, sizeof(task_state_segment_64)); // 清零宿主TSS，避免旧值影响任务切换
	RtlSecureZeroMemory(&vcpu->host_gdt, sizeof(segment_descriptor_32) * HOST_GDT_DESCRIPTOR_COUNT); // 清零宿主GDT缓冲，后续填充
	RtlSecureZeroMemory(&vcpu->host_idt, sizeof(segment_descriptor_interrupt_gate_64) * HOST_IDT_DESCRIPTOR_COUNT); // 清零宿主IDT缓冲，后续填充

	outFile("---------- VCPU基础资源初始化完成：VCPU=0x%p ----------", vcpu);

	return true;
}

// 在目标逻辑处理器上构建该核的 EPT 页表。
// IA32_VMX_EPT_VPID_CAP（1GB 大页支持）、CPUID.80000008H（MAXPHYADDR）、MTRR 都是每逻辑处理器
// 状态，所以 EPT 必须由它所属的那个核来构建：在一个核上替所有核统一建表，遇到大小核
//（P-core/E-core）能力不一致时会写出该核并不支持的条目（例如 1GB EPT 大页）。
bool init_vcpu_ept(__vcpu* vcpu)
{
	outFile("[进行中][CPU%u][VCPU] 开始初始化EPT结构：VCPU=0x%p", KeGetCurrentProcessorNumber(), vcpu);

	if (ept::initialize(*vcpu->ept_state) == false)
	{
		outFile("[失败][CPU%u][VCPU] EPT结构初始化失败：VCPU=0x%p", KeGetCurrentProcessorNumber(), vcpu);
		return false; // 返回失败避免无EPT进入VMX
	}

	outFile("[成功][CPU%u][VCPU] EPT结构初始化完成：EPTP=0x%llX MappedPDPTE=%u MappedPML4=%u",
		KeGetCurrentProcessorNumber(),
		vcpu->ept_state->ept_pointer != nullptr ? vcpu->ept_state->ept_pointer->all : 0,
		vcpu->ept_state->mapped_pdpte_count,
		vcpu->ept_state->mapped_pml4_count);

	return true;
}

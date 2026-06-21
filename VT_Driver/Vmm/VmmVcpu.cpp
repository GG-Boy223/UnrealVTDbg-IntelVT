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

	vcpu->vcpu_bitmaps.io_bitmap_a = allocate_pool<unsigned __int8*>(PAGE_SIZE); // 分配IO位图A（端口0-0x7FFF），用于控制IO拦截
	if (vcpu->vcpu_bitmaps.io_bitmap_a == nullptr)
	{
		outFile("[失败][VCPU] IO Bitmap A分配失败：VCPU=0x%p", vcpu);
		return false; // 返回失败避免后续VMCS引用空指针
	}
	RtlSecureZeroMemory(vcpu->vcpu_bitmaps.io_bitmap_a, PAGE_SIZE); // 清零位图，默认不拦截IO，减少性能损耗
	vcpu->vcpu_bitmaps.io_bitmap_a_physical = MmGetPhysicalAddress(vcpu->vcpu_bitmaps.io_bitmap_a).QuadPart; // 记录物理地址供VMCS使用
	outFile("[成功][VCPU] IO Bitmap A分配完成：VA=0x%p PA=0x%llX", vcpu->vcpu_bitmaps.io_bitmap_a, vcpu->vcpu_bitmaps.io_bitmap_a_physical);

	vcpu->vcpu_bitmaps.io_bitmap_b = allocate_pool<unsigned __int8*>(PAGE_SIZE); // 分配IO位图B（端口0x8000-0xFFFF）
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
	outFile("[成功][VCPU] EPT状态结构分配完成：VA=0x%p", vcpu->ept_state);

	RtlSecureZeroMemory(&vcpu->host_tss, sizeof(task_state_segment_64)); // 清零宿主TSS，避免旧值影响任务切换
	RtlSecureZeroMemory(&vcpu->host_gdt, sizeof(segment_descriptor_32) * HOST_GDT_DESCRIPTOR_COUNT); // 清零宿主GDT缓冲，后续填充
	RtlSecureZeroMemory(&vcpu->host_idt, sizeof(segment_descriptor_interrupt_gate_64) * HOST_IDT_DESCRIPTOR_COUNT); // 清零宿主IDT缓冲，后续填充

	//
	// Initialize ept structure
	// 初始化 ept 结构
	//
	outFile("[进行中][VCPU] 开始初始化EPT结构：VCPU=0x%p", vcpu);
	if (ept::initialize(*vcpu->ept_state) == false) // 初始化EPT页表与控制字段，确保硬件可用
	{
		outFile("[失败][VCPU] EPT结构初始化失败：VCPU=0x%p", vcpu);
		return false; // 返回失败避免无EPT进入VMX
	}
	outFile("[成功][VCPU] EPT结构初始化完成：EPTP=0x%llX MappedPDPTE=%u MappedPML4=%u",
		vcpu->ept_state->ept_pointer != nullptr ? vcpu->ept_state->ept_pointer->all : 0,
		vcpu->ept_state->mapped_pdpte_count,
		vcpu->ept_state->mapped_pml4_count);

	outFile("---------- VCPU基础资源初始化完成：VCPU=0x%p ----------", vcpu);

	return true;
}

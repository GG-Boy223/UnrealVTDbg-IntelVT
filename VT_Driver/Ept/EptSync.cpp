#include "../Driver.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../msr.h"
#include "../AllocateMem.h"
#include "../vmx.h"
#include "../hypervisor_routines.h"
#include "../Vmcs/VmcsApi.h"

namespace ept
{
	/// <summary>
	/// Swap physcial pages and invalidate tlb
	/// 交换物理页面并刷新tlb
	/// </summary>
	/// <param name="entry_address"> Pointer to page table entry which we want to change </param>
	/// <param name="entry_value"> Pointer to page table entry which we want use to change </param>
	/// <param name="invalidation_type"> Specifiy if we want to invalidate single context or all contexts  </param>
	void swap_pml1_and_invalidate_tlb(__ept_state& ept_state, __ept_pte* entry_address, __ept_pte entry_value, invept_type invalidation_type)
	{
		// 早期设计模式是所有cpu核共用一个ept，故而要上锁
		// 目前的设计是每个cpu独享一份ept页表，故无需上锁
		// Acquire the lock
		//spinlock::lock(&g_vmm_context.pml_lock);

		// Set the value
		entry_address->all = entry_value.all;

		// Invalidate the cache
		if (invalidation_type == invept_single_context)
		{
			_mm_mfence();
			invept_single_context_func(ept_state.ept_pointer->all);
		}
		else
		{
			_mm_mfence();
			invept_all_contexts_func();
		}

		// Release the lock
		//spinlock::unlock(&g_vmm_context.pml_lock);
	}
}

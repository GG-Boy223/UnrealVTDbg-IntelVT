#pragma once
#include "../Driver.h"
#include "../Globals.h"

// EPT TLB 同步和页表刷新
namespace ept
{
	/// <summary>
	/// Swap physcial pages and invalidate tlb
	/// 交换物理页面并刷新tlb
	/// </summary>
	/// <param name="entry_address"> Pointer to page table entry which we want to change </param>
	/// <param name="entry_value"> Pointer to page table entry which we want use to change </param>
	/// <param name="invalidation_type"> Specifiy if we want to invalidate single context or all contexts  </param>
	/// <param name="invalidate"> If true invalidates tlb after changning pte value </param>
	void swap_pml1_and_invalidate_tlb(__ept_state& ept_state, __ept_pte* entry_address, __ept_pte entry_value, invept_type invalidation_type);
}

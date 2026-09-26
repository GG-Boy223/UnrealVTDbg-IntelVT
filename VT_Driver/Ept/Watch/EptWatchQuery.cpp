#include "EptWatchInternal.h"

namespace ept
{
	//填充页事件所需信息
	void fillPageEventBasic(PageEventBasic* peb, guest_context* guest_registers)
	{
		peb->VirtualAddress = hv::vmread(GUEST_LINEAR_ADDRESS);
		peb->PhysicalAddress = hv::vmread(GUEST_PHYSICAL_ADDRESS);
		peb->CR3 = hv::vmread(GUEST_CR3);
		peb->FSBASE = hv::vmread(GUEST_FS_BASE);
		peb->GSBASE = hv::vmread(GUEST_GS_BASE);
		peb->FLAGS = hv::vmread(GUEST_RFLAGS);
		peb->RAX = guest_registers->rax;
		peb->RBX = guest_registers->rbx;
		peb->RCX = guest_registers->rcx;
		peb->RDX = guest_registers->rdx;
		peb->RSI = guest_registers->rsi;
		peb->RDI = guest_registers->rdi;
		peb->R8 = guest_registers->r8;
		peb->R9 = guest_registers->r9;
		peb->R10 = guest_registers->r10;
		peb->R11 = guest_registers->r11;
		peb->R12 = guest_registers->r12;
		peb->R13 = guest_registers->r13;
		peb->R14 = guest_registers->r14;
		peb->R15 = guest_registers->r15;
		peb->RBP = guest_registers->rbp;
		peb->RSP = hv::vmread(GUEST_RSP);
		peb->RIP = hv::vmread(GUEST_RIP);
		// These guest values are saved by the VM-exit assembly entry; VMX root mode DRs are host state.
		peb->DR0 = guest_registers->dr0;
		peb->DR1 = guest_registers->dr1;
		peb->DR2 = guest_registers->dr2;
		peb->DR3 = guest_registers->dr3;
		peb->DR6 = guest_registers->dr6;
		peb->DR7 = hv::vmread(GUEST_DR7);
		peb->CS = static_cast<unsigned short>(hv::vmread(GUEST_CS_SELECTOR));
		peb->DS = static_cast<unsigned short>(hv::vmread(GUEST_DS_SELECTOR));
		peb->ES = static_cast<unsigned short>(hv::vmread(GUEST_ES_SELECTOR));
		peb->SS = static_cast<unsigned short>(hv::vmread(GUEST_SS_SELECTOR));
		peb->FS = static_cast<unsigned short>(hv::vmread(GUEST_FS_SELECTOR));
		peb->GS = static_cast<unsigned short>(hv::vmread(GUEST_GS_SELECTOR));
	}

	void RestoreEptPageProperties(__ept_hooked_page_info* hooked_page_info, int ID)
	{
		//还原ept页的属性，让guest程序能正常执行，正常读写
		// 与 apply_active_watch_permissions_locked 同理：恢复整页权限时 entry 必须回到原页。
		// 伪页是 VMM 的私有副本，guest 对伪页的写入不会同步回原页；把 entry 停在伪页上还会让
		// 伪页里的隐藏断点字节继续生效。
		hooked_page_info->entry_address->read = 1;
		hooked_page_info->entry_address->write = 1;
		hooked_page_info->entry_address->execute = 1;
		hooked_page_info->entry_address->page_frame_number = hooked_page_info->pfn_of_hooked_page;
		hooked_page_info->ID = ID;
		hv::set_mtf(true); //开启mtf
	}

	// 调用方必须持有 eptWatchList_lock；本函数只读表，不自行加锁。
	//判断是否是监视的地址
	int ept_isWatchAddress(unsigned __int64 guest_physical_adddress, int ID)
	{
		return (ept_watch_entry_is_active(eptWatchList[ID]) &&
			((guest_physical_adddress >= eptWatchList[ID].PhysicalAddress) &&
			(guest_physical_adddress < eptWatchList[ID].PhysicalAddress + eptWatchList[ID].Size)));
	}

	// 调用方必须持有 eptWatchList_lock；本函数只读表，不自行加锁。
	//判断是否是监视的页
	int ept_isWatchPage(unsigned __int64 pfn, int ID)
	{
		//比较页帧
		return (ept_watch_entry_is_active(eptWatchList[ID]) && (GET_PFN(eptWatchList[ID].PhysicalAddress) == pfn));
	}

	// 调用方必须持有 eptWatchList_lock；本函数遍历全局 watch 表。
	int ept_getWatchID(unsigned __int64 guest_physical_adddress)
	{
		unsigned __int64 pfn = GET_PFN(guest_physical_adddress); //获取页帧
		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if (ept_isWatchPage(pfn, i))
			{
				return i; //返回空闲 ID
			}
		}

		return -1; // 没有空闲项
	}

	void clear_detected_breakpoint_for_watch_locked(unsigned __int64 virtual_address)
	{
		if (virtual_address == NULL ||
			g_vmm_context.vcpu == nullptr ||
			g_vmm_context.processor_count == 0)
		{
			return;
		}

		for (unsigned int i = 0; i < g_vmm_context.processor_count; i++)
		{
			__vcpu* vcpu = &g_vmm_context.vcpu[i];
			if (vcpu->breakpoint_detected == virtual_address)
			{
				vcpu->breakpoint_detected = NULL;
				vcpu->breakpoint_debug_register = -1;
				vcpu->Cid = { 0 };
			}
		}
	}
}

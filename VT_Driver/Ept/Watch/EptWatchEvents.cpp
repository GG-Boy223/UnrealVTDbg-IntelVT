#include "EptWatchInternal.h"

namespace ept
{
	static bool is_watch_access_match(const EPTWatchEntry& entry, __ept_violation ept_violation)
	{
		if (entry.Type == EPTW_WRITE)
		{
			return ept_violation.write_access != 0;
		}

		if (entry.Type == EPTW_READWRITE)
		{
			return ept_violation.read_access != 0 || ept_violation.write_access != 0;
		}

		if (entry.Type == EPTW_EXECUTE)
		{
			return ept_violation.execute_access != 0;
		}

		return false;
	}

	static bool is_watch_entry_exact_physical_match_locked(const EPTWatchEntry& entry, unsigned __int64 guest_physical_address)
	{
		if (entry.Size <= 0 || guest_physical_address < entry.PhysicalAddress)
		{
			return false;
		}

		return guest_physical_address - entry.PhysicalAddress < static_cast<unsigned __int64>(entry.Size);
	}

	static int find_exact_non_int3_watch_id_for_ept_event_locked(unsigned __int64 guest_physical_address, __ept_violation ept_violation)
	{
		const unsigned __int64 pfn = GET_PFN(guest_physical_address);

		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if (!ept_watch_entry_is_active(eptWatchList[i]) ||
				eptWatchList[i].bpType == 3 ||
				GET_PFN(eptWatchList[i].PhysicalAddress) != pfn ||
				!is_watch_access_match(eptWatchList[i], ept_violation))
			{
				continue;
			}

			if (is_watch_entry_exact_physical_match_locked(eptWatchList[i], guest_physical_address))
			{
				return i; //返回空闲 ID
			}
		}

		return -1; // 没有空闲项
	}

	static int find_int3_page_watch_id_locked(unsigned __int64 guest_physical_address)
	{
		const unsigned __int64 pfn = GET_PFN(guest_physical_address);

		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if (ept_watch_entry_is_active(eptWatchList[i]) &&
				eptWatchList[i].bpType == 3 &&
				GET_PFN(eptWatchList[i].PhysicalAddress) == pfn)
			{
				return i; //返回空闲 ID
			}
		}

		return -1; // 没有空闲项
	}

	static bool is_cached_watch_id_valid_locked(int ID, unsigned __int64 guest_physical_address, __ept_violation ept_violation)
	{
		if (!is_watch_id_valid(ID) ||
			!ept_watch_entry_is_active(eptWatchList[ID]) ||
			GET_PFN(eptWatchList[ID].PhysicalAddress) != GET_PFN(guest_physical_address))
		{
			return false;
		}

		if (eptWatchList[ID].bpType == 3) //int3
		{
			const int exactWatchID = find_exact_non_int3_watch_id_for_ept_event_locked(guest_physical_address, ept_violation);
			return exactWatchID == -1 && (ept_violation.read_access || ept_violation.write_access);
		}

		if (!is_watch_access_match(eptWatchList[ID], ept_violation))
		{
			return false;
		}

		if (is_watch_entry_exact_physical_match_locked(eptWatchList[ID], guest_physical_address))
		{
			return true;
		}

		if ((ept_violation.read_access || ept_violation.write_access) &&
			find_int3_page_watch_id_locked(guest_physical_address) != -1)
		{
			return false;
		}

		const int exactWatchID = find_exact_non_int3_watch_id_for_ept_event_locked(guest_physical_address, ept_violation);
		return exactWatchID == -1;
	}

	static int find_watch_id_for_ept_event_locked(unsigned __int64 guest_physical_address, __ept_violation ept_violation)
	{
		const unsigned __int64 pfn = GET_PFN(guest_physical_address);
		int int3PageWatchId = -1;
		int pageWatchId = -1;

		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if (!ept_watch_entry_is_active(eptWatchList[i]) || GET_PFN(eptWatchList[i].PhysicalAddress) != pfn)
			{
				continue;
			}

			if (eptWatchList[i].bpType == 3)
			{
				if (int3PageWatchId == -1)
				{
					int3PageWatchId = i;
				}
				continue;
			}

			if (!is_watch_access_match(eptWatchList[i], ept_violation))
			{
				continue;
			}

			if (is_watch_entry_exact_physical_match_locked(eptWatchList[i], guest_physical_address))
			{
				return i; //返回空闲 ID
			}

			if (pageWatchId == -1)
			{
				pageWatchId = i;
			}
		}

		// 隐藏 INT3 的 fake page 对读写不可访问；同页任意读写都需要临时切回原页。
		if (int3PageWatchId != -1 && (ept_violation.read_access || ept_violation.write_access))
		{
			return int3PageWatchId;
		}

		// 普通 EPT watch 会关闭整页权限；同页非目标地址也必须放行一条指令，避免反复 VM-exit 卡死。
		if (pageWatchId != -1)
		{
			return pageWatchId;
		}

		return -1; // 没有空闲项
	}

	bool ept_handleWatchEvent(__vcpu* vcpu, __ept_violation ept_violation, __ept_hooked_page_info* hooked_page_info,
		unsigned __int64 guest_physical_adddress, int& bpType)
	{
		//判断是否是被监视的页面
		if (hooked_page_info->pfn_of_hooked_page == GET_PFN(guest_physical_adddress))
		{
			hooked_page_info->ID = -1;
			hooked_page_info->isBp = false;
			hooked_page_info->isInt3 = false;

			spinlock::lock(&eptWatchList_lock);
			int ID = -1;
			if (is_cached_watch_id_valid_locked(hooked_page_info->last_watch_id, guest_physical_adddress, ept_violation))
			{
				ID = hooked_page_info->last_watch_id;
			}
			else
			{
				ID = find_watch_id_for_ept_event_locked(guest_physical_adddress, ept_violation);
			}

			if (ID == -1)
			{
				hooked_page_info->last_watch_id = -1;
				spinlock::unlock(&eptWatchList_lock);
				return false;
			}

			hooked_page_info->last_watch_id = ID;

			if (eptWatchList[ID].bpType == 3) //int3
			{
				//如果读到的是int3监视页则进行换页
				bpType = 3;
				hooked_page_info->ID = ID;
				if (ept_violation.write_access)
				{
					hooked_page_info->fake_page_needs_full_sync = true;
				}
				spinlock::unlock(&eptWatchList_lock);
				return true;
			}

			if (!is_watch_access_match(eptWatchList[ID], ept_violation))
			{
				spinlock::unlock(&eptWatchList_lock);
				return false;
			}

			const bool isTargetWatchAddress = ept_isWatchAddress(guest_physical_adddress, ID) != 0;
			//判断是否是虚拟化断点
			if ((eptWatchList[ID].Options & EPTO_VIRTUAL_BREAKPOINT) &&
				isTargetWatchAddress &&
				(guest_physical_adddress >= eptWatchList[ID].PhysicalAddress) &&
				(guest_physical_adddress < eptWatchList[ID].PhysicalAddress + eptWatchList[ID].Size))
			{
				//系统软件可以使用 TPR 寄存器暂时阻止低优先级中断，中断高优先级任务。
				//这是通过将要阻止的最高优先级中断的值加载到 TPR 来实现的。例如，将 TPR 的值加载为 9 (1001b)
				//将阻止优先级为 9 或更低的所有中断，同时允许识别优先级为 10 或更高的所有中断。将 TPR 加载为 0 可启用所有外部中断。将 TPR 加载为
				//15 (1111b) 可禁用所有外部中断。
				//Task Priority Register (CR8)
				unsigned __int64 CR8 = __readcr8();
				rflags rflags; // RFLAGS 结构，用于判断 TF（单步标志）等位
				rflags.flags = hv::vmread(GUEST_RFLAGS); // 读取来宾 RFLAGS，查看是否开启单步
				__vmx_interruptibility_state is = { hv::vmread(GUEST_INTERRUPTIBILITY_STATE) };
				int canBreak = (CR8 == 0) && (rflags.interrupt_enable_flag); //判断中断是否开启
				canBreak = canBreak && ((is.all & (1 << 0)) == 0);
				//判断触发ept违例之前的模式是内核还是用户模式
				const int kernelmode = hv::get_guest_cpl() == 0;

				if (!kernelmode && canBreak && eptWatchList[ID].LoopUserMode)
				{
					hooked_page_info->isBp = true;
					vcpu->breakpoint_detected = eptWatchList[ID].VirtualAddress;
				}
			}

			RestoreEptPageProperties(hooked_page_info, ID);
			spinlock::unlock(&eptWatchList_lock);
			return true;
		}

		return false;
	}
}

#include "EptWatchInternal.h"

namespace ept
{
	static int find_existing_int3_watch_id_locked(unsigned __int64 virtual_address, unsigned __int64 physical_address)
	{
		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if (eptWatchList[i].inuse &&
				eptWatchList[i].bpType == 3 &&
				eptWatchList[i].VirtualAddress == virtual_address &&
				eptWatchList[i].PhysicalAddress == physical_address)
			{
				return i; //返回空闲 ID
			}
		}

		return -1; // 没有空闲项
	}

	//设置隐形软件断点-多页监控
	bool set_hide_software_breakpoint(PVT_BREAK_POINT vmcallinfo)
	{
		int status = 0; // 预留状态变量（当前未使用）
		VT_BREAK_POINT tmp_vmcallinfo = { 0 }; // 本地缓冲保存断点请求

		if (sizeof(VT_BREAK_POINT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT))) // 读取来宾断点参数
		{
			return false;
		}

		unsigned int cpu_count = normalize_vcpu_count(tmp_vmcallinfo.CPUCount);
		if (cpu_count == 0 || !is_valid_watch_range(tmp_vmcallinfo.VirtualAddress, tmp_vmcallinfo.Size))
		{
			return false;
		}

		unsigned __int64 va = tmp_vmcallinfo.VirtualAddress;
		unsigned __int64 left = tmp_vmcallinfo.Size;
		int ids[EPTWATCHLISTSIZE] = { 0 };
		int id_count = 0;
		int firstID = -1;
		unsigned __int64 first_phys = 0;

		spinlock::lock(&eptWatchList_lock);

		while (left > 0)
		{
			unsigned __int64 page_offset = va & 0xFFF;
			unsigned __int64 chunk = 0x1000 - page_offset;
			if (chunk > left)
			{
				chunk = left;
			}

			unsigned __int64 physical_address = hv::get_physical_address(tmp_vmcallinfo.cr3, (PVOID)va);
			// 检测该函数是否存在于物理内存中
			// 如果该函数的物理地址为空
			// 说明该函数不存在于物理内存中
			//
			if (physical_address == NULL)
			{
				spinlock::unlock(&eptWatchList_lock);
				goto rollback;
			}

			int existingID = find_existing_int3_watch_id_locked(va, physical_address);
			if (existingID != -1)
			{
				if (firstID == -1)
				{
					firstID = existingID;
					first_phys = eptWatchList[existingID].PhysicalAddress;
				}

				va += chunk;
				left -= chunk;
				continue;
			}

			int ID = hv::getIdleWatchID();
			if (ID == -1)
			{
				spinlock::unlock(&eptWatchList_lock);
				goto rollback;
			}

			if (firstID == -1)
			{
				firstID = ID;
				first_phys = physical_address;
			}

			if (id_count >= EPTWATCHLISTSIZE)
			{
				spinlock::unlock(&eptWatchList_lock);
				goto rollback;
			}
			ids[id_count++] = ID;

			eptWatchList[ID].Size = static_cast<int>(chunk);
			eptWatchList[ID].cr3 = tmp_vmcallinfo.cr3;
			eptWatchList[ID].VirtualAddress = va;
			eptWatchList[ID].PhysicalAddress = physical_address;
			eptWatchList[ID].LoopUserMode = tmp_vmcallinfo.LoopUserMode;
			eptWatchList[ID].OriginalByte = tmp_vmcallinfo.OriginalBytes;
			eptWatchList[ID].Type = EPTW_READWRITE;
			eptWatchList[ID].Options = EPTO_VIRTUAL_BREAKPOINT;
			eptWatchList[ID].bpType = 3;
			eptWatchList[ID].inuse = 1;

			VT_BREAK_POINT page_info = tmp_vmcallinfo;
			page_info.VirtualAddress = va;
			page_info.Size = static_cast<int>(chunk);
			page_info.PhysicalAddress = physical_address;

			status = 0;
			//设置每个cpu
			for (unsigned int i = 0; i < cpu_count; i++)
			{
				__vcpu* vcpu = &g_vmm_context.vcpu[i];
				if (hide_cc_breakpoint(*vcpu->ept_state, page_info, physical_address, EPTW_READWRITE))
				{
					status++;
				}
			}

			if (status != static_cast<int>(cpu_count))
			{
				spinlock::unlock(&eptWatchList_lock);
				goto rollback;
			}

			va += chunk;
			left -= chunk;
		}

		tmp_vmcallinfo.watchid = firstID;
		tmp_vmcallinfo.PhysicalAddress = first_phys;

		if (sizeof(VT_BREAK_POINT) != hv::write_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT)))
		{
			spinlock::unlock(&eptWatchList_lock);
			goto rollback;
		}

		_mm_mfence();
		invept_all_contexts_func();
		spinlock::unlock(&eptWatchList_lock);
		return true;

	rollback:
		for (int i = 0; i < id_count; i++)
		{
			ept::ept_watch_deactivate(tmp_vmcallinfo, ids[i]); //卸载监视
		}
		return false;
	}
}

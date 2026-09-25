#include "EptWatchInternal.h"

namespace ept
{
	static int find_existing_int3_watch_id_locked(unsigned __int64 virtual_address, unsigned __int64 physical_address)
	{
		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if (ept_watch_entry_is_active(eptWatchList[i]) &&
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
		VT_BREAK_POINT tmp_vmcallinfo = { 0 }; // 本地缓冲保存断点请求

		if (sizeof(VT_BREAK_POINT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT))) // 读取来宾断点参数
		{
			return false;
		}

		unsigned int cpu_count = normalize_vcpu_count(tmp_vmcallinfo.CPUCount);
		if (cpu_count == 0 || tmp_vmcallinfo.Size != 1 ||
			!is_valid_watch_range(tmp_vmcallinfo.VirtualAddress, tmp_vmcallinfo.Size))
		{
			return false;
		}

		unsigned __int64 va = tmp_vmcallinfo.VirtualAddress;
		unsigned __int64 left = tmp_vmcallinfo.Size;
		int ids[EPTWATCHLISTSIZE] = { 0 };
		unsigned __int64 committed_cpu_masks[EPTWATCHLISTSIZE] = { 0 };
		int id_count = 0;
		int firstID = -1;
		unsigned __int64 first_phys = 0;
		bool guest_id_written = false;

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
				goto rollback;
			}

			spinlock::lock(&eptWatchList_lock);
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
				spinlock::unlock(&eptWatchList_lock);
				continue;
			}
			spinlock::unlock(&eptWatchList_lock);

			const int tracker_id = Cr3TrackerAcquire(
				tmp_vmcallinfo.cr3,
				va,
				physical_address);
			if (tracker_id == kInvalidCr3TrackerId)
			{
				goto rollback;
			}

			spinlock::lock(&eptWatchList_lock);
			int ID = hv::getIdleWatchID();
			if (ID == -1)
			{
				spinlock::unlock(&eptWatchList_lock);
				Cr3TrackerRelease(tracker_id);
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
				Cr3TrackerRelease(tracker_id);
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
			eptWatchList[ID].GroupId = firstID;
			eptWatchList[ID].Cr3TrackerId = tracker_id;
			eptWatchList[ID].inuse = EPT_WATCH_INSTALLING;
			spinlock::unlock(&eptWatchList_lock);

			VT_BREAK_POINT page_info = tmp_vmcallinfo;
			page_info.VirtualAddress = va;
			page_info.Size = static_cast<int>(chunk);
			page_info.PhysicalAddress = physical_address;

			// Guest 整页读取、split、pool 和跨 VCPU commit 均在表锁外执行。
			for (unsigned int i = 0; i < cpu_count; i++)
			{
				__vcpu* vcpu = &g_vmm_context.vcpu[i];
				if (hide_cc_breakpoint(*vcpu->ept_state, page_info, physical_address, EPTW_READWRITE))
				{
					committed_cpu_masks[id_count - 1] |= 1ull << i;
					continue;
				}

				goto rollback;
			}

			va += chunk;
			left -= chunk;
		}

		tmp_vmcallinfo.watchid = firstID;
		tmp_vmcallinfo.PhysicalAddress = first_phys;

		if (sizeof(VT_BREAK_POINT) != hv::write_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT)))
		{
			goto rollback;
		}
		guest_id_written = true;

		for (int index = 0; index < id_count; index++)
		{
			const int ID = ids[index];
			EPTWatchEntry entry = { 0 };
			spinlock::lock(&eptWatchList_lock);
			entry = eptWatchList[ID];
			spinlock::unlock(&eptWatchList_lock);

			for (unsigned int cpu = 0; cpu < cpu_count; cpu++)
			{
				if ((committed_cpu_masks[index] & (1ull << cpu)) == 0)
				{
					continue;
				}

				__vcpu* vcpu = &g_vmm_context.vcpu[cpu];
				if (!commit_hide_cc_breakpoint(
					*vcpu->ept_state,
					entry.VirtualAddress,
					entry.PhysicalAddress))
				{
					goto rollback;
				}
			}
		}

		spinlock::lock(&eptWatchList_lock);
		for (int index = 0; index < id_count; index++)
		{
			eptWatchList[ids[index]].inuse = EPT_WATCH_ACTIVE;
		}
		spinlock::unlock(&eptWatchList_lock);

		_mm_mfence();
		invept_all_contexts_func();
		return true;

	rollback:
		for (int index = id_count - 1; index >= 0; index--)
		{
			const int ID = ids[index];
			EPTWatchEntry entry = { 0 };
			int has = 0;
			spinlock::lock(&eptWatchList_lock);
			entry = eptWatchList[ID];
			for (int other = 0; other < EPTWATCHLISTSIZE; other++)
			{
				if (other != ID && ept_watch_entry_is_active(eptWatchList[other]) &&
					GET_PFN(eptWatchList[other].PhysicalAddress) == GET_PFN(entry.PhysicalAddress))
				{
					has = 1;
				}
			}
			spinlock::unlock(&eptWatchList_lock);

			for (unsigned int cpu = cpu_count; cpu > 0; cpu--)
			{
				const unsigned int cpu_index = cpu - 1;
				if ((committed_cpu_masks[index] & (1ull << cpu_index)) == 0)
				{
					continue;
				}

				__vcpu* vcpu = &g_vmm_context.vcpu[cpu_index];
				if (!ept_watch_deactivate_internal(vcpu,
					*vcpu->ept_state,
					entry.VirtualAddress,
					entry.PhysicalAddress,
					entry.Type,
					entry.bpType,
					entry.OriginalByte,
					has,
					ID))
				{
					KeBugCheckEx(
						0x00020001,
						0x45505457,
						4,
						ID,
						cpu_index);
				}
			}

			spinlock::lock(&eptWatchList_lock);
			const int tracker_id = eptWatchList[ID].Cr3TrackerId;
			eptWatchList[ID] = { 0 };
			spinlock::unlock(&eptWatchList_lock);
			Cr3TrackerRelease(tracker_id);
		}

		_mm_mfence();
		invept_all_contexts_func();
		if (guest_id_written)
		{
			tmp_vmcallinfo.watchid = -1;
			tmp_vmcallinfo.PhysicalAddress = 0;
			tmp_vmcallinfo.errorCode = 102;
			hv::write_guest_virtual_memory(
				vmcallinfo,
				&tmp_vmcallinfo,
					sizeof(VT_BREAK_POINT));
		}
		return false;
	}
}

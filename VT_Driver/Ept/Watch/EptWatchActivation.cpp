#include "EptWatchInternal.h"

namespace ept
{
	bool ept_watch_activate(
		VT_BREAK_POINT vmcallinfo,
		unsigned __int64 Type,
		PVT_BREAK_POINT guest_request,
		int& errorCode)
	{
		unsigned int cpu_count = normalize_vcpu_count(vmcallinfo.CPUCount);
		if (guest_request == nullptr ||
			cpu_count == 0 ||
			!is_valid_watch_range(vmcallinfo.VirtualAddress, vmcallinfo.Size))
		{
			errorCode = 99;
			return false;
		}

		unsigned __int64 va = vmcallinfo.VirtualAddress;
		unsigned __int64 left = vmcallinfo.Size;
		int firstID = -1;
		int ids[EPTWATCHLISTSIZE] = { 0 };
		unsigned __int64 installed_physical_addresses[EPTWATCHLISTSIZE] = { 0 };
		unsigned __int64 committed_cpu_masks[EPTWATCHLISTSIZE] = { 0 };
		int id_count = 0;

		while (left > 0)
		{
			unsigned __int64 page_offset = va & 0xFFF;
			unsigned __int64 chunk = 0x1000 - page_offset;
			if (chunk > left)
			{
				chunk = left;
			}

			unsigned __int64 physical_address = hv::get_physical_address(vmcallinfo.cr3, (PVOID)va);
			// 检测该函数是否存在于物理内存中
			// 如果该函数的物理地址为空
			// 说明该函数不存在于物理内存中
			//
			if (physical_address == NULL)
			{
				errorCode = 100;
				goto rollback;
			}

			const int tracker_id = Cr3TrackerAcquire(
				vmcallinfo.cr3,
				va,
				physical_address);
			if (tracker_id == kInvalidCr3TrackerId)
			{
				errorCode = 104;
				goto rollback;
			}

			spinlock::lock(&eptWatchList_lock);
			int ID = hv::getIdleWatchID();
			if (ID == -1)
			{
				errorCode = 101;
				spinlock::unlock(&eptWatchList_lock);
				Cr3TrackerRelease(tracker_id);
				goto rollback;
			}

			if (firstID == -1)
			{
				firstID = ID;
				vmcallinfo.PhysicalAddress = physical_address;
			}

			if (id_count >= EPTWATCHLISTSIZE)
			{
				errorCode = 103;
				spinlock::unlock(&eptWatchList_lock);
				Cr3TrackerRelease(tracker_id);
				goto rollback;
			}
			ids[id_count++] = ID;

			eptWatchList[ID].Size = static_cast<int>(chunk);
			eptWatchList[ID].cr3 = vmcallinfo.cr3;
			eptWatchList[ID].VirtualAddress = va;
			eptWatchList[ID].PhysicalAddress = physical_address;
			eptWatchList[ID].LoopUserMode = vmcallinfo.LoopUserMode;
			eptWatchList[ID].Type = Type;
			eptWatchList[ID].Options = EPTO_VIRTUAL_BREAKPOINT;
			eptWatchList[ID].bpType = 1;
			eptWatchList[ID].debugRegister = vmcallinfo.debugRegister;
			eptWatchList[ID].GroupId = firstID;
			eptWatchList[ID].Cr3TrackerId = tracker_id;
			eptWatchList[ID].inuse = EPT_WATCH_INSTALLING;
			installed_physical_addresses[id_count - 1] = eptWatchList[ID].PhysicalAddress;
			spinlock::unlock(&eptWatchList_lock);

			// EPT split、池操作和跨 VCPU commit 均在表锁外执行，VM-exit 热路径不会长时间自旋。
			for (unsigned int i = 0; i < cpu_count; i++)
			{
				__vcpu* vcpu = &g_vmm_context.vcpu[i];
				if (ept::ept_watch_activate_internal(*vcpu->ept_state, physical_address, Type))
				{
					committed_cpu_masks[id_count - 1] |= 1ull << i;
					continue;
				}

				errorCode = 102;
				goto rollback;
			}

			va += chunk;
			left -= chunk;
		}

		vmcallinfo.watchid = firstID;
		if (sizeof(VT_BREAK_POINT) != hv::write_guest_virtual_memory(
			guest_request,
			&vmcallinfo,
			sizeof(VT_BREAK_POINT)))
		{
			errorCode = 105;
			goto rollback;
		}

		spinlock::lock(&eptWatchList_lock);
		for (int index = 0; index < id_count; index++)
		{
			eptWatchList[ids[index]].inuse = EPT_WATCH_ACTIVE;
		}
		spinlock::unlock(&eptWatchList_lock);

		_mm_mfence();
		invept_all_contexts_func(); // 刷新 EPT 缓存，避免使用旧映射
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
				if (other != ID &&
					eptWatchList[other].GroupId != entry.GroupId &&
					ept_watch_entry_is_effective(eptWatchList[other]) &&
					GET_PFN(eptWatchList[other].PhysicalAddress) == GET_PFN(entry.PhysicalAddress))
				{
					has = eptWatchList[other].Type == entry.Type ? 2 : 1;
					if (has == 2)
					{
						break;
					}
				}
			}
			spinlock::unlock(&eptWatchList_lock);

			for (unsigned int cpu = 0; cpu < cpu_count; cpu++)
			{
				if ((committed_cpu_masks[index] & (1ull << cpu)) == 0)
				{
					continue;
				}

				bool page_already_rolled_back_on_cpu = false;
				for (int previous = index + 1; previous < id_count; previous++)
				{
					if (installed_physical_addresses[previous] ==
						installed_physical_addresses[index] &&
						(committed_cpu_masks[previous] & (1ull << cpu)) != 0)
					{
						page_already_rolled_back_on_cpu = true;
						break;
					}
				}

				if (page_already_rolled_back_on_cpu)
				{
					continue;
				}

				__vcpu* vcpu = &g_vmm_context.vcpu[cpu];
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
						3,
						ID,
						cpu);
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
		return false;
	}
}

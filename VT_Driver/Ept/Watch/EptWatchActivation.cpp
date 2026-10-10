#include "EptWatchInternal.h"

namespace ept
{
	bool ept_watch_activate(
		__vcpu* vcpu,
		VT_BREAK_POINT vmcallinfo,
		unsigned __int64 Type,
		PVT_BREAK_POINT guest_request,
		int& errorCode)
	{
		unsigned int cpu_count = normalize_vcpu_count(vmcallinfo.CPUCount);
		if (vcpu == nullptr ||
			vcpu->ept_state == nullptr ||
			guest_request == nullptr ||
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
		int id_count = 0;
		int committed_count = 0;
		bool reuse_existing = false;

		// HvVmxCore 式去重：同 (起始地址, 类型) 的 watch 已激活时，本次是另一个核的重复广播，
		// 复用已有 GroupId，只为当前核应用 EPT，不再新建 watch 条目。
		// 只调试单个目标进程，地址空间唯一，无需用 CR3 参与去重。
		spinlock::lock(&eptWatchList_lock);
		for (int i = 0; i < EPTWATCHLISTSIZE; i++)
		{
			if (ept_watch_entry_is_active(eptWatchList[i]) &&
				eptWatchList[i].VirtualAddress == vmcallinfo.VirtualAddress &&
				eptWatchList[i].Type == Type)
			{
				firstID = eptWatchList[i].GroupId;
				reuse_existing = true;
				break;
			}
		}
		spinlock::unlock(&eptWatchList_lock);

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

			if (reuse_existing)
			{
				goto skip_alloc;
			}

			int tracker_detail = kCr3TrackerFailureNone;
			const int tracker_id = Cr3TrackerAcquire(
				vmcallinfo.cr3,
				va,
				physical_address,
				vmcallinfo.pid,
				&tracker_detail);
			if (tracker_id == kInvalidCr3TrackerId)
			{
				errorCode = tracker_detail != kCr3TrackerFailureNone
					? 140 + tracker_detail
					: 104;
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
			eptWatchList[ID].pid = vmcallinfo.pid;
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
			spinlock::unlock(&eptWatchList_lock);

		skip_alloc:
			int activate_detail = EPT_WATCH_DETAIL_NONE;
			// HvVmxCore 式：只改当前核自己的 EPT，不再跨核遍历。
			if (!ept::ept_watch_activate_internal(*vcpu->ept_state, physical_address, Type, &activate_detail))
			{
				errorCode = ept_watch_detail_error(activate_detail);
				if (errorCode == 0)
				{
					errorCode = 102;
				}
				goto rollback;
			}
			committed_count++;

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

		if (!reuse_existing)
		{
			spinlock::lock(&eptWatchList_lock);
			for (int index = 0; index < id_count; index++)
			{
				eptWatchList[ids[index]].inuse = EPT_WATCH_ACTIVE;
			}
			spinlock::unlock(&eptWatchList_lock);
		}

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
					has = ept_watch_entries_share_kind(eptWatchList[other], entry) ? 2 : 1;
					if (has == 2)
					{
						break;
					}
				}
			}
			spinlock::unlock(&eptWatchList_lock);

			// HvVmxCore 式：只回滚当前核；未 commit 的最后一个条目只清记录，不回滚 EPT。
			if (index < committed_count)
			{
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
					KeBugCheckEx(0x00020001, 0x45505457, 3, ID, 0);
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

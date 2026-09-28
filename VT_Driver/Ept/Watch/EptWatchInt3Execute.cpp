#include "EptWatchInternal.h"

namespace ept
{
    bool ept_handle_int3_execute(
        __vcpu* vcpu,
        __ept_hooked_page_info* hooked_page_info,
        unsigned __int64 guest_physical_address)
    {
        if (vcpu == nullptr ||
            vcpu->ept_state == nullptr ||
            hooked_page_info == nullptr)
        {
            return false;
        }

        const unsigned __int64 pfn = GET_PFN(guest_physical_address);
        bool has_active_watch = false;
        int transitional_id = -1;
        int sync_id = -1; // 需要整页重新同步伪页时用来取 cr3 / 虚拟地址的 active INT3 watch

        spinlock::lock(&eptWatchList_lock);
        for (int index = 0; index < EPTWATCHLISTSIZE; index++)
        {
            EPTWatchEntry const& entry = eptWatchList[index];
            if (entry.bpType != 3 ||
                !ept_watch_entry_owns_hardware(entry) ||
                GET_PFN(entry.PhysicalAddress) != pfn)
            {
                continue;
            }

            const int offset = static_cast<int>(entry.VirtualAddress & 0xFFF);
            if (ept_watch_entry_is_active(entry))
            {
                hooked_page_info->fake_page_contents[offset] = 0xCC;
                has_active_watch = true;
                if (sync_id == -1)
                {
                    sync_id = index;
                }
                continue;
            }

            hooked_page_info->fake_page_contents[offset] = entry.OriginalByte;
            if (transitional_id == -1)
            {
                transitional_id = index;
            }
        }

        if (has_active_watch)
        {
            // 伪页是快照：隐藏态被写过后必须先整页重新同步，否则这次换入的就是过期指令流，
            // guest 会按旧代码执行（「单步几步后跳回断点」即由此而来，见 docs/23）。
            const bool needs_full_sync =
                !hooked_page_info->fake_page_synced ||
                hooked_page_info->fake_page_needs_full_sync;
            cr3 sync_cr3 = {};
            unsigned __int64 sync_virtual_address = 0;
            if (needs_full_sync)
            {
                sync_cr3.flags = eptWatchList[sync_id].cr3;
                sync_virtual_address = eptWatchList[sync_id].VirtualAddress;
            }

            spinlock::unlock(&eptWatchList_lock);

            if (needs_full_sync)
            {
                if (PAGE_SIZE != hv::read_guest_virtual_memory(
                        sync_cr3,
                        reinterpret_cast<void*>(ept::page_align_down(sync_virtual_address)),
                        &hooked_page_info->fake_page_contents,
                        PAGE_SIZE))
                {
                    // 读不到就保留脏标记，本次仍按现有快照放行让 guest 前向推进，
                    // 下一次命中继续尝试同步。
                    hooked_page_info->fake_page_synced = false;
                    hooked_page_info->fake_page_needs_full_sync = true;
                }
                else
                {
                    hooked_page_info->fake_page_synced = true;
                    hooked_page_info->fake_page_needs_full_sync = false;

                    // 整页同步会覆盖伪页里的 0xCC，必须把本页所有 active INT3 字节重新写回。
                    spinlock::lock(&eptWatchList_lock);
                    for (int index = 0; index < EPTWATCHLISTSIZE; index++)
                    {
                        if (ept_isWatchPage(pfn, index) && eptWatchList[index].bpType == 3)
                        {
                            hooked_page_info->fake_page_contents[eptWatchList[index].VirtualAddress & 0xFFF] = 0xCC;
                        }
                    }
                    spinlock::unlock(&eptWatchList_lock);
                }
            }

            swap_pml1_and_invalidate_tlb(
                *vcpu->ept_state,
                hooked_page_info->entry_address,
                hooked_page_info->changed_entry,
                invept_type::invept_single_context);
            return true;
        }

        if (transitional_id == -1)
        {
            spinlock::unlock(&eptWatchList_lock);
            return false;
        }

        hooked_page_info->ID = transitional_id;
        hooked_page_info->isInt3 = true;
        hooked_page_info->isBp = false;
        spinlock::unlock(&eptWatchList_lock);

        if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
        {
            // removing 已由删除事务置位：本次不能再切换表项或启用 MTF。
            // 但这里必须返回 false（交由调用方走 unhandled 路径），不能返回 true：
            // 返回 true 会让 VmExitEpt 直接 handled_return —— 表项仍是"不可执行"且没有任何
            // 状态推进，来宾会立刻重试同一条指令并再次进入本函数，该逻辑处理器被永久困在
            // VMX root（IF=0）。一旦有核卡在 root，删除断点走的 KeGenericCallDpc 全核屏障
            // 就永远等不到它，表现为"删除所有断点后调试器与被调试进程一起卡死"。
            // 返回 false 后由 VmExitEpt 的 unhandled 路径释放热引用、按页计数并本地 INVEPT
            // 让来宾重试：删除事务完成后重试即成功，条件持续存在时也会在 64 次后收敛为
            // 可诊断的 vmx_fatal_error。
            hv::set_mtf(false);
            return false;
        }

        // 一次性放行窗口：重试的访问可能是读也可能是写（隐藏态已把 write 关掉），
        // 所以这里给原页完整权限，MTF 收尾时再恢复隐藏态。
        hooked_page_info->original_entry.read = 1;
        hooked_page_info->original_entry.write = 1;
        hooked_page_info->original_entry.execute = 1;
        hv::set_mtf(true);
        swap_pml1_and_invalidate_tlb(
            *vcpu->ept_state,
            hooked_page_info->entry_address,
            hooked_page_info->original_entry,
            invept_type::invept_single_context);
        return true;
    }
}

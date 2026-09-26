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
            spinlock::unlock(&eptWatchList_lock);
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

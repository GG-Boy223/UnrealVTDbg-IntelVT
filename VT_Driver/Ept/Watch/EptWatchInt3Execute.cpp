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
        hooked_page_info->original_entry.execute = 1;
        spinlock::unlock(&eptWatchList_lock);

        if (!hv::mark_pending_mtf_page(vcpu, hooked_page_info))
        {
            return false;
        }

        hv::set_mtf(true);
        swap_pml1_and_invalidate_tlb(
            *vcpu->ept_state,
            hooked_page_info->entry_address,
            hooked_page_info->original_entry,
            invept_type::invept_single_context);
        return true;
    }
}

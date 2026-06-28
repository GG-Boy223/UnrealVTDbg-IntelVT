#include "GlobalsInternal.h"

namespace hv
{
    void restore_pending_mtf_guest_interruptibility(__vcpu* vcpu)
    {
        if (vcpu == nullptr || vcpu->ept_state == nullptr)
        {
            return;
        }

        if (vcpu->ept_state->mtf_restore_interruptibility)
        {
            hv::vmwrite(GUEST_INTERRUPTIBILITY_STATE, vcpu->ept_state->mtf_saved_interruptibility);
            vcpu->ept_state->mtf_restore_interruptibility = false;
            vcpu->ept_state->mtf_saved_interruptibility = 0;
        }
    }

    void clear_pending_mtf_metadata(__ept_state* ept_state)
    {
        if (ept_state == nullptr)
        {
            return;
        }

        ept_state->page_to_change = nullptr;
        ept_state->mtf_pending = false;
        ept_state->mtf_restore_interruptibility = false;
        ept_state->mtf_saved_interruptibility = 0;
    }

    void clear_pending_mtf_metadata(__vcpu* vcpu)
    {
        if (vcpu == nullptr)
        {
            return;
        }

        restore_pending_mtf_guest_interruptibility(vcpu);
        //clear_pending_mtf_metadata(vcpu->ept_state);
        clear_pending_mtf_metadata(vcpu->ept_state);
    }

    bool mark_pending_mtf_page(__vcpu* vcpu, __ept_hooked_page_info* hooked_page_info)
    {
        if (vcpu == nullptr || vcpu->ept_state == nullptr || hooked_page_info == nullptr)
        {
            return false;
        }

        __ept_state* ept_state = vcpu->ept_state;
        if (ept_state->mtf_pending &&
            ept_state->page_to_change != nullptr &&
            ept_state->page_to_change != hooked_page_info)
        {
            __ept_hooked_page_info* stale_page = ept_state->page_to_change;
            if (stale_page->entry_address != nullptr)
            {
                if (stale_page->Options == EPTO_HOOK_FUNCTION)
                {
                    stale_page->original_entry.execute = 0;
                    ept::swap_pml1_and_invalidate_tlb(*ept_state,
                        stale_page->entry_address,
                        stale_page->original_entry,
                        invept_type::invept_single_context);
                }
                else if (stale_page->Options == EPTO_VIRTUAL_BREAKPOINT)
                {
                    spinlock::lock(&eptWatchList_lock);
                    ept::apply_active_watch_permissions_locked(stale_page);
                    spinlock::unlock(&eptWatchList_lock);
                    _mm_mfence();
                    if (ept_state->ept_pointer != nullptr)
                    {
                        invept_single_context_func(ept_state->ept_pointer->all);
                    }
                }
            }

            restore_pending_mtf_guest_interruptibility(vcpu);
            clear_pending_mtf_metadata(ept_state);
        }

        ept_state->page_to_change = hooked_page_info;
        ept_state->mtf_pending = true;
        return true;
    }

    bool set_mtf_guest_interruptibility_block(__vcpu* vcpu)
    {
        if (vcpu == nullptr || vcpu->ept_state == nullptr)
        {
            return false;
        }

        __vmx_interruptibility_state interruptibility = { hv::vmread(GUEST_INTERRUPTIBILITY_STATE) };
        if (!vcpu->ept_state->mtf_restore_interruptibility)
        {
            vcpu->ept_state->mtf_saved_interruptibility = interruptibility.all;
            vcpu->ept_state->mtf_restore_interruptibility = true;
        }

        interruptibility.blocking_by_sti = 1;
        hv::vmwrite(GUEST_INTERRUPTIBILITY_STATE, interruptibility.all);
        return true;
    }
}

#include "GlobalsInternal.h"

namespace hv
{
    PKPCR current_guest_kpcr()
    {
        if (current_guest_cpl() == 0)
        {
            return reinterpret_cast<PKPCR>(vmread(GUEST_GS_BASE));
        }

        return reinterpret_cast<PKPCR>(__readmsr(IA32_KERNEL_GS_BASE));
    }

    size_t current_guest_ethread()
    {
        auto const kpcr = current_guest_kpcr();

        if (!kpcr)
        {
            return NULL;
        }

        size_t kprcb = NULL;
        read_guest_virtual_memory(ghv.system_cr3,
            reinterpret_cast<uint8_t*>(kpcr) + ghv.kpcr_pcrb_offset,
            &kprcb,
            sizeof(kprcb));

        if (!kprcb)
        {
            return NULL;
        }

        size_t current_thread = NULL;
        read_guest_virtual_memory(ghv.system_cr3,
            reinterpret_cast<uint8_t*>(kprcb) + ghv.kprcb_current_thread_offset,
            &current_thread,
            sizeof(current_thread));

        return current_thread;
    }

    struct BREAKPOINT_CLEAR_STATE
    {
        unsigned __int64 command;
        unsigned __int64 ThreadId;
        unsigned __int64 VirtualAddress;
        unsigned __int64 Cr3;
        int WatchId;
        int Reserved;
    };

    bool read_breakpoint_clear_state(void* vmcallinfo, BREAKPOINT_CLEAR_STATE& request)
    {
        RtlZeroMemory(&request, sizeof(request));

        if (vmcallinfo == nullptr)
        {
            return true;
        }

        return sizeof(BREAKPOINT_CLEAR_STATE) ==
            read_guest_virtual_memory(vmcallinfo, &request, sizeof(BREAKPOINT_CLEAR_STATE));
    }

    unsigned __int64 resolve_clear_state_pfn(BREAKPOINT_CLEAR_STATE const& request)
    {
        unsigned __int64 target_pfn = 0;

        if (request.WatchId >= 0 && request.WatchId < EPTWATCHLISTSIZE)
        {
            spinlock::lock(&eptWatchList_lock);
            if (eptWatchList[request.WatchId].inuse &&
                eptWatchList[request.WatchId].PhysicalAddress != NULL)
            {
                target_pfn = GET_PFN(eptWatchList[request.WatchId].PhysicalAddress);
            }
            spinlock::unlock(&eptWatchList_lock);
        }

        if (target_pfn == 0 &&
            request.Cr3 != 0 &&
            request.VirtualAddress != 0)
        {
            const unsigned __int64 physical_address =
                get_physical_address(request.Cr3, reinterpret_cast<PVOID>(request.VirtualAddress));
            if (physical_address != NULL)
            {
                target_pfn = GET_PFN(physical_address);
            }
        }

        return target_pfn;
    }

    bool vcpu_thread_matches(__vcpu const* vcpu, unsigned __int64 thread_id)
    {
        if (thread_id == 0)
        {
            return true;
        }

        return vcpu != nullptr &&
            reinterpret_cast<unsigned __int64>(vcpu->Cid.UniqueThread) == thread_id;
    }

    bool vcpu_breakpoint_matches(__vcpu const* vcpu, BREAKPOINT_CLEAR_STATE const& request)
    {
        if (vcpu == nullptr)
        {
            return false;
        }

        const bool has_thread = request.ThreadId != 0;
        const bool has_address = request.VirtualAddress != 0;
        const bool has_watch = request.WatchId >= 0;
        if (!has_thread && !has_address && !has_watch)
        {
            return true;
        }

        if (has_thread && !vcpu_thread_matches(vcpu, request.ThreadId))
        {
            return false;
        }

        if (has_address && vcpu->breakpoint_detected != request.VirtualAddress)
        {
            return false;
        }

        return has_thread || has_address;
    }

    bool page_to_change_matches(__vcpu const* vcpu,
        BREAKPOINT_CLEAR_STATE const& request,
        unsigned __int64 target_pfn)
    {
        if (vcpu == nullptr ||
            vcpu->ept_state == nullptr ||
            !vcpu->ept_state->mtf_pending ||
            vcpu->ept_state->page_to_change == nullptr)
        {
            return false;
        }

        const bool has_filter = request.ThreadId != 0 ||
            request.VirtualAddress != 0 ||
            request.WatchId >= 0;
        if (!has_filter)
        {
            return true;
        }

        if (request.ThreadId != 0 && vcpu_thread_matches(vcpu, request.ThreadId))
        {
            return true;
        }

        return target_pfn != 0 &&
            vcpu->ept_state->page_to_change->pfn_of_hooked_page == target_pfn;
    }

    bool clear_breakpoint_state(__vcpu* vcpu, void* vmcallinfo)
    {
        BREAKPOINT_CLEAR_STATE request = {};
        if (!read_breakpoint_clear_state(vmcallinfo, request))
        {
            return false;
        }

        const unsigned __int64 target_pfn = resolve_clear_state_pfn(request);

        auto clear_one_vcpu = [&](__vcpu* target_vcpu)
        {
            if (target_vcpu == nullptr)
            {
                return;
            }

            if (vcpu_breakpoint_matches(target_vcpu, request))
            {
                target_vcpu->breakpoint_detected = NULL;
                target_vcpu->Cid = { 0 };
            }

            if (page_to_change_matches(target_vcpu, request, target_pfn))
            {
                clear_pending_mtf_metadata(target_vcpu);
            }
        };

        if (g_vmm_context.vcpu != nullptr && g_vmm_context.processor_count != 0)
        {
            for (unsigned int i = 0; i < g_vmm_context.processor_count; i++)
            {
                clear_one_vcpu(&g_vmm_context.vcpu[i]);
            }
        }
        else
        {
            clear_one_vcpu(vcpu);
        }

        return true;
    }

    bool get_breakpoint_detected(__vcpu* vcpu, PBREAKPOINT_DETECTED vmcallinfo)
    {
        BREAKPOINT_DETECTED tmp_vmcallinfo = { 0 };

        if (sizeof(BREAKPOINT_DETECTED) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(BREAKPOINT_DETECTED)))
        {
            return false;
        }

        if (tmp_vmcallinfo.Cid.UniqueThread == 0)
        {
            return false;
        }

        __vcpu* detected_vcpu = nullptr;
        if (g_vmm_context.vcpu != nullptr && g_vmm_context.processor_count != 0)
        {
            for (unsigned int i = 0; i < g_vmm_context.processor_count; i++)
            {
                __vcpu* candidate = &g_vmm_context.vcpu[i];
                if (candidate->Cid.UniqueThread == (HANDLE)tmp_vmcallinfo.Cid.UniqueThread &&
                    candidate->breakpoint_detected != NULL)
                {
                    detected_vcpu = candidate;
                    break;
                }
            }
        }

        if (detected_vcpu == nullptr &&
            vcpu != nullptr &&
            vcpu->Cid.UniqueThread == (HANDLE)tmp_vmcallinfo.Cid.UniqueThread &&
            vcpu->breakpoint_detected != NULL)
        {
            detected_vcpu = vcpu;
        }

        if (detected_vcpu != nullptr)
        {
            tmp_vmcallinfo.breakpoint_detected = detected_vcpu->breakpoint_detected;

            if (sizeof(BREAKPOINT_DETECTED) != hv::write_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(BREAKPOINT_DETECTED)))
            {
                return false;
            }

            detected_vcpu->breakpoint_detected = NULL;
            detected_vcpu->Cid = { 0 };
            return true;
        }
        return false;
    }

    void inject_single_step(__vcpu* vcpu)
    {
        int kernelmode = hv::get_guest_cpl() == 0;
        if (!kernelmode)
        {
            PCLIENT_ID Cid = GuestCurrentThreadCid();
            if (Cid == nullptr)
            {
                outDebug("跳过 inject_single_step：ETHREAD.Cid 尚未就绪。\n");
                vcpu->Cid = { 0 };
                vcpu->breakpoint_detected = NULL;
                return;
            }

            vcpu->Cid.UniqueThread = Cid->UniqueThread;
            hv::inject_interruption(EXCEPTION_VECTOR_SINGLE_STEP, INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false);
        }
        else
        {
            vcpu->Cid = { 0 };
            vcpu->breakpoint_detected = NULL;
        }
    }

    int getIdleWatchID()
    {
        int i;
        for (i = 0; i < EPTWATCHLISTSIZE; i++)
        {
            if (eptWatchList[i].inuse == 0)
            {
                return i;
            }
        }
        return -1;
    }
}

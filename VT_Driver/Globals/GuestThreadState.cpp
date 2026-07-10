#include "GlobalsInternal.h"

namespace hv
{
    PKPCR current_guest_kpcr()
    {
        // GS base holds the KPCR when in ring-0
        if (current_guest_cpl() == 0)
        {
            return reinterpret_cast<PKPCR>(vmread(GUEST_GS_BASE)); // 内核态直接取 GS_BASE
        }

        // when in ring-3, the GS_SWAP contains the KPCR
        // 在 ring-3 中，GS_SWAP 包含 内核KPCR
        return reinterpret_cast<PKPCR>(__readmsr(IA32_KERNEL_GS_BASE)); // 用户态取 KERNEL_GS_BASE
    }

    // get the ETHREAD of the current guest
    // 获取guest里的当前线程对象
    size_t current_guest_ethread()
    {
        // KPCR
        auto const kpcr = current_guest_kpcr(); // 获取 KPCR

        if (!kpcr)
        {
            return NULL; // CR3 无效时无法转换
        }

        // KPCR::CurrentPrcb
        size_t kprcb = NULL;
        read_guest_virtual_memory(ghv.system_cr3,
            reinterpret_cast<uint8_t*>(kpcr) + ghv.kpcr_pcrb_offset,
            &kprcb,
            sizeof(kprcb));

        if (!kprcb)
        {
            return NULL; // CR3 无效时无法转换
        }

        // KPRCB::CurrentThread
        size_t current_thread = NULL;
        read_guest_virtual_memory(ghv.system_cr3,
            reinterpret_cast<uint8_t*>(kprcb) + ghv.kprcb_current_thread_offset,
            &current_thread,
            sizeof(current_thread));

        return current_thread; // 返回 ETHREAD 地址
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
            if (ept_watch_entry_is_active(eptWatchList[request.WatchId]) &&
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

    //返回触发的断点给guest调试器
    bool get_breakpoint_detected(__vcpu* vcpu, PBREAKPOINT_DETECTED vmcallinfo)
    {
        BREAKPOINT_DETECTED tmp_vmcallinfo = { 0 }; // 临时结构，用于读取来宾传入的请求

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

        // 兼容旧路径：如果全局 vcpu 数组尚未就绪，仍检查当前逻辑处理器。
        if (detected_vcpu == nullptr &&
            vcpu != nullptr &&
            vcpu->Cid.UniqueThread == (HANDLE)tmp_vmcallinfo.Cid.UniqueThread &&
            vcpu->breakpoint_detected != NULL)
        {
            detected_vcpu = vcpu;
        }

        if (detected_vcpu != nullptr)
        {
            tmp_vmcallinfo.breakpoint_detected = detected_vcpu->breakpoint_detected; // 回填触发地址

            if (sizeof(BREAKPOINT_DETECTED) != hv::write_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(BREAKPOINT_DETECTED)))
            {
                return false;
            }

            //这说明该断点事件即将被调试器处理，是时候将它移除了
            detected_vcpu->breakpoint_detected = NULL; // 清空触发地址，避免重复上报
            detected_vcpu->Cid = { 0 }; // 清空线程 ID 记录
            return true; // 成功返回
        }
        return false;
    }

    //向guest注入#DB事件
    void inject_single_step(__vcpu* vcpu)
    {
        //guest的模式如果是内核就不注入#DB
        int kernelmode = hv::get_guest_cpl() == 0; // 判断 CPL，内核态不注入以避免内核断点风险
        if (!kernelmode)
        {
            PCLIENT_ID Cid = GuestCurrentThreadCid(); // 获取当前线程 ID
            if (Cid == nullptr)
            {
                // 方案 2：Cid 还没从 Win10 sys 下发过来时，不继续记录线程 ID，
                // 也不继续走后续依赖该字段的逻辑，避免把 ETHREAD 地址误当成 CLIENT_ID。
                outDebug("跳过 inject_single_step：ETHREAD.Cid 尚未就绪。\n");
                vcpu->Cid = { 0 };
                vcpu->breakpoint_detected = NULL;
                return;
            }

            vcpu->Cid.UniqueThread = Cid->UniqueThread; //记录当前guest的线程id
            hv::inject_interruption(EXCEPTION_VECTOR_SINGLE_STEP, INTERRUPT_TYPE_HARDWARE_EXCEPTION, 0, false); // 注入单步异常到来宾
        }
        else
        {
            vcpu->Cid = { 0 };
            vcpu->breakpoint_detected = NULL;
        }
    }

    //获取空闲的id
    int getIdleWatchID()
    {
        int i; // 循环索引
        for (i = 0; i < EPTWATCHLISTSIZE; i++)
        {
            if (!ept_watch_entry_is_reserved(eptWatchList[i])) // 事务中的槽位也不可重复分配
            {
                return i; //返回空闲 ID
            }
        }
        return -1; // 没有空闲项
    }
}

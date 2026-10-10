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
            if (ept_watch_entry_is_effective(eptWatchList[request.WatchId]) &&
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

    // 按 watchid 解析该监视点记录中的虚拟地址；条目已释放时返回 0（视为无此过滤维度）。
    unsigned __int64 resolve_clear_state_watch_virtual_address(BREAKPOINT_CLEAR_STATE const& request)
    {
        unsigned __int64 watch_virtual_address = 0;

        if (request.WatchId >= 0 && request.WatchId < EPTWATCHLISTSIZE)
        {
            spinlock::lock(&eptWatchList_lock);
            if (ept_watch_entry_is_reserved(eptWatchList[request.WatchId]))
            {
                watch_virtual_address = eptWatchList[request.WatchId].VirtualAddress;
            }
            spinlock::unlock(&eptWatchList_lock);
        }

        return watch_virtual_address;
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

    bool vcpu_breakpoint_matches(
        __vcpu const* vcpu,
        BREAKPOINT_CLEAR_STATE const& request,
        unsigned __int64 watch_virtual_address)
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

        // watchid 也是过滤维度：旧逻辑在判断“有没有过滤维度”时使用了 has_watch，
        // 结算时却只承认 thread/address，导致仅带 watchid 的清理请求永远返回 false，
        // 一次性命中状态（breakpoint_detected / debugRegister / Cid）残留在 VCPU 上，
        // 后续 GET_BREAKPOINT 可能重复上报或 Step 状态卡死。
        if (has_watch &&
            watch_virtual_address != 0 &&
            vcpu->breakpoint_detected != watch_virtual_address)
        {
            return false;
        }

        return true;
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
        const unsigned __int64 watch_virtual_address =
            resolve_clear_state_watch_virtual_address(request);

        auto clear_one_vcpu = [&](__vcpu* target_vcpu)
        {
            if (target_vcpu == nullptr)
            {
                return;
            }

            if (vcpu_breakpoint_matches(target_vcpu, request, watch_virtual_address))
            {
                target_vcpu->breakpoint_detected = NULL;
                target_vcpu->breakpoint_debug_register = -1;
                target_vcpu->Cid = { 0 };
            }

            if (page_to_change_matches(target_vcpu, request, target_pfn))
            {
                clear_pending_mtf_metadata(target_vcpu);
            }
        };

        // 只清理当前 CPU 所属的 vCPU，不跨核改写其它 vCPU 的状态。
        //
        // 该 VMCALL 的发送方是用户态 Hook：Hook/vmx/VmxBroadcast.cpp:vmcall() 通过逐 CPU
        // 亲和性切换，在每个逻辑处理器上各执行一次 VMCALL；因此“清理本核”即等价于
        // “清理全部 CPU”，且天然互斥于本核的 MTF/EPT violation 处理器。
        //
        // 旧实现由第一个到达的 CPU 一次性遍历 g_vmm_context.vcpu[] 改写所有 vCPU：
        // 其它核此刻可能正在自己的 MTF/EPT violation 处理器里操作同一份
        // page_to_change / mtf_pending / mtf_reference_count，会造成引用计数下溢
        // （延迟回收永久失效）或丢失正在建立的 MTF 窗口。按 per-CPU 所有权收敛后，
        // 每个 vCPU 的状态只由它自己的处理器线程序列化修改。
        clear_one_vcpu(vcpu);

        return true;
    }

    namespace
    {
        bool query_breakpoint_detected(__vcpu* vcpu,
            PBREAKPOINT_DETECTED vmcallinfo,
            bool consume)
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

            if (detected_vcpu == nullptr)
            {
                return false;
            }

            tmp_vmcallinfo.breakpoint_detected = detected_vcpu->breakpoint_detected; // 回填触发地址
            tmp_vmcallinfo.debugRegister = detected_vcpu->breakpoint_debug_register;
            if (sizeof(BREAKPOINT_DETECTED) != hv::write_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(BREAKPOINT_DETECTED)))
            {
                return false;
            }

            if (consume)
            {
                //这说明该断点事件即将被调试器处理，是时候将它移除了
                detected_vcpu->breakpoint_detected = NULL; // 清空触发地址，避免重复上报
                detected_vcpu->breakpoint_debug_register = -1;
                detected_vcpu->Cid = { 0 }; // 清空线程 ID 记录
            }
            return true;
        }
    }

    //返回触发的断点给guest调试器，并消费一次性命中状态
    bool get_breakpoint_detected(__vcpu* vcpu, PBREAKPOINT_DETECTED vmcallinfo)
    {
        return query_breakpoint_detected(vcpu, vmcallinfo, true);
    }

    //读取触发的断点给guest调试器，不消费一次性命中状态
    bool peek_breakpoint_detected(__vcpu* vcpu, PBREAKPOINT_DETECTED vmcallinfo)
    {
        return query_breakpoint_detected(vcpu, vmcallinfo, false);
    }

}

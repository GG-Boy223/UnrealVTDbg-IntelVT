#include "GlobalsInternal.h"

namespace hv
{
    void inject_single_step(__vcpu* vcpu)
    {
        bool const kernelMode = get_guest_cpl() == 0;
        if (!kernelMode)
        {
            PCLIENT_ID cid = GuestCurrentThreadCid();
            if (cid == nullptr)
            {
                outDebug("跳过 inject_single_step：ETHREAD.Cid 尚未就绪。\n");
                vcpu->Cid = { 0 };
                vcpu->breakpoint_detected = NULL;
                vcpu->breakpoint_debug_register = -1;
                return;
            }

            vcpu->Cid.UniqueThread = cid->UniqueThread;
            // Intel event injection 不会替 guest 更新 DR6；快照必须与 #DB 一起投递。
            unsigned __int64 debugRegisterState = vcpu->vmexit_info.guest_registers->dr6;
            debugRegisterState &= ~((1ULL << 14) | 0xFULL);
            if (vcpu->breakpoint_debug_register >= 0 &&
                vcpu->breakpoint_debug_register < 4)
            {
                debugRegisterState |= 1ULL << vcpu->breakpoint_debug_register;
            }
            inject_interruption_with_debug_state(
                EXCEPTION_VECTOR_SINGLE_STEP,
                INTERRUPT_TYPE_HARDWARE_EXCEPTION,
                0,
                false,
                debugRegisterState);
            return;
        }

        vcpu->Cid = { 0 };
        vcpu->breakpoint_detected = NULL;
        vcpu->breakpoint_debug_register = -1;
    }

    // 调用方必须持有 eptWatchList_lock；本函数只做表内查找，不自行加锁。
    int getIdleWatchID()
    {
        for (int i = 0; i < EPTWATCHLISTSIZE; i++)
        {
            if (!ept_watch_entry_is_reserved(eptWatchList[i]))
            {
                return i;
            }
        }

        return -1;
    }
}

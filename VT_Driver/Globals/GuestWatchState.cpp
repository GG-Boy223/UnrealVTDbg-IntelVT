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
                return;
            }

            vcpu->Cid.UniqueThread = cid->UniqueThread;
            // Intel event injection 不会替 guest 更新 DR6，先清除旧 B0-B3/BS，避免把 synthetic watch 命中误判为硬件断点或 TF 单步。
            vcpu->vmexit_info.guest_registers->dr6 &= ~((1ULL << 14) | 0xFULL);
            inject_interruption(
                EXCEPTION_VECTOR_SINGLE_STEP,
                INTERRUPT_TYPE_HARDWARE_EXCEPTION,
                0,
                false);
            return;
        }

        vcpu->Cid = { 0 };
        vcpu->breakpoint_detected = NULL;
    }

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

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

#include "HypervisorInternal.h"

namespace hv
{
    unsigned __int64 swap_context()
    {
        auto const current_process = reinterpret_cast<uint8_t*>(IoGetCurrentProcess());
        unsigned __int64 current_cr3 = __readcr3();
        unsigned __int64 guest_cr3 =
            *reinterpret_cast<unsigned __int64*>(current_process + hv::ghv.kprocess_directory_table_base_offset);

        __writecr3(guest_cr3);
        return current_cr3;
    }

    void swap_context(__vcpu* vcpu, unsigned __int64 new_cr3)
    {
        vcpu->target_cr3.flags = new_cr3;
    }

    void restore_context(__vcpu* vcpu)
    {
        vcpu->target_cr3.flags = 0;
    }

    unsigned __int64 get_system_directory_table_base()
    {
        auto const system_process = reinterpret_cast<uint8_t*>(PsInitialSystemProcess);
        return *reinterpret_cast<unsigned __int64*>(system_process + hv::ghv.kprocess_directory_table_base_offset);
    }

    bool is_address_canonical(unsigned __int64 address)
    {
        if (address < NON_CANONICIAL_ADDRESS_END && address > NON_CANONICIAL_ADDRESS_START)
        {
            return false;
        }

        return true;
    }
}

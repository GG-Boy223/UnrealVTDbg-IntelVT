#pragma once

#include "../../Shared/VmcallReason.h"

namespace hv::vmcall_dispatch_policy
{
    constexpr bool IsEptMutation(unsigned __int64 command)
    {
        return command == VMCALL_HIDE_SOFTWARE_BREAKPOINT ||
            command == VMCALL_WATCH_WRITES ||
            command == VMCALL_WATCH_READS ||
            command == VMCALL_WATCH_EXECUTES ||
            command == VMCALL_WATCH_DELETE ||
            command == VMCALL_CLEAR_BREAKPOINT_STATE;
    }
}
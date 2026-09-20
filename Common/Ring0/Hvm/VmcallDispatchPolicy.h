#pragma once

#include "../../Shared/VmcallReason.h"

namespace hv::vmcall_dispatch_policy
{
    constexpr bool IsEptMutation(unsigned __int64 command)
    {
        return command == VMCALL_EPT_CC_HOOK ||
            command == VMCALL_EPT_INT1_HOOK ||
            command == VMCALL_EPT_RIP_HOOK ||
            command == VMCALL_EPT_HOOK_FUNCTION ||
            command == VMCALL_EPT_UNHOOK_FUNCTION ||
            command == VMCALL_HIDE_SOFTWARE_BREAKPOINT ||
            command == VMCALL_READ_SOFTWARE_BREAKPOINT ||
            command == VMCALL_READ_EPT_FAKE_PAGE_MEMORY ||
            command == VMCALL_SET_SOFTWARE_BREAKPOINT_BUFFER ||
            command == VMCALL_GET_SOFTWARE_BREAKPOINT_BUFFER ||
            command == VMCALL_WATCH_WRITES ||
            command == VMCALL_WATCH_READS ||
            command == VMCALL_WATCH_EXECUTES ||
            command == VMCALL_WATCH_DELETE ||
            command == VMCALL_CLEAR_BREAKPOINT_STATE;
    }
}

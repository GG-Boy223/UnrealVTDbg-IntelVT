#include "VmxInternal.h"


namespace
{
    struct BREAKPOINT_CLEAR_STATE
    {
        unsigned __int64 command;
        unsigned __int64 ThreadId;
        unsigned __int64 VirtualAddress;
        unsigned __int64 Cr3;
        int WatchId;
        int Reserved;
    };
}

bool clear_vt_breakpoint_state(
    unsigned __int64 threadId,
    unsigned __int64 virtualAddress,
    unsigned __int64 cr3,
    int watchId)
{
    BREAKPOINT_CLEAR_STATE request = {};
    request.command = VMCALL_CLEAR_BREAKPOINT_STATE;
    request.ThreadId = threadId;
    request.VirtualAddress = virtualAddress;
    request.Cr3 = cr3;
    request.WatchId = watchId;

    const bool success = vmcall(&request);
    if (!success)
    {
        // 旧日志包含 watchid，避免在日志文件暴露内部断点编号。
        // logger.Log("清理VT断点残留状态失败: tid=%llu addr=0x%llX watchid=%d", ...);
        logger.Log("清理VT断点残留状态失败: tid=%llu addr=0x%llX",
            threadId,
            virtualAddress);
    }
    return success;
}

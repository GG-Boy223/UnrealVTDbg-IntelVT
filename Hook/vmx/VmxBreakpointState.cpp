#include "VmxInternal.h"


namespace
{
    struct BREAKPOINT_CLEAR_STATE
    {
        unsigned __int64 command; //此成员必须放在结构体的第一位
        unsigned __int64 ThreadId;
        unsigned __int64 VirtualAddress; //断点地址
        unsigned __int64 Cr3;
        int WatchId;
        int Reserved;
    };
}

//清理 VT 侧断点残留状态，不改变 watch 表和现有结构体布局
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

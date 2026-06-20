#include "DriverInternal.h"

PCLIENT_ID GuestCurrentThreadCid()
{
    constexpr size_t InvalidOffset = static_cast<size_t>(0xFFFFFFFF);
    if (ethread_offset::Cid == 0 || ethread_offset::Cid == InvalidOffset)
    {
        return nullptr;
    }
    size_t Thread = hv::current_guest_ethread();
    size_t ptr_Cid = Thread + ethread_offset::Cid;
    return (PCLIENT_ID)ptr_Cid;
}

bool SetBreakpoint(PVT_BREAK_POINT vmcallinfo, unsigned __int64 Type)
{
    int errorCode = 0;
    VT_BREAK_POINT tmp_vmcallinfo = { 0 };

    if (sizeof(VT_BREAK_POINT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT)))
    {
        return false;
    }

    int outID = -1;
    if (ept::ept_watch_activate(tmp_vmcallinfo, Type, &outID, errorCode))
    {
        tmp_vmcallinfo.watchid = outID;

        if (sizeof(VT_BREAK_POINT) != hv::write_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT)))
        {
            return false;
        }
        return true;
    }
    else
    {
        tmp_vmcallinfo.errorCode = errorCode;

        if (sizeof(VT_BREAK_POINT) != hv::write_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT)))
        {
            return false;
        }
    }
    return false;
}

// 旧项目保留的禁用路径：删除断点时只把 ept_watch_deactivate 返回 0 视为成功。
// 为什么注释：当前实现把 0 和 2 都视为成功，兼容“已经不存在/已清理”的幂等删除结果，避免用户态本地记录无法回收。
// 放开的好处：语义更严格，只有明确注销成功才返回 true。
// 放开的坏处：如果 VT 侧已经清理 watch 表但返回 2，用户态会认为删除失败，导致 Hook 本地状态残留。
// 项目影响：断点删除路径需要幂等；旧路径会增加重复删除、进程退出清理时的误报。
// 结论：继续注释，保留当前 0/2 均成功的实现。
//bool RemoveBreakpoint(PVT_BREAK_POINT vmcallinfo)
//{
//    VT_BREAK_POINT tmp_vmcallinfo = { 0 };
//
//    if (sizeof(VT_BREAK_POINT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT)))
//    {
//        return false;
//    }
//
//    if (ept::ept_watch_deactivate(tmp_vmcallinfo, tmp_vmcallinfo.watchid) == 0)
//    {
//        return true;
//    }
//
//    return false;
//}

bool RemoveBreakpoint(PVT_BREAK_POINT vmcallinfo)
{
    VT_BREAK_POINT tmp_vmcallinfo = { 0 };

    if (sizeof(VT_BREAK_POINT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT)))
    {
        return false;
    }

    const int deactivate_status = ept::ept_watch_deactivate(tmp_vmcallinfo, tmp_vmcallinfo.watchid);
    if (deactivate_status == 0 || deactivate_status == 2)
    {
        return true;
    }

    return false;
}

void MyKeBugCheck(
    _In_ ULONG BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter1,
    _In_ ULONG_PTR BugCheckParameter2,
    _In_ ULONG_PTR BugCheckParameter3,
    _In_ ULONG_PTR BugCheckParameter4
)
{
    KeBugCheckEx(BugCheckCode, BugCheckParameter1, BugCheckParameter2, BugCheckParameter3, BugCheckParameter4);
}

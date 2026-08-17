#include "DriverInternal.h"

PCLIENT_ID GuestCurrentThreadCid() // 计算来宾当前线程的CLIENT_ID地址，便于识别触发事件的线程
{
    constexpr size_t InvalidOffset = static_cast<size_t>(0xFFFFFFFF);
    // 方案 2：在 Win10 sys 还没有把 Cid 下发过来之前，
    // 这里必须明确返回空，禁止后续继续解引用错误地址。
    if (ethread_offset::Cid == 0 || ethread_offset::Cid == InvalidOffset)
    {
        return nullptr; // 失败返回空指针
    }
    size_t Thread = hv::current_guest_ethread(); // 获取来宾当前ETHREAD地址
    size_t ptr_Cid = Thread + ethread_offset::Cid; // 计算Cid字段地址
    return (PCLIENT_ID)ptr_Cid; // 返回Cid指针供上层使用
}

bool SetBreakpoint(PVT_BREAK_POINT vmcallinfo, unsigned __int64 Type) // 在来宾地址上设置EPT监视断点（不修改来宾代码）
{
    int errorCode = 0; // 记录失败原因的错误码
    VT_BREAK_POINT tmp_vmcallinfo = { 0 }; // 本地缓冲保存断点请求

    if (sizeof(VT_BREAK_POINT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT))) // 读取来宾断点参数
    {
        return false;
    }

	if (ept::ept_watch_activate(tmp_vmcallinfo, Type, vmcallinfo, errorCode)) // 通过EPT拦截实现断点，避免软件断点被检测
	{
		return true;
	}
    else
    {
        tmp_vmcallinfo.errorCode = errorCode; // 回填失败错误码

        if (sizeof(VT_BREAK_POINT) != hv::write_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT))) // 将错误信息写回来宾
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
    VT_BREAK_POINT tmp_vmcallinfo = { 0 }; // 本地缓冲保存断点请求

    if (sizeof(VT_BREAK_POINT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(VT_BREAK_POINT))) // 读取来宾断点参数
    {
        return false;
    }

    const int deactivate_status = ept::ept_watch_deactivate(tmp_vmcallinfo, tmp_vmcallinfo.watchid);
    // 删除断点要保持幂等：CE 步过/删除可能重复下发同一个 watchid，
    // watchid 已经空闲时也视为完成，避免用户态保留过期断点导致目标线程继续卡住。
    if (deactivate_status == 0 || deactivate_status == 2)
    {
        return true;
    }

    return false;
}

void MyKeBugCheck(
    _In_ ULONG BugCheckCode, // 蓝屏主错误码
    _In_ ULONG_PTR BugCheckParameter1, // 参数1：问题上下文相关信息
    _In_ ULONG_PTR BugCheckParameter2, // 参数2：问题上下文相关信息
    _In_ ULONG_PTR BugCheckParameter3, // 参数3：问题上下文相关信息
    _In_ ULONG_PTR BugCheckParameter4 // 参数4：问题上下文相关信息
)
{
    //__vmx_off();                                  // 退出vmx模式
    // __writecr3(g_guest_cr3);                      // 还原cr3
    // __reload_gdtr(g_gdtr.base_address, g_gdtr.limit); // 还原gdt
    // __lidt(&g_idtr);                                 // 还原idt
    KeBugCheckEx(BugCheckCode, BugCheckParameter1, BugCheckParameter2, BugCheckParameter3, BugCheckParameter4); // 触发蓝屏并生成dump
}

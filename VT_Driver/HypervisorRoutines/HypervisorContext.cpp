#include "HypervisorInternal.h"

namespace hv
{
    /// <summary>
    /// 将 CR3 切换到当前进程的页表基址(DTB)
    /// </summary>
    /// <returns> 返回切换前的 CR3 </returns>
    unsigned __int64 swap_context() // 切换到当前进程的页表基址，并返回旧 CR3
    {
        auto const current_process = reinterpret_cast<uint8_t*>(IoGetCurrentProcess());
        unsigned __int64 current_cr3 = __readcr3();
        unsigned __int64 guest_cr3 =
            *reinterpret_cast<unsigned __int64*>(current_process + hv::ghv.kprocess_directory_table_base_offset);

        __writecr3(guest_cr3);
        return current_cr3;
    }

    void swap_context(__vcpu* vcpu, unsigned __int64 new_cr3) // 在 vCPU 中记录目标 CR3，供切换使用
    {
        vcpu->target_cr3.flags = new_cr3;
    }

    /// <summary>
    /// 恢复 CR3 切换状态（通过清空目标 CR3 标记）
    /// </summary>
    /// <param name="vcpu">当前 vCPU 结构</param>
    void restore_context(__vcpu* vcpu) // 清除目标 CR3 标记，恢复默认页表上下文
    {
        vcpu->target_cr3.flags = 0;
    }

    /// <summary>
    /// 获取系统进程的页表基址
    /// </summary>
    /// <returns></returns>
    unsigned __int64 get_system_directory_table_base() // 读取系统进程（PID 4）的页表基址
    {
        auto const system_process = reinterpret_cast<uint8_t*>(PsInitialSystemProcess);
        return *reinterpret_cast<unsigned __int64*>(system_process + hv::ghv.kprocess_directory_table_base_offset);
    }

    /// <summary>
    /// 判断地址是否为规范地址（4 级分页）
    /// </summary>
    /// <param name="address">要检查的线性地址</param>
    /// <returns></returns>
    bool is_address_canonical(unsigned __int64 address) // 判断地址是否为规范地址（48 位有效地址范围）
    {
        if (address < NON_CANONICIAL_ADDRESS_END && address > NON_CANONICIAL_ADDRESS_START) // 判断是否落在非规范地址区间（落在则非法）
        {
            return false;
        }

        return true;
    }
}

#include "HypervisorInternal.h"

namespace hv
{
    /// <summary>
    /// 在 MSR 位图中设置某个 MSR 的拦截位
    /// </summary>
    /// <param name="msr">MSR 编号</param>
    /// <param name="bitmap">MSR 位图结构</param>
    /// <param name="enable_exiting">是否开启对该 MSR 的 VM-exit</param>
    void set_msr_bitmap(vmx_msr_bitmap& bitmap, uint32_t const msr, bool const enable_exiting) // 设置 MSR 位图以控制 VM-exit
    {
        auto const bit = static_cast<uint8_t>(enable_exiting ? 1 : 0); // 将布尔值转换为 0/1，用于设置位图

        if (msr <= MSR_ID_LOW_MAX) // MSR 在低范围，使用低位图
        {
            // 在低位 MSR 位图中设置该位
            bitmap.rdmsr_low[msr / 8] = (bit << (msr & 0b0111)); // 在低 MSR 范围的读位图中设置对应位
        }
        else if (msr >= MSR_ID_HIGH_MIN && msr <= MSR_ID_HIGH_MAX) // MSR 在高范围，使用高位图
        {
            // 在高位 MSR 位图中设置该位
            bitmap.rdmsr_high[(msr - MSR_ID_HIGH_MIN) / 8] = (bit << (msr & 0b0111)); // 在高 MSR 范围的读位图中设置对应位
        }
    }

    /// <summary>
    /// 在 I/O 位图中设置或清除某端口的拦截位
    /// </summary>
    /// <param name="io_port">需要设置的 I/O 端口号</param>
    /// <param name="vcpu">当前 vCPU 结构</param>
    /// <param name="value">true 表示拦截（置位），false 表示放行（清位）</param>
    void set_io_bitmap(unsigned __int16 io_port, __vcpu* vcpu, bool value) // 设置 I/O 位图拦截指定端口
    {
        unsigned __int16 bitmap_position; // 记录字节索引位置
        unsigned __int8 bitmap_bit; // 记录字节内的位索引

        //
        // 端口号 0x8000~0xFFFF 由 I/O 位图 B 管理
        if (io_port >= 0x8000) // 高端口号使用位图 B
        {
            io_port -= 0x8000; // 将端口号映射到位图 B 的范围
            bitmap_position = io_port / 8; // 计算字节位置
            bitmap_bit = io_port % 8; // 计算位位置

            if (value == true) // 根据 value 决定置位还是清位
            {
                *(vcpu->vcpu_bitmaps.io_bitmap_b + bitmap_position) |= (1 << bitmap_bit); // 置位：开启该端口 VM-exit
            }
            else
            {
                *(vcpu->vcpu_bitmaps.io_bitmap_b + bitmap_position) &= ~(1 << bitmap_bit); // 清位：关闭该端口 VM-exit
            }
        }
        else
        {
            bitmap_position = io_port / 8; // 计算字节位置
            bitmap_bit = io_port % 8; // 计算位位置

            if (value == true) // 根据 value 决定置位还是清位
            {
                *(vcpu->vcpu_bitmaps.io_bitmap_a + bitmap_position) |= (1 << bitmap_bit); // 置位：开启该端口 VM-exit
            }
            else
            {
                *(vcpu->vcpu_bitmaps.io_bitmap_a + bitmap_position) &= ~(1 << bitmap_bit); // 清位：关闭该端口 VM-exit
            }
        }
    }
}

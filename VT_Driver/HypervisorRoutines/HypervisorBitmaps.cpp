#include "HypervisorInternal.h"

namespace hv
{
    void set_msr_bitmap(vmx_msr_bitmap& bitmap, uint32_t const msr, bool const enable_exiting)
    {
        auto const bit = static_cast<uint8_t>(enable_exiting ? 1 : 0);

        if (msr <= MSR_ID_LOW_MAX)
        {
            bitmap.rdmsr_low[msr / 8] = (bit << (msr & 0b0111));
        }
        else if (msr >= MSR_ID_HIGH_MIN && msr <= MSR_ID_HIGH_MAX)
        {
            bitmap.rdmsr_high[(msr - MSR_ID_HIGH_MIN) / 8] = (bit << (msr & 0b0111));
        }
    }

    void set_io_bitmap(unsigned __int16 io_port, __vcpu* vcpu, bool value)
    {
        unsigned __int16 bitmap_position;
        unsigned __int8 bitmap_bit;

        if (io_port >= 0x8000)
        {
            io_port -= 0x8000;
            bitmap_position = io_port / 8;
            bitmap_bit = io_port % 8;

            if (value == true)
            {
                *(vcpu->vcpu_bitmaps.io_bitmap_b + bitmap_position) |= (1 << bitmap_bit);
            }
            else
            {
                *(vcpu->vcpu_bitmaps.io_bitmap_b + bitmap_position) &= ~(1 << bitmap_bit);
            }
        }
        else
        {
            bitmap_position = io_port / 8;
            bitmap_bit = io_port % 8;

            if (value == true)
            {
                *(vcpu->vcpu_bitmaps.io_bitmap_a + bitmap_position) |= (1 << bitmap_bit);
            }
            else
            {
                *(vcpu->vcpu_bitmaps.io_bitmap_a + bitmap_position) &= ~(1 << bitmap_bit);
            }
        }
    }
}

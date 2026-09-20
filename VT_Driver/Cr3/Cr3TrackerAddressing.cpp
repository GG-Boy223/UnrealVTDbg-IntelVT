#include "Cr3TrackerInternal.h"

#include "../Driver.h"
#include "../Globals.h"
#include "../hypervisor_routines.h"

namespace cr3_tracker_internal
{
    namespace
    {
        constexpr unsigned __int64 kEntryAddressMask = 0x000ffffffffff000ull;

        BOOLEAN ReadPhysicalEntry(
            unsigned __int64 physicalAddress,
            unsigned __int64* value)
        {
            if (value == nullptr ||
                (physicalAddress & 0xfffull) > PAGE_SIZE - sizeof(*value))
            {
                return FALSE;
            }

            void* virtualAddress = nullptr;
            if (!hv::get_physical_mapping_va(physicalAddress, &virtualAddress))
            {
                return FALSE;
            }

            hv::host_exception_info e = { 0 };
            hv::memcpy_safe(e, value,
                virtualAddress,
                sizeof(*value));
            return !e.exception_occurred;
        }
    }

    unsigned __int64 NormalizeCr3(unsigned __int64 value)
    {
        const unsigned __int64 root = value & ~0xfffull;
        return root != 0 ? root : 0;
    }

    unsigned __int64 ReadPml4Fingerprint(
        unsigned __int64 cr3,
        unsigned __int64 virtualAddress)
    {
        unsigned __int64 entry = 0;
        const unsigned __int64 index = (virtualAddress >> 39) & 0x1ff;
        if (!ReadPhysicalEntry(cr3 + index * sizeof(entry), &entry) ||
            (entry & 1) == 0)
        {
            return 0;
        }

        return entry & kEntryAddressMask;
    }

    unsigned __int64 TranslateProbe(
        unsigned __int64 cr3,
        unsigned __int64 virtualAddress)
    {
        static const ULONG shifts[] = { 39, 30, 21, 12 };
        unsigned __int64 table = cr3;

        for (ULONG level = 0; level < RTL_NUMBER_OF(shifts); ++level)
        {
            unsigned __int64 entry = 0;
            const unsigned __int64 index =
                (virtualAddress >> shifts[level]) & 0x1ff;
            if (!ReadPhysicalEntry(table + index * sizeof(entry), &entry) ||
                (entry & 1) == 0)
            {
                return 0;
            }

            if (level < 3 && (entry & (1ull << 7)) != 0)
            {
                const unsigned __int64 offsetMask =
                    (1ull << shifts[level]) - 1;
                return (entry & kEntryAddressMask & ~offsetMask) |
                    (virtualAddress & offsetMask);
            }

            table = entry & kEntryAddressMask;
            if (level == 3)
            {
                return table | (virtualAddress & 0xfff);
            }
        }

        return 0;
    }
}

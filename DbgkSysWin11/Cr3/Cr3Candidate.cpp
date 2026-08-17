#include "Cr3Internal.h"

namespace
{
    constexpr unsigned __int64 kPageMask = ~0xfffull;
    constexpr unsigned __int64 kEntryAddressMask = 0x000ffffffffff000ull;
    constexpr unsigned __int64 kPresentBit = 1ull;
    constexpr unsigned __int64 kLargePageBit = 1ull << 7;

    struct CR3_BASELINE
    {
        volatile LONG state;
        ULONG selfIndex;
        ULONG kernelIndex;
        unsigned __int64 kernelEntryAddress;
    };

    CR3_BASELINE g_Baseline = { 0 };

    BOOLEAN ReadPhysical(
        unsigned __int64 physicalAddress,
        PVOID buffer,
        SIZE_T size)
    {
        MM_COPY_ADDRESS source = { 0 };
        SIZE_T transferred = 0;

        source.PhysicalAddress.QuadPart = physicalAddress;
        NTSTATUS status = MmCopyMemory(
            buffer,
            source,
            size,
            MM_COPY_MEMORY_PHYSICAL,
            &transferred);
        return NT_SUCCESS(status) && transferred == size;
    }

    BOOLEAN InitializeBaseline()
    {
        const unsigned __int64 currentCr3 = Cr3NormalizeCandidate(__readcr3());
        if (currentCr3 == 0)
        {
            return FALSE;
        }

        PHYSICAL_ADDRESS address = { 0 };
        address.QuadPart = currentCr3;
        PULONG64 pml4 = static_cast<PULONG64>(MmGetVirtualForPhysical(address));
        if (pml4 == NULL)
        {
            return FALSE;
        }

        ULONG selfIndex = MAXULONG;
        __try
        {
            for (ULONG index = 0; index < 512; ++index)
            {
                if ((pml4[index] & kEntryAddressMask) == currentCr3)
                {
                    selfIndex = index;
                    break;
                }
            }

            const ULONG kernelIndex =
                (static_cast<unsigned __int64>(reinterpret_cast<ULONG_PTR>(&InitializeBaseline)) >> 39) & 0x1ff;
            const unsigned __int64 kernelEntry = pml4[kernelIndex] & kEntryAddressMask;
            if (selfIndex == MAXULONG || kernelEntry == 0)
            {
                return FALSE;
            }

            g_Baseline.selfIndex = selfIndex;
            g_Baseline.kernelIndex = kernelIndex;
            g_Baseline.kernelEntryAddress = kernelEntry;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return FALSE;
        }

        return TRUE;
    }

    BOOLEAN EnsureBaseline()
    {
        LONG state = InterlockedCompareExchange(&g_Baseline.state, 0, 0);
        if (state == 2)
        {
            return TRUE;
        }

        if (InterlockedCompareExchange(&g_Baseline.state, 1, 0) == 0)
        {
            const BOOLEAN initialized = InitializeBaseline();
            InterlockedExchange(&g_Baseline.state, initialized ? 2 : 0);
            return initialized;
        }

        while ((state = InterlockedCompareExchange(&g_Baseline.state, 0, 0)) == 1)
        {
            _mm_pause();
        }
        return state == 2;
    }

    BOOLEAN HasExpectedPml4Layout(
        PULONG64 pml4,
        unsigned __int64 candidateCr3)
    {
        if (pml4 == NULL || !EnsureBaseline())
        {
            return FALSE;
        }

        __try
        {
            const unsigned __int64 selfEntry = pml4[g_Baseline.selfIndex];
            const unsigned __int64 kernelEntry = pml4[g_Baseline.kernelIndex];
            return (selfEntry & kPresentBit) != 0 &&
                (selfEntry & kEntryAddressMask) == candidateCr3 &&
                (kernelEntry & kPresentBit) != 0 &&
                (kernelEntry & kEntryAddressMask) == g_Baseline.kernelEntryAddress;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return FALSE;
        }
    }

    unsigned __int64 TranslateVirtualAddress(
        unsigned __int64 cr3,
        unsigned __int64 virtualAddress)
    {
        static const ULONG shifts[] = { 39, 30, 21, 12 };
        unsigned __int64 table = cr3;

        for (ULONG level = 0; level < RTL_NUMBER_OF(shifts); ++level)
        {
            const unsigned __int64 index = (virtualAddress >> shifts[level]) & 0x1ff;
            unsigned __int64 entry = 0;
            if (!ReadPhysical(table + index * sizeof(entry), &entry, sizeof(entry)) ||
                (entry & kPresentBit) == 0)
            {
                return 0;
            }

            if (level < 3 && (entry & kLargePageBit) != 0)
            {
                const unsigned __int64 offsetMask = (1ull << shifts[level]) - 1;
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

    BOOLEAN ValidateImage(
        unsigned __int64 candidateCr3,
        const CR3_PROCESS_PROBE* probe)
    {
        UCHAR imageBytes[kCr3ImageProbeSize] = { 0 };
        const unsigned __int64 physicalAddress =
            TranslateVirtualAddress(candidateCr3, probe->imageBase);
        if (physicalAddress == 0 ||
            !ReadPhysical(physicalAddress, imageBytes, sizeof(imageBytes)))
        {
            return FALSE;
        }

        if (*reinterpret_cast<PUSHORT>(imageBytes) != IMAGE_DOS_SIGNATURE)
        {
            return FALSE;
        }

        return !probe->hasImageProbe ||
            RtlCompareMemory(
                imageBytes,
                probe->imageBytes,
                sizeof(imageBytes)) == sizeof(imageBytes);
    }
}

BOOLEAN Cr3ValidateCandidate(
    unsigned __int64 candidateCr3,
    const CR3_PROCESS_PROBE* probe)
{
    const unsigned __int64 root = Cr3NormalizeCandidate(candidateCr3);
    if (root == 0 || probe == NULL || !EnsureBaseline())
    {
        return FALSE;
    }

    unsigned __int64 selfEntry = 0;
    unsigned __int64 kernelEntry = 0;
    if (!ReadPhysical(root + g_Baseline.selfIndex * sizeof(ULONG64), &selfEntry, sizeof(selfEntry)) ||
        !ReadPhysical(root + g_Baseline.kernelIndex * sizeof(ULONG64), &kernelEntry, sizeof(kernelEntry)))
    {
        return FALSE;
    }

    return (selfEntry & kPresentBit) != 0 &&
        (selfEntry & kEntryAddressMask) == root &&
        (kernelEntry & kPresentBit) != 0 &&
        (kernelEntry & kEntryAddressMask) == g_Baseline.kernelEntryAddress &&
        ValidateImage(root, probe);
}

unsigned __int64 Cr3ScanPhysicalMemory(
    const CR3_PROCESS_PROBE* probe,
    unsigned __int64* scannedPages)
{
    if (scannedPages != NULL)
    {
        *scannedPages = 0;
    }
    if (probe == NULL || KeGetCurrentIrql() != PASSIVE_LEVEL || !EnsureBaseline())
    {
        return 0;
    }

    PPHYSICAL_MEMORY_RANGE ranges = MmGetPhysicalMemoryRanges();
    if (ranges == NULL)
    {
        return 0;
    }

    unsigned __int64 resolvedCr3 = 0;
    for (ULONG rangeIndex = 0;
        ranges[rangeIndex].NumberOfBytes.QuadPart != 0 && resolvedCr3 == 0;
        ++rangeIndex)
    {
        const unsigned __int64 start = ranges[rangeIndex].BaseAddress.QuadPart & kPageMask;
        const unsigned __int64 end = start + ranges[rangeIndex].NumberOfBytes.QuadPart;
        for (unsigned __int64 candidate = start; candidate < end; candidate += PAGE_SIZE)
        {
            if (scannedPages != NULL)
            {
                ++(*scannedPages);
            }

            PHYSICAL_ADDRESS address = { 0 };
            address.QuadPart = candidate;
            PULONG64 pml4 = static_cast<PULONG64>(MmGetVirtualForPhysical(address));
            if (HasExpectedPml4Layout(pml4, candidate) && ValidateImage(candidate, probe))
            {
                resolvedCr3 = candidate;
                break;
            }
        }
    }

    ExFreePool(ranges);
    return resolvedCr3;
}

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
        unsigned __int64 candidateCr3)
    {
        if (candidateCr3 == 0 || !EnsureBaseline())
        {
            return FALSE;
        }

        // 这里必须按物理地址读，不能把 MmGetVirtualForPhysical 的返回值直接解引用。
        //
        // MmGetVirtualForPhysical 只是「物理内存窗口基址 + 物理地址」的纯加法，它不校验该物理页
        // 是否真实存在、是否已被映射进窗口；而 MmGetPhysicalMemoryRanges 报出来的区间并不保证
        // 每一页都落在窗口内。一旦扫描到的页帧正好落在窗口空洞里，解引用就会立刻触发非分页区
        // 缺页 —— 这种缺页由内存管理器直接 KeBugCheckEx(0x50 PAGE_FAULT_IN_NONPAGED_AREA) 上报，
        // **根本不会变成可分发的异常，__try/__except 拦不住**（本函数原先的保护是假的）。
        //
        // ReadPhysical 走 MmCopyMemory(MM_COPY_MEMORY_PHYSICAL)，先校验 PFN 再拷贝，非法地址
        // 返回失败状态而不是缺页。同文件的 Cr3ValidateCandidate 一直就是这么做的，这里只是把
        // 两条路径统一到同一个安全姿势上。
        unsigned __int64 selfEntry = 0;
        if (!ReadPhysical(candidateCr3 + g_Baseline.selfIndex * sizeof(ULONG64), &selfEntry, sizeof(selfEntry)) ||
            (selfEntry & kPresentBit) == 0 ||
            (selfEntry & kEntryAddressMask) != candidateCr3)
        {
            return FALSE;
        }

        unsigned __int64 kernelEntry = 0;
        if (!ReadPhysical(candidateCr3 + g_Baseline.kernelIndex * sizeof(ULONG64), &kernelEntry, sizeof(kernelEntry)))
        {
            return FALSE;
        }

        return (kernelEntry & kPresentBit) != 0 &&
            (kernelEntry & kEntryAddressMask) == g_Baseline.kernelEntryAddress;
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

            if (HasExpectedPml4Layout(candidate) && ValidateImage(candidate, probe))
            {
                resolvedCr3 = candidate;
                break;
            }
        }
    }

    ExFreePool(ranges);
    return resolvedCr3;
}

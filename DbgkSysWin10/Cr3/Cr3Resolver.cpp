#include "Cr3Internal.h"

namespace
{
    constexpr ULONG kCr3CacheSize = 16;

    enum CR3_RESOLVE_SOURCE
    {
        Cr3SourceNone,
        Cr3SourceCache,
        Cr3SourceSupplied,
        Cr3SourceDirectoryTable,
        Cr3SourceAttach,
        Cr3SourcePfnScan
    };

    struct CR3_CACHE_ENTRY
    {
        PEPROCESS process;
        HANDLE processId;
        unsigned __int64 imageBase;
        unsigned __int64 cr3;
        unsigned __int64 generation;
    };

    KSPIN_LOCK g_CacheLock = { 0 };
    volatile LONG g_CacheState = 0;
    volatile LONG g_CacheCursor = 0;
    unsigned __int64 g_CacheGeneration = 0;
    CR3_CACHE_ENTRY g_Cache[kCr3CacheSize] = { 0 };

    VOID EnsureCacheInitialized()
    {
        if (InterlockedCompareExchange(&g_CacheState, 1, 0) == 0)
        {
            KeInitializeSpinLock(&g_CacheLock);
            RtlZeroMemory(g_Cache, sizeof(g_Cache));
            InterlockedExchange(&g_CacheState, 2);
            return;
        }

        while (InterlockedCompareExchange(&g_CacheState, 0, 0) != 2)
        {
            _mm_pause();
        }
    }

    unsigned __int64 CacheGet(const CR3_PROCESS_PROBE* probe)
    {
        EnsureCacheInitialized();
        KIRQL oldIrql = PASSIVE_LEVEL;
        unsigned __int64 cr3 = 0;
        KeAcquireSpinLock(&g_CacheLock, &oldIrql);
        for (ULONG index = 0; index < kCr3CacheSize; ++index)
        {
            const CR3_CACHE_ENTRY& entry = g_Cache[index];
            if (entry.process == probe->process &&
                entry.processId == probe->processId &&
                entry.imageBase == probe->imageBase)
            {
                cr3 = entry.cr3;
                break;
            }
        }
        KeReleaseSpinLock(&g_CacheLock, oldIrql);
        return cr3;
    }

    VOID CacheInvalidate(
        const CR3_PROCESS_PROBE* probe,
        unsigned __int64 expectedCr3)
    {
        KIRQL oldIrql = PASSIVE_LEVEL;
        KeAcquireSpinLock(&g_CacheLock, &oldIrql);
        for (ULONG index = 0; index < kCr3CacheSize; ++index)
        {
            if (g_Cache[index].process == probe->process &&
                g_Cache[index].processId == probe->processId &&
                g_Cache[index].cr3 == expectedCr3)
            {
                g_Cache[index] = { 0 };
                break;
            }
        }
        KeReleaseSpinLock(&g_CacheLock, oldIrql);
    }

    VOID CacheUpdate(
        const CR3_PROCESS_PROBE* probe,
        unsigned __int64 cr3)
    {
        KIRQL oldIrql = PASSIVE_LEVEL;
        LONG selected = -1;
        KeAcquireSpinLock(&g_CacheLock, &oldIrql);
        for (ULONG index = 0; index < kCr3CacheSize; ++index)
        {
            if (g_Cache[index].process == probe->process ||
                (selected < 0 && g_Cache[index].process == NULL))
            {
                selected = static_cast<LONG>(index);
                if (g_Cache[index].process == probe->process)
                {
                    break;
                }
            }
        }
        if (selected < 0)
        {
            selected = InterlockedIncrement(&g_CacheCursor) % kCr3CacheSize;
        }

        CR3_CACHE_ENTRY& entry = g_Cache[selected];
        entry.process = probe->process;
        entry.processId = probe->processId;
        entry.imageBase = probe->imageBase;
        entry.cr3 = cr3;
        entry.generation = ++g_CacheGeneration;
        KeReleaseSpinLock(&g_CacheLock, oldIrql);
    }

    unsigned __int64 ReadDirectoryTableBase(PEPROCESS process)
    {
        unsigned __int64 value = 0;
        __try
        {
            value = *reinterpret_cast<unsigned __int64*>(
                reinterpret_cast<PUCHAR>(process) + kprocess_offset::DirectoryTableBase);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value = 0;
        }
        return value;
    }

    unsigned __int64 ReadAttachedCr3(PEPROCESS process)
    {
        KAPC_STATE apcState = { 0 };
        BOOLEAN attached = FALSE;
        unsigned __int64 value = 0;

        __try
        {
            if (Original_KeStackAttachProcess != NULL)
            {
                Original_KeStackAttachProcess(reinterpret_cast<PRKPROCESS>(process), &apcState);
            }
            else
            {
                KeStackAttachProcess(reinterpret_cast<PRKPROCESS>(process), &apcState);
            }
            attached = TRUE;
            value = __readcr3();
        }
        __finally
        {
            if (attached)
            {
                KeUnstackDetachProcess(&apcState);
            }
        }
        return value;
    }

    const char* SourceName(CR3_RESOLVE_SOURCE source)
    {
        switch (source)
        {
        case Cr3SourceCache:
            return "cache";
        case Cr3SourceSupplied:
            return "supplied";
        case Cr3SourceDirectoryTable:
            return "directory-table";
        case Cr3SourceAttach:
            return "attach";
        case Cr3SourcePfnScan:
            return "pfn-scan";
        default:
            return "none";
        }
    }

    unsigned __int64 TryCandidate(
        unsigned __int64 value,
        const CR3_PROCESS_PROBE* probe)
    {
        const unsigned __int64 candidate = Cr3NormalizeCandidate(value);
        return candidate != 0 && Cr3ValidateCandidate(candidate, probe) ? candidate : 0;
    }
}

NTSTATUS Cr3ResolveProcess(
    PEPROCESS process,
    unsigned __int64 suppliedCr3,
    unsigned __int64* resolvedCr3)
{
    if (process == NULL || resolvedCr3 == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }
    *resolvedCr3 = 0;

    CR3_PROCESS_PROBE probe = { 0 };
    if (!Cr3BuildProcessProbe(process, &probe))
    {
        //DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,"[UnrealDbg][CR3] target probe capture failed pid=%p\n",PsGetProcessId(process));
        return STATUS_NOT_FOUND;
    }

    CR3_RESOLVE_SOURCE source = Cr3SourceNone;
    unsigned __int64 candidate = CacheGet(&probe);
    if (candidate != 0)
    {
        if (Cr3ValidateCandidate(candidate, &probe))
        {
            source = Cr3SourceCache;
        }
        else
        {
            CacheInvalidate(&probe, candidate);
            candidate = 0;
        }
    }

    if (candidate == 0)
    {
        candidate = TryCandidate(suppliedCr3, &probe);
        source = candidate != 0 ? Cr3SourceSupplied : Cr3SourceNone;
    }
    if (candidate == 0)
    {
        candidate = TryCandidate(ReadDirectoryTableBase(process), &probe);
        source = candidate != 0 ? Cr3SourceDirectoryTable : Cr3SourceNone;
    }
    if (candidate == 0)
    {
        __try
        {
            candidate = TryCandidate(ReadAttachedCr3(process), &probe);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            candidate = 0;
        }
        source = candidate != 0 ? Cr3SourceAttach : Cr3SourceNone;
    }

    unsigned __int64 scannedPages = 0;
    if (candidate == 0)
    {
        candidate = Cr3ScanPhysicalMemory(&probe, &scannedPages);
        source = candidate != 0 ? Cr3SourcePfnScan : Cr3SourceNone;
    }
    if (candidate == 0)
    {
        //DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,"[UnrealDbg][CR3] resolve failed pid=%p scanned=%llu\n",probe.processId, scannedPages);
        return STATUS_NOT_FOUND;
    }

    CacheUpdate(&probe, candidate);
    *resolvedCr3 = candidate;
    //DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,"[UnrealDbg][CR3] resolved pid=%p cr3=%llx source=%s scanned=%llu\n", probe.processId,candidate, SourceName(source),scannedPages);
    return STATUS_SUCCESS;
}

VOID Cr3ResolverReset()
{
    EnsureCacheInitialized();
    KIRQL oldIrql = PASSIVE_LEVEL;
    KeAcquireSpinLock(&g_CacheLock, &oldIrql);
    RtlZeroMemory(g_Cache, sizeof(g_Cache));
    g_CacheGeneration = 0;
    InterlockedExchange(&g_CacheCursor, 0);
    KeReleaseSpinLock(&g_CacheLock, oldIrql);
}

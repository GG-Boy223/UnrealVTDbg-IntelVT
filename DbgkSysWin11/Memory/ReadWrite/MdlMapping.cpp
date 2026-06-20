#include "ReadWriteInternal.h"

PVOID MDLMapVirtualAddress(IN PVOID VirtualAddress, IN SIZE_T BufferSize, OUT MDL_MAP& map_table)
{
    PMDL mdl = NULL;
    PVOID MappedAddress = NULL;
    BOOLEAN lockpage = FALSE;

    __try
    {
        SIZE_T tosize;

        if (BufferSize < PAGE_SIZE)
        {
            tosize = PAGE_SIZE;
        }
        else
        {
            tosize = BufferSize;
        }

        if (tosize > 0xFFFFFFFFULL)
        {
            map_table = { 0 };
            return NULL;
        }

        mdl = IoAllocateMdl(VirtualAddress, static_cast<ULONG>(tosize), FALSE, FALSE, NULL);
        if (mdl)
        {
            MmProbeAndLockPages(mdl, KernelMode, IoReadAccess);
            lockpage = TRUE;

            MappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmCached, NULL, FALSE, HighPagePriority);
            if (MappedAddress == NULL)
            {
                MmUnlockPages(mdl);
                IoFreeMdl(mdl);
                map_table = { 0 };
            }
            else
            {
                map_table.mdl = mdl;
                map_table.MappedAddress = MappedAddress;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (MappedAddress)
        {
            MmUnmapLockedPages(MappedAddress, mdl);
        }
        if (lockpage)
        {
            MmUnlockPages(mdl);
        }
        if (mdl)
        {
            IoFreeMdl(mdl);
        }
        MappedAddress = NULL;
    }
    return MappedAddress;
}

VOID UnMDLMapVirtualAddress(MDL_MAP map_table)
{
    if (map_table.MappedAddress)
    {
        MmUnmapLockedPages(map_table.MappedAddress, map_table.mdl);
    }
    if (map_table.mdl)
    {
        MmUnlockPages(map_table.mdl);
        IoFreeMdl(map_table.mdl);
    }
}

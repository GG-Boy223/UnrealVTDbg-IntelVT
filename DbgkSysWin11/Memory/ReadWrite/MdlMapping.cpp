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
            ////判断当前调用者是否是调试器进程
            //if (!IsDebugger(PsGetCurrentProcess()))
            //{
            //    //根据窗口句柄获得窗口对象
            //    PWND pwnd = Sys_ValidateHwnd(hwnd);
            //    if (pwnd)
            //    {
            //        //根据线程对象获得进程对象
            //        PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);
            //    }
            //}
            //NtUserQueryWindow(hwnd, WindowInfo);
            return NULL;
        }

        mdl = IoAllocateMdl(VirtualAddress, static_cast<ULONG>(tosize), FALSE, FALSE, NULL);
        if (mdl)
        {
            //锁住物理页，不让它被交换到磁盘上.
            MmProbeAndLockPages(mdl, KernelMode, IoReadAccess);
            lockpage = TRUE;

            //将 MDL 描述的物理页面映射到虚拟内存
            MappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmCached, NULL, FALSE, HighPagePriority);
            if (MappedAddress == NULL)
            {
                //失败则进行扫尾工作
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
            //失败则进行扫尾工作
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

#include <ntifs.h>
#include "../StringHandler.h"

PVOID MemAllocate(IN SIZE_T Size, IN BOOLEAN Paged, IN ULONG Tag)
{
    if (Size == 0)
    {
        return NULL;
    }

#pragma warning(suppress:4996)
    PVOID pAddr = ExAllocatePoolWithTag(Paged ? PagedPool : NonPagedPool,
        Size,
        Tag);
    if (pAddr != NULL)
    {
        RtlZeroMemory(pAddr, Size);
    }
    return pAddr;
}

VOID FreeMemAllocate(IN PVOID Ptr, IN ULONG Tag)
{
    ExFreePoolWithTag(Ptr, Tag);
}

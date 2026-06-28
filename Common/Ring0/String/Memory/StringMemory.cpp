#include <ntifs.h>
#include "../StringHandler.h"

//分配池内存
PVOID MemAllocate(IN SIZE_T Size, IN BOOLEAN Paged, IN ULONG Tag)
{
    if (Size == 0)
    {
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
        return NULL; // CR3 无效时无法转换
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

//释放池内存
VOID FreeMemAllocate(IN PVOID Ptr, IN ULONG Tag)
{
    ExFreePoolWithTag(Ptr, Tag);
}

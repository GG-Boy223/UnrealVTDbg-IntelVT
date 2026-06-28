#include <ntifs.h>
#include <ntstrsafe.h>
#include "../../../SymbolicAccess/Utils/Log.h"
#include "../Api/ApcInjectNativeApi.h"
#include "ModuleResolver.h"

/// <param name="pProcess">Target process</param>
/// <param name="ModuleName">Nodule name to search for</param>
/// <param name="isWow64">If TRUE - search in 32-bit PEB</param>
/// <returns>Found address, NULL if not found</returns>
/// <summary>
/// Get module base address by name
/// </summary>
PVOID BBGetUserModule(IN PEPROCESS pProcess, IN PUNICODE_STRING ModuleName, IN BOOLEAN isWow64)
{
    ASSERT(pProcess != NULL);
    // System image, not supported
    if (pProcess == NULL)
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

    // Protect from UserMode AV
    __try
    {
        LARGE_INTEGER time = { 0 };
        time.QuadPart = -250ll * 10 * 1000; // 250 msec.

        // Wow64 process
        if (isWow64)
        {
            PPEB32 pPeb32 = (PPEB32)PsGetProcessWow64Process(pProcess);
            if (pPeb32 == NULL)
            {
                outLog("BBGetUserModule failed to get Wow64 PEB");
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

            // Wait for loader a bit
            for (INT i = 0; !pPeb32->Ldr && i < 10; i++)
            {
                KeDelayExecutionThread(KernelMode, TRUE, &time);
            }

            // Still no loader
            if (!pPeb32->Ldr)
            {
                outLog("BBGetUserModule Wow64 loader is null");
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

            // Search in InLoadOrderModuleList
            for (PLIST_ENTRY32 pListEntry = (PLIST_ENTRY32)((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList.Flink;
                pListEntry != &((PPEB_LDR_DATA32)pPeb32->Ldr)->InLoadOrderModuleList;
                pListEntry = (PLIST_ENTRY32)pListEntry->Flink)
            {
                UNICODE_STRING ustr;
                PLDR_DATA_TABLE_ENTRY32 pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

                RtlUnicodeStringInit(&ustr, (PWCH)pEntry->BaseDllName.Buffer);

                if (RtlCompareUnicodeString(&ustr, ModuleName, TRUE) == 0)
                    return (PVOID)pEntry->DllBase;
            }
        }
        else
        {
            PPEB pPeb = PsGetProcessPeb(pProcess);
            if (!pPeb)
            {
                outLog("BBGetUserModule failed to get PEB");
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

            // Wait for loader a bit
            for (INT i = 0; !pPeb->Ldr && i < 10; i++)
            {
                KeDelayExecutionThread(KernelMode, TRUE, &time);
            }

            // Still no loader
            if (!pPeb->Ldr)
            {
                outLog("BBGetUserModule loader is null");
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

            // Search in InLoadOrderModuleList
            for (PLIST_ENTRY pListEntry = pPeb->Ldr->InLoadOrderModuleList.Flink;
                pListEntry != &pPeb->Ldr->InLoadOrderModuleList;
                pListEntry = pListEntry->Flink)
            {
                PLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
                if (RtlCompareUnicodeString(&pEntry->BaseDllName, ModuleName, TRUE) == 0)
                    return pEntry->DllBase;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        outLog("BBGetUserModule Exception, Code: 0x%X\n", GetExceptionCode());
    }

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
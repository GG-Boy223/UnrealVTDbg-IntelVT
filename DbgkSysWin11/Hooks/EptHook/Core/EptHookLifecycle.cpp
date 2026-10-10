#include "EptHookCommon.h"

EXTERN_C
VOID UnEptHook()
{
    if (!g_IsInitGlobalVariable)
    {
        outLog("跳过 EPT 取消 hook：符号或 hook 尚未初始化.");
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    bool vtReady = false;
    __try
    {
        vtReady = hvgt::test_vmcall();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        vtReady = false;
    }

    if (!vtReady)
    {
        outLog("跳过 EPT 取消 hook：VT 通道不可用.");
        KeBugCheckEx(0x00020001, 0x4442474B, 1, 0, 0);
        return;
    }

    bool unhooked = false;
    __try
    {
        unhooked = hvgt::ept_unhook();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        unhooked = false;
    }

    if (unhooked)
    {
        outLog("EPT 取消 hook 成功.");

        Original_PspExitThread = NULL;
        Original_DbgkCreateThread = NULL;
        Original_PspCallThreadNotifyRoutines = NULL;
        Original_ObReferenceObjectByHandleWithTag = NULL;
        Original_ObfDereferenceObjectWithTag = NULL;
        Original_ObReferenceObjectByHandle = NULL;
        Original_ObfDereferenceObject = NULL;
        Original_ValidateHwnd = NULL;
        Original_NtUserFindWindowEx = NULL;
        Original_NtUserWindowFromPoint = NULL;
        Original_NtUserSetWindowsHookEx = NULL;
        Original_NtUserGetClassName = NULL;
        Original_NtTerminateProcess = NULL;
        Original_NtSuspendThread = NULL;
        Original_NtResumeThread = NULL;
        Original_NtQueryInformationThread = NULL;
        Original_NtReadVirtualMemory = NULL;
        Original_NtWriteVirtualMemory = NULL;
        Original_NtGetContextThread = NULL;
        Original_NtSetContextThread = NULL;
        Original_PspCreateThread = NULL;
        Original_NtCreateThreadEx = NULL;
        Original_PspAllocateThread = NULL;
        Original_KeStackAttachProcess = NULL;
        Original_KiStackAttachProcess = NULL;
        Original_NtOpenProcess = NULL;
        Original_NtProtectVirtualMemory = NULL;
        Original_MmProtectVirtualMemory = NULL;
        Original_MmCopyVirtualMemory = NULL;
        Original_PspCreateUserContext = NULL;
        Original_ObpReferenceObjectByHandleWithTag = NULL;
        Original_MiObtainReferencedVadEx = NULL;
        Original_KiDispatchException = NULL;

        g_IsInitGlobalVariable = FALSE;
        return;
    }

    outLog("EPT 取消 hook 失败或已跳过.");
    KeBugCheckEx(0x00020001, 0x4442474B, 2, 0, 0);
}

EXTERN_C
VOID SetupEptHook()
{
    if (g_IsInitGlobalVariable)
    {
        //ntos
        Hook_NtCreateDebugObject(); //此函数是创建调试对象 必须第一个先hook
        Hook_PspInsertProcess();
        Hook_NtSetInformationDebugObject();
        Hook_NtRemoveProcessDebug();
        Hook_NtDebugActiveProcess();
        Hook_NtWaitForDebugEvent();
        Hook_NtDebugContinue();
        Hook_DbgkMapViewOfSection();
        Hook_DbgkUnMapViewOfSection();
        Hook_DbgkCreateThread();
        Hook_DbgkExitThread();
        Hook_DbgkExitProcess();
        Hook_DbgkForwardException();
        Hook_DbgkpQueueMessage();
        Hook_PspCallThreadNotifyRoutines();
        Hook_PspExitThread();
        Hook_ObpReferenceObjectByHandleWithTag();

        //win32k.sys
        Hook_ValidateHwnd(); //win32k中此函数必须先hook
        Hook_NtUserFindWindowEx();
        Hook_NtUserWindowFromPoint();
        Hook_NtUserSetWindowsHookEx();
        Hook_NtUserGetClassName();
    }
}

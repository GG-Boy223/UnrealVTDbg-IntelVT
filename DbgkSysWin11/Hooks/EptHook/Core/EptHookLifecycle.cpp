#include "EptHookCommon.h"

EXTERN_C
VOID UnEptHook()
{
    if (!g_IsInitGlobalVariable)
    {
        outLog("\xCC\xF8\xB9\xFD\x20\x45\x50\x54\x20\xC8\xA1\xCF\xFB\x20\x68\x6F\x6F\x6B\xA3\xBA\xB7\xFB\xBA\xC5\xBB\xF2\x20\x68\x6F\x6F\x6B\x20\xC9\xD0\xCE\xB4\xB3\xF5\xCA\xBC\xBB\xAF\x2E");
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
        outLog("\xCC\xF8\xB9\xFD\x20\x45\x50\x54\x20\xC8\xA1\xCF\xFB\x20\x68\x6F\x6F\x6B\xA3\xBA\x56\x54\x20\xCD\xA8\xB5\xC0\xB2\xBB\xBF\xC9\xD3\xC3\x2E");
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
        outLog("\x45\x50\x54\x20\xC8\xA1\xCF\xFB\x20\x68\x6F\x6F\x6B\x20\xB3\xC9\xB9\xA6\x2E");
        g_IsInitGlobalVariable = FALSE;
        return;
    }

    outLog("\x45\x50\x54\x20\xC8\xA1\xCF\xFB\x20\x68\x6F\x6F\x6B\x20\xCA\xA7\xB0\xDC\xBB\xF2\xD2\xD1\xCC\xF8\xB9\xFD\x2E");
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
    }
}

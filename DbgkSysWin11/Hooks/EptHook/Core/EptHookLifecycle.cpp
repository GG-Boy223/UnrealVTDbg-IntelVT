#include "EptHookCommon.h"

static ULONG64 g_EptHookSelectionMask = HOOK_SELECTION_ALL;

EXTERN_C
NTSTATUS ConfigureEptHookSelection(ULONG64 mask)
{
    if ((mask & ~HOOK_SELECTION_ALL) != 0)
    {
        outFile("拒绝修改 EPT hook 选择：掩码包含未知位 0x%llX.", mask);
        return STATUS_INVALID_PARAMETER;
    }

    g_EptHookSelectionMask = mask;
    outFile("EPT hook 选择已配置：mask=0x%llX.", mask);
    return STATUS_SUCCESS;
}

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
        Original_DbgkOpenProcessDebugPort = NULL;
        Original_NtQueryInformationProcess = NULL;
        Original_NtSetInformationProcess = NULL;
        Original_NtQueryObject = NULL;
        Original_NtQuerySystemInformation = NULL;
        Original_NtSystemDebugControl = NULL;
        Original_NtClose = NULL;
        Original_NtDuplicateObject = NULL;
        Original_NtSetInformationThread = NULL;

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
        outFile("开始安装 EPT hook：mask=0x%llX.", g_EptHookSelectionMask);

        //ntos
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_CREATE_DEBUG_OBJECT)
            Hook_NtCreateDebugObject(); //此函数是创建调试对象 必须第一个先hook
        if (g_EptHookSelectionMask & HOOK_SELECTION_PSP_INSERT_PROCESS)
            Hook_PspInsertProcess();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_SET_INFORMATION_DEBUG_OBJECT)
            Hook_NtSetInformationDebugObject();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_REMOVE_PROCESS_DEBUG)
            Hook_NtRemoveProcessDebug();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_DEBUG_ACTIVE_PROCESS)
            Hook_NtDebugActiveProcess();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_WAIT_FOR_DEBUG_EVENT)
            Hook_NtWaitForDebugEvent();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_DEBUG_CONTINUE)
            Hook_NtDebugContinue();
        if (g_EptHookSelectionMask & HOOK_SELECTION_DBGK_MAP_VIEW_OF_SECTION)
            Hook_DbgkMapViewOfSection();
        if (g_EptHookSelectionMask & HOOK_SELECTION_DBGK_UNMAP_VIEW_OF_SECTION)
            Hook_DbgkUnMapViewOfSection();
        if (g_EptHookSelectionMask & HOOK_SELECTION_DBGK_CREATE_THREAD)
            Hook_DbgkCreateThread();
        if (g_EptHookSelectionMask & HOOK_SELECTION_DBGK_EXIT_THREAD)
            Hook_DbgkExitThread();
        if (g_EptHookSelectionMask & HOOK_SELECTION_DBGK_EXIT_PROCESS)
            Hook_DbgkExitProcess();
        if (g_EptHookSelectionMask & HOOK_SELECTION_DBGK_FORWARD_EXCEPTION)
            Hook_DbgkForwardException();
        if (g_EptHookSelectionMask & HOOK_SELECTION_DBGKP_QUEUE_MESSAGE)
            Hook_DbgkpQueueMessage();
        // Hook_PspCallThreadNotifyRoutines 已延后到首次附加进程（NtCreateDebugObject）时安装，
        // 减少驱动加载阶段的 hook 面；线程创建事件只在有调试目标后才有意义。
        if (g_EptHookSelectionMask & HOOK_SELECTION_PSP_EXIT_THREAD)
            Hook_PspExitThread();
        if (g_EptHookSelectionMask & HOOK_SELECTION_OBP_REFERENCE_OBJECT_BY_HANDLE_TAG)
            Hook_ObpReferenceObjectByHandleWithTag();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_CLOSE)
            Hook_NtClose();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_DUPLICATE_OBJECT)
            Hook_NtDuplicateObject();

        // 反-反调试（TitanHide 对照）。
        // 2026-10-15 的 #UD 根因是 handler_vmcall_rip 使用宿主 dwm CR3 翻译 guest RIP；
        // KVA shadow/地址空间切换后查找失败，vmexit_vmcall_handler 才把自定义 VMCALL
        // 误报成 guest #UD。VT_Driver 已改为使用当前 GUEST_CR3 并增加精确 rip 回退，
        // 因此这里恢复 TitanHide 同语义的完整查询/句柄保护面。
        if (g_EptHookSelectionMask & HOOK_SELECTION_DBGK_OPEN_PROCESS_DEBUG_PORT)
            Hook_DbgkOpenProcessDebugPort();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_QUERY_INFORMATION_PROCESS)
            Hook_NtQueryInformationProcess();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_SET_INFORMATION_PROCESS)
            Hook_NtSetInformationProcess();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_QUERY_OBJECT)
            Hook_NtQueryObject();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_QUERY_SYSTEM_INFORMATION)
            Hook_NtQuerySystemInformation();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_SYSTEM_DEBUG_CONTROL)
            Hook_NtSystemDebugControl();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_SET_INFORMATION_THREAD)
            Hook_NtSetInformationThread();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_QUERY_INFORMATION_THREAD)
            Hook_NtQueryInformationThread();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_GET_CONTEXT_THREAD)
            Hook_NtGetContextThread();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_SET_CONTEXT_THREAD)
            Hook_NtSetContextThread();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_CREATE_THREAD_EX)
            Hook_NtCreateThreadEx();

        //win32k.sys
        if (g_EptHookSelectionMask & HOOK_SELECTION_VALIDATE_HWND)
            Hook_ValidateHwnd(); //win32k中此函数必须先hook
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_USER_FIND_WINDOW_EX)
            Hook_NtUserFindWindowEx();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_USER_WINDOW_FROM_POINT)
            Hook_NtUserWindowFromPoint();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_USER_SET_WINDOWS_HOOK_EX)
            Hook_NtUserSetWindowsHookEx();
        if (g_EptHookSelectionMask & HOOK_SELECTION_NT_USER_GET_CLASS_NAME)
            Hook_NtUserGetClassName();
    }
}

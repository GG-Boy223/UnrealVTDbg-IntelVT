#include "EptHookCommon.h"

EXTERN_C
VOID Hook_NtQueryInformationThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtQueryInformationThread, NewNtQueryInformationThread, (PVOID*)&Original_NtQueryInformationThread, "NtQueryInformationThread", "NtQueryInformationThread");
    }
}

EXTERN_C
VOID Hook_NtSuspendThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtSuspendThread, NewNtSuspendThread, (PVOID*)&Original_NtSuspendThread, "NtSuspendThread", "NtSuspendThread");
    }
}

EXTERN_C
VOID Hook_NtResumeThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtResumeThread, NewNtResumeThread, (PVOID*)&Original_NtResumeThread, "NtResumeThread", "NtResumeThread");
    }
}

// 旧项目保留的禁用路径：PspInsertThread 的 EPT hook wrapper 曾被整段注释。
// 为什么注释：当前线程创建链路主要使用 DbgkCreateThread、PspCallThreadNotifyRoutines、PspExitThread 等更贴近调试事件
// 的入口；PspInsertThread 更靠近内核线程插入流程，调用频率高且结构版本敏感。
// 放开的好处：能更早观察线程插入，可能覆盖 DbgkCreateThread 之前的状态。
// 放开的坏处：容易影响系统线程/非目标线程，且旧 wrapper 没有额外过滤，回归风险高。
// 项目影响：会扩大 EPT hook 面，线程创建、退出和调试端口状态可能被重复处理。
// 结论：继续注释。
//EXTERN_C
//VOID Hook_PspInsertThread()
//{
//    if (g_IsInitGlobalVariable)
//    {
//        ASSERT(Sys_PspInsertThread);
//        if (Sys_PspInsertThread)
//        {
//            if (hvgt::hook_function(Sys_PspInsertThread, PspInsertThread, NULL))
//            {
//                outLog("hook PspInsertThread成功.");
//            }
//            else
//            {
//                outLog("hook PspInsertThread失败.");
//            }
//        }
//        else
//        {
//            outLog("Sys_PspInsertThread为空指针.");
//        }
//    }
//}

// 旧项目里 Hook_DbgkCreateThread 曾被整段注释；重构后这里已经放开，并改为 HookEptRoutine 统一包装。
// 为什么放开：DbgkCreateThread 是调试线程创建事件的关键节点，当前调试对象事件链需要它参与。
// 放开的好处：能恢复/维持 CREATE_THREAD_DEBUG_EVENT 相关内核调试消息路径。
// 放开的坏处：符号偏移或系统版本不匹配时会影响调试事件分发，所以依赖 InitOffset/符号初始化正确。
// 项目影响：这是当前 SetupEptHook 默认启用项，不能随意关闭，否则调试线程创建事件可能不完整。
EXTERN_C
VOID Hook_DbgkCreateThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_DbgkCreateThread, DbgkCreateThread, NULL, "DbgkCreateThread", "DbgkCreateThread");
    }
}

EXTERN_C
VOID Hook_DbgkExitThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_DbgkExitThread, DbgkExitThread, NULL, "DbgkExitThread", "DbgkExitThread");
    }
}

EXTERN_C
VOID Hook_DbgkExitProcess()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_DbgkExitProcess, DbgkExitProcess, NULL, "DbgkExitProcess", "DbgkExitProcess");
    }
}

EXTERN_C
VOID Hook_PspCallThreadNotifyRoutines()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_PspCallThreadNotifyRoutines, NewPspCallThreadNotifyRoutines, (PVOID*)&Original_PspCallThreadNotifyRoutines, "PspCallThreadNotifyRoutines", "PspCallThreadNotifyRoutines");
    }
}

EXTERN_C
VOID Hook_NtTerminateProcess()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtTerminateProcess, NewNtTerminateProcess, (PVOID*)&Original_NtTerminateProcess, "NtTerminateProcess", "NtTerminateProcess");
    }
}

EXTERN_C
VOID Hook_PspExitThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_PspExitThread, PspExitThread, (PVOID*)&Original_PspExitThread, "PspExitThread", "PspExitThread");
        SetupHook_PspExitThread_CMP_Debugport();
    }
}

EXTERN_C
VOID Hook_PspCreateThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_PspCreateThread, PspCreateThread, (PVOID*)&Original_PspCreateThread, "PspCreateThread", "PspCreateThread");
    }
}

EXTERN_C
VOID Hook_NtCreateThreadEx()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtCreateThreadEx, NtCreateThreadEx, (PVOID*)&Original_NtCreateThreadEx, "NtCreateThreadEx", "NtCreateThreadEx");
    }
}

EXTERN_C
VOID Hook_PspAllocateThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_PspAllocateThread, NewPspAllocateThread, (PVOID*)&Original_PspAllocateThread, "PspAllocateThread", "PspAllocateThread");
    }
}

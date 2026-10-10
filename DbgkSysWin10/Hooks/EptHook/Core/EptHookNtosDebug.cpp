#include "EptHookCommon.h"

EXTERN_C
VOID Hook_NtCreateDebugObject()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtCreateDebugObject, NtCreateDebugObject, NULL, "NtCreateDebugObject", "NtCreateDebugObject");
    }
    else
    {
        outLog("Hook_NtCreateDebugObject 失败");
    }
}

EXTERN_C
VOID Hook_NtSetInformationDebugObject()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtSetInformationDebugObject, NtSetInformationDebugObject, NULL, "NtSetInformationDebugObject", "NtSetInformationDebugObject");
    }
}

EXTERN_C
VOID Hook_NtRemoveProcessDebug()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtRemoveProcessDebug, NtRemoveProcessDebug, NULL, "NtRemoveProcessDebug", "NtRemoveProcessDebug");
    }
}

EXTERN_C
VOID Hook_NtDebugActiveProcess()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtDebugActiveProcess, NtDebugActiveProcess, NULL, "NtDebugActiveProcess", "NtDebugActiveProcess");
    }
}

EXTERN_C
VOID Hook_NtWaitForDebugEvent()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtWaitForDebugEvent, NtWaitForDebugEvent, NULL, "NtWaitForDebugEvent", "NtWaitForDebugEvent");
    }
}

EXTERN_C
VOID Hook_KiDispatchException()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_KiDispatchException, KiDispatchException, (PVOID*)&Original_KiDispatchException, "KiDispatchException", "KiDispatchException");
    }
}

EXTERN_C
VOID Hook_PspInsertProcess()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_PspInsertProcess, PspInsertProcess, NULL, "PspInsertProcess", "PspInsertProcess");
    }
}

EXTERN_C
VOID Hook_NtDebugContinue()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtDebugContinue, NtDebugContinue, NULL, "NtDebugContinue", "NtDebugContinue");
    }
}

EXTERN_C
VOID Hook_DbgkMapViewOfSection()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_DbgkMapViewOfSection, DbgkMapViewOfSection, NULL, "DbgkMapViewOfSection", "DbgkMapViewOfSection");
    }
}

EXTERN_C
VOID Hook_DbgkUnMapViewOfSection()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_DbgkUnMapViewOfSection, DbgkUnMapViewOfSection, NULL, "DbgkUnMapViewOfSection", "DbgkUnMapViewOfSection");
    }
}

EXTERN_C
VOID Hook_DbgkForwardException()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_DbgkForwardException, DbgkForwardException, NULL, "DbgkForwardException", "DbgkForwardException");
    }
}

EXTERN_C
VOID Hook_DbgkpQueueMessage()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_DbgkpQueueMessage, DbgkpQueueMessage, NULL, "DbgkpQueueMessage", "DbgkpQueueMessage");
    }
}

// 旧项目保留的禁用路径：NtShutdownSystem 的 EPT hook wrapper。
// 为什么注释：它不属于调试对象、进程保护或断点核心链路，拦截关机 API 会扩大驱动行为边界。
// 放开的好处：可用于阻断/审计系统关机调用，保护调试会话。
// 放开的坏处：误拦截会影响系统关机、重启和测试机恢复流程，属于明显的系统行为修改。
// 项目影响：与 UnrealDbg.sln 当前调试功能目标无直接依赖，打开会增加安全软件和系统兼容风险。
// 结论：继续注释。
//EXTERN_C
//VOID Hook_NtShutdownSystem()
//{
//    if (g_IsInitGlobalVariable)
//    {
//        ASSERT(Sys_NtShutdownSystem);
//        if (Sys_NtShutdownSystem)
//        {
//            if (hvgt::hook_function(Sys_NtShutdownSystem, NtShutdownSystem, NULL))
//            {
//                outLog("hook NtShutdownSystem成功.");
//            }
//            else
//            {
//                outLog("hook NtShutdownSystem失败.");
//            }
//        }
//        else
//        {
//            outLog("Sys_NtShutdownSystem为空指针.");
//        }
//    }
//}

#include "EptHookCommon.h"

EXTERN_C
VOID Hook_KeStackAttachProcess()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_KeStackAttachProcess, NewKeStackAttachProcess, (PVOID*)&Original_KeStackAttachProcess, "KeStackAttachProcess", "KeStackAttachProcess");
    }
}

EXTERN_C
VOID Hook_KiStackAttachProcess()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_KiStackAttachProcess, NewKiStackAttachProcess, (PVOID*)&Original_KiStackAttachProcess, "KiStackAttachProcess", "KiStackAttachProcess");
    }
}

EXTERN_C
VOID Hook_NtProtectVirtualMemory()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtProtectVirtualMemory, NtProtectVirtualMemory, (PVOID*)&Original_NtProtectVirtualMemory, "NtProtectVirtualMemory", "NtProtectVirtualMemory");
    }
}

EXTERN_C
VOID Hook_MiObtainReferencedVadEx()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_MiObtainReferencedVadEx, MiObtainReferencedVadEx, (PVOID*)&Original_MiObtainReferencedVadEx, "MiObtainReferencedVadEx", "MiObtainReferencedVadEx");
    }
}

EXTERN_C
VOID Hook_MmProtectVirtualMemory()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_MmProtectVirtualMemory, MmProtectVirtualMemory, (PVOID*)&Original_MmProtectVirtualMemory, "MmProtectVirtualMemory", "MmProtectVirtualMemory");
    }
}

EXTERN_C
VOID Hook_NtGetContextThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtGetContextThread, NtGetContextThread, (PVOID*)&Original_NtGetContextThread, "NtGetContextThread", "NtGetContextThread");
    }
}

EXTERN_C
VOID Hook_NtSetContextThread()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtSetContextThread, NtSetContextThread, (PVOID*)&Original_NtSetContextThread, "NtSetContextThread", "NtSetContextThread");
    }
}

EXTERN_C
VOID Hook_NtOpenProcess()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtOpenProcess, NewNtOpenProcess, (PVOID*)&Original_NtOpenProcess, "NtOpenProcess", "NtOpenProcess");
    }
}

EXTERN_C
VOID Hook_NtReadVirtualMemory()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtReadVirtualMemory, NtReadVirtualMemory, (PVOID*)&Original_NtReadVirtualMemory, "NtReadVirtualMemory", "NtReadVirtualMemory");
    }
}

EXTERN_C
VOID Hook_NtWriteVirtualMemory()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_NtWriteVirtualMemory, NtWriteVirtualMemory, (PVOID*)&Original_NtWriteVirtualMemory, "NtWriteVirtualMemory", "NtWriteVirtualMemory");
    }
}

EXTERN_C
VOID Hook_ObReferenceObjectByHandle()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_ObReferenceObjectByHandle, NewObReferenceObjectByHandle, (PVOID*)&Original_ObReferenceObjectByHandle, "ObReferenceObjectByHandle", "ObReferenceObjectByHandle");
    }
}

EXTERN_C
VOID Hook_ObReferenceObjectByHandleWithTag()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_ObReferenceObjectByHandleWithTag, NewObReferenceObjectByHandleWithTag, (PVOID*)&Original_ObReferenceObjectByHandleWithTag, "ObReferenceObjectByHandleWithTag", "ObReferenceObjectByHandleWithTag");
    }
}

EXTERN_C
VOID Hook_ObpReferenceObjectByHandleWithTag()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_ObpReferenceObjectByHandleWithTag, NewObpReferenceObjectByHandleWithTag, (PVOID*)&Original_ObpReferenceObjectByHandleWithTag, "ObpReferenceObjectByHandleWithTag", "ObpReferenceObjectByHandleWithTag");
    }
}

EXTERN_C
VOID Hook_ObfDereferenceObjectWithTag()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_ObfDereferenceObjectWithTag, NewObfDereferenceObjectWithTag, (PVOID*)&Original_ObfDereferenceObjectWithTag, "ObfDereferenceObjectWithTag", "ObfDereferenceObjectWithTag");
    }
}

EXTERN_C
VOID Hook_ObfDereferenceObject()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_ObfDereferenceObject, NewObfDereferenceObject, (PVOID*)&Original_ObfDereferenceObject, "ObfDereferenceObject", "ObfDereferenceObject");
    }
}

EXTERN_C
VOID Hook_MmCopyVirtualMemory()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_MmCopyVirtualMemory, NewMmCopyVirtualMemory, (PVOID*)&Original_MmCopyVirtualMemory, "MmCopyVirtualMemory", "MmCopyVirtualMemory");
    }
}

EXTERN_C
VOID Hook_PspCreateUserContext()
{
    if (g_IsInitGlobalVariable)
    {
        HookEptRoutine(Sys_PspCreateUserContext, NewPspCreateUserContext, (PVOID*)&Original_PspCreateUserContext, "PspCreateUserContext", "PspCreateUserContext");
    }
}

// 旧项目保留的禁用路径：DbgkpCloseObject 的 EPT hook wrapper。
// 为什么注释：DbgkpCloseObject 贴近调试对象生命周期尾部，打开后会干预对象释放/引用计数相关路径；当前事件链已经通过
// DbgkpQueueMessage、NtRemoveProcessDebug、NtDebugContinue 等入口覆盖主要调试对象行为。
// 放开的好处：可观察调试对象关闭，辅助定位调试器退出、DebugPort 清理问题。
// 放开的坏处：对象关闭路径一旦处理错误，可能造成引用泄漏、对象提前释放或目标进程调试状态残留。
// 项目影响：属于调试对象生命周期高风险 hook，默认启用会提升崩溃和资源泄漏风险。
// 结论：继续注释。
//EXTERN_C
//VOID Hook_DbgkpCloseObject()
//{
//    if (g_IsInitGlobalVariable)
//    {
//        ASSERT(Sys_DbgkpCloseObject);
//        if (Sys_DbgkpCloseObject)
//        {
//            if (hvgt::hook_function(Sys_DbgkpCloseObject, DbgkpCloseObject, NULL))
//            {
//                outLog("hook DbgkpCloseObject成功.");
//            }
//            else
//            {
//                outLog("hook DbgkpCloseObject失败.");
//            }
//        }
//        else
//        {
//            outLog("Sys_DbgkpCloseObject为空指针.");
//        }
//    }
//}

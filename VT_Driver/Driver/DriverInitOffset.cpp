#include "DriverInternal.h"
#include <Shared/DriverProtocol.h>

#ifdef _DEBUG_Key_Log
// ================================
// InitOffset 延后打印所需的全局状态
// ================================
unsigned __int64 verifyKeyold = 0;
volatile LONG g_InitOffsetLogPending = 0;
const char* g_InitOffsetLastMessage = nullptr;
size_t g_InitOffsetLastCid = 0;
size_t g_InitOffsetLastDirectoryTableBase = 0;
HANDLE g_InitOffsetLogThreadHandle = nullptr;
volatile LONG g_StopInitOffsetLogThread = 0;

bool InitOffset(PWINDOWS_STRUCT vmcallinfo, unsigned __int64 verifyKey) // 从来宾传入结构初始化关键偏移（fallback）
{
    if (verifyKey != VT_INIT_HANDSHAKE_KEY)
    {
        g_InitOffsetLastCid = 0;
        g_InitOffsetLastDirectoryTableBase = 0;
        g_InitOffsetLastMessage = "InitOffset 失败：verifyKey 不匹配";
        InterlockedExchange(&g_InitOffsetLogPending, 1);
        return false;
    }
    verifyKeyold = verifyKey;

    if (vmcallinfo == nullptr)
    {
        g_InitOffsetLastCid = 0;
        g_InitOffsetLastDirectoryTableBase = 0;
        g_InitOffsetLastMessage = "InitOffset 失败：vmcallinfo 为空";
        InterlockedExchange(&g_InitOffsetLogPending, 1);
        return false;
    }

    WINDOWS_STRUCT tmp_vmcallinfo = { 0 };
    if (sizeof(WINDOWS_STRUCT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(WINDOWS_STRUCT)))
    {
        g_InitOffsetLastCid = 0;
        g_InitOffsetLastDirectoryTableBase = 0;
        g_InitOffsetLastMessage = "InitOffset 失败：read_guest_virtual_memory 读取失败";
        InterlockedExchange(&g_InitOffsetLogPending, 1);
        return false;
    }

    if (tmp_vmcallinfo.command != VMCALL_INIT_OFFSET)
    {
        g_InitOffsetLastCid = 0;
        g_InitOffsetLastDirectoryTableBase = 0;
        g_InitOffsetLastMessage = "InitOffset 失败：command 不是 VMCALL_INIT_OFFSET";
        InterlockedExchange(&g_InitOffsetLogPending, 1);
        return false;
    }

    if (tmp_vmcallinfo.ethread_offset_Cid == 0)
    {
        g_InitOffsetLastCid = 0;
        g_InitOffsetLastDirectoryTableBase = 0;
        g_InitOffsetLastMessage = "InitOffset 失败：remoteCid 无效";
        InterlockedExchange(&g_InitOffsetLogPending, 1);
        return false;
    }

    const size_t remoteCid = static_cast<size_t>(tmp_vmcallinfo.ethread_offset_Cid);
    const size_t remoteDirectoryTableBase =
        static_cast<size_t>(tmp_vmcallinfo.kprocess_offset_DirectoryTableBase);

    ethread_offset::Cid = remoteCid;

    if (tmp_vmcallinfo.kprocess_offset_DirectoryTableBase != 0)
    {
        hv::ghv.kprocess_directory_table_base_offset = tmp_vmcallinfo.kprocess_offset_DirectoryTableBase;
    }

    if (tmp_vmcallinfo.eprocess_offset_UniqueProcessId != 0)
    {
        hv::ghv.eprocess_unique_process_id_offset = tmp_vmcallinfo.eprocess_offset_UniqueProcessId;
    }

    if (tmp_vmcallinfo.eprocess_offset_ImageFileName != 0)
    {
        hv::ghv.eprocess_image_file_name = tmp_vmcallinfo.eprocess_offset_ImageFileName;
    }

    if (tmp_vmcallinfo.kthread_offset_ApcState != 0)
    {
        hv::ghv.kthread_apc_state_offset = tmp_vmcallinfo.kthread_offset_ApcState;
    }

    if (tmp_vmcallinfo.kapc_state_offset_Process != 0)
    {
        hv::ghv.kapc_state_process_offset = tmp_vmcallinfo.kapc_state_offset_Process;
    }

    g_InitOffsetLastCid = remoteCid;
    g_InitOffsetLastDirectoryTableBase = remoteDirectoryTableBase;
    g_InitOffsetLastMessage = "InitOffset 已初始化";
    InterlockedExchange(&g_InitOffsetLogPending, 1);
    return true;
}

VOID InitOffsetLogThread(_In_ PVOID StartContext)
{
    UNREFERENCED_PARAMETER(StartContext);

    LARGE_INTEGER interval = {};
    interval.QuadPart = -10 * 1000 * 100; // 100ms，相对时间

    for (;;)
    {
        if (InterlockedCompareExchange(&g_StopInitOffsetLogThread, 0, 0) != 0)
        {
            break;
        }

        // 有待打印日志时，由普通系统线程统一输出。
        if (InterlockedExchange(&g_InitOffsetLogPending, 0) != 0)
        {
            if (g_InitOffsetLastMessage != nullptr)
            {
                outDebug("%s\n", g_InitOffsetLastMessage);
                if (g_InitOffsetLastCid != 0)
                {
                    outDebug("InitOffset ETHREAD.Cid：%p DirectoryTableBase：%p\n",
                        (PVOID)g_InitOffsetLastCid,
                        (PVOID)g_InitOffsetLastDirectoryTableBase);
                    outDebug("VT 驱动得到KEY : 0x%llX\n", verifyKeyold);
                }
            }
        }

        KeDelayExecutionThread(KernelMode, FALSE, &interval);
    }

    PsTerminateSystemThread(STATUS_SUCCESS);
}

#else

bool InitOffset(PWINDOWS_STRUCT vmcallinfo, unsigned __int64 verifyKey) // 从来宾传入结构初始化关键偏移（fallback）
{
    if (verifyKey != VT_INIT_HANDSHAKE_KEY)
    {
        //sLog("VT 驱动得到的错误KEY : 0x%llX\n", verifyKey);
        return false;
    }

    if (vmcallinfo == nullptr)
    {
        return false;
    }

    WINDOWS_STRUCT tmp_vmcallinfo = { 0 };
    if (sizeof(WINDOWS_STRUCT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(WINDOWS_STRUCT)))
    {
        return false;
    }

    if (tmp_vmcallinfo.command != VMCALL_INIT_OFFSET || tmp_vmcallinfo.ethread_offset_Cid == 0)
    {
        return false;
    }

    ethread_offset::Cid = static_cast<size_t>(tmp_vmcallinfo.ethread_offset_Cid);

    if (tmp_vmcallinfo.kprocess_offset_DirectoryTableBase != 0)
    {
        hv::ghv.kprocess_directory_table_base_offset = tmp_vmcallinfo.kprocess_offset_DirectoryTableBase;
    }

    if (tmp_vmcallinfo.eprocess_offset_UniqueProcessId != 0)
    {
        hv::ghv.eprocess_unique_process_id_offset = tmp_vmcallinfo.eprocess_offset_UniqueProcessId;
    }

    if (tmp_vmcallinfo.eprocess_offset_ImageFileName != 0)
    {
        hv::ghv.eprocess_image_file_name = tmp_vmcallinfo.eprocess_offset_ImageFileName;
    }

    if (tmp_vmcallinfo.kthread_offset_ApcState != 0)
    {
        hv::ghv.kthread_apc_state_offset = tmp_vmcallinfo.kthread_offset_ApcState;
    }

    if (tmp_vmcallinfo.kapc_state_offset_Process != 0)
    {
        hv::ghv.kapc_state_process_offset = tmp_vmcallinfo.kapc_state_offset_Process;
    }

    return true;
}

#endif

// 旧项目保留的禁用路径：InitOffset fallback 只从来宾 WINDOWS_STRUCT 中读取 ETHREAD.Cid，并在本地偏移未初始化时赋值。
// 为什么注释：当前实现要求 verifyKey == VT_INIT_HANDSHAKE_KEY，并校验 command == VMCALL_INIT_OFFSET，同时同步
// DirectoryTableBase、UniqueProcessId、ImageFileName、ApcState 等多个偏移；旧 fallback 缺少握手校验和完整偏移同步。
// 放开的好处：当旧版 DbgkSys 只下发 Cid 时，VT_Driver 仍可最低限度运行 GuestCurrentThreadCid。
// 放开的坏处：任何来宾都可能伪造结构初始化关键偏移，且 CR3/进程字段缺失会影响断点、进程识别和内存转换。
// 项目影响：会削弱 VT 初始化握手，降低 Win10/Win11 驱动与 VT_Driver 的协议一致性。
// 结论：继续注释，不放开；如需兼容旧协议，应加明确版本号和最小字段集校验。
//bool InitOffset(PWINDOWS_STRUCT vmcallinfo, unsigned __int64 verifyKey)
//{
//    WINDOWS_STRUCT tmp_vmcallinfo = { 0 };
//
//    if (sizeof(WINDOWS_STRUCT) != hv::read_guest_virtual_memory(vmcallinfo, &tmp_vmcallinfo, sizeof(WINDOWS_STRUCT)))
//    {
//        return false;
//    }
//
//    constexpr size_t InvalidOffset = static_cast<size_t>(0xFFFFFFFF);
//
//    if ((ethread_offset::Cid == 0 || ethread_offset::Cid == InvalidOffset) &&
//        tmp_vmcallinfo.ethread_offset_Cid != 0 &&
//        tmp_vmcallinfo.ethread_offset_Cid != InvalidOffset)
//    {
//        ethread_offset::Cid = static_cast<size_t>(tmp_vmcallinfo.ethread_offset_Cid);
//    }
//
//    return ethread_offset::Cid != 0 && ethread_offset::Cid != InvalidOffset;
//}

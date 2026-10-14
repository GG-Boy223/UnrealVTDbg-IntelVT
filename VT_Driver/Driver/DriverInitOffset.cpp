#include "DriverInternal.h"
#include <Shared/DriverProtocol.h>


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

    if (tmp_vmcallinfo.command != VMCALL_INIT_OFFSET)
    {
        return false;
    }

    // 协议版本闸门：Win10/Win11 的 DbgkSys 都会填充 WINDOWS_STRUCT 的
    // magic/version/size/source。版本不匹配时字段布局/语义可能不同，
    // 必须拒绝握手，禁止把错误偏移静默装进 VMM（旧逻辑只校验 verifyKey/command）。
    if (tmp_vmcallinfo.magic != WINDOWS_OFFSET_MAGIC ||
        tmp_vmcallinfo.version != WINDOWS_OFFSET_VERSION ||
        tmp_vmcallinfo.size != sizeof(WINDOWS_STRUCT) ||
        (tmp_vmcallinfo.source != WINDOWS_OFFSET_SOURCE_WIN10 &&
            tmp_vmcallinfo.source != WINDOWS_OFFSET_SOURCE_WIN11))
    {
        outFile("InitOffset 失败：WINDOWS_STRUCT 协议版本不匹配。\n");
        return false;
    }

    // Cid 是本握手中唯一必需的偏移：必须落在协议允许范围内，
    // 避免 GuestCurrentThreadCid() 用损坏偏移解引用 ETHREAD。
    if (tmp_vmcallinfo.ethread_offset_Cid == 0 ||
        tmp_vmcallinfo.ethread_offset_Cid == WINDOWS_OFFSET_INVALID ||
        tmp_vmcallinfo.ethread_offset_Cid > WINDOWS_OFFSET_REASONABLE_MAX)
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

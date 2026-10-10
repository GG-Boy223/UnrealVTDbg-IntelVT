#include "../Core/DebugBreakInternal.h"

// 调整说明：旧硬件断点路径会在 KeStackAttachProcess 后强读目标地址、再 _disable/_enable 读取 CR3，并在 attach 状态内执行 VMCALL；CE 查找地址访问时异常频繁，容易造成 APC_INDEX_MISMATCH。
// 旧实现先保留在这里，方便回溯和对照；当前版本不再编译这段逻辑。
bool AddHardwareBreakpoint(PBREAKPOINT_RECORD Breakpoint)
{
    if (Breakpoint == NULL || Breakpoint->Address == 0)
    {
        //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
        if (Breakpoint != NULL)
        {
            Breakpoint->errorCode = 99;
        }
        return false;
    }

    VT_BREAK_POINT vmcallinfo = { 0 };
    vmcallinfo.cr3 = Breakpoint->cr3;
    vmcallinfo.VirtualAddress = Breakpoint->Address;
    vmcallinfo.Size = Breakpoint->length;
    vmcallinfo.command = Breakpoint->command;
    vmcallinfo.CPUCount = Breakpoint->CPUCount;
    vmcallinfo.LoopUserMode = Breakpoint->LoopUserMode;
    vmcallinfo.pid = Breakpoint->pid;
    vmcallinfo.debugRegister = Breakpoint->debugRegister;
    vmcallinfo.watchid = -1;

    NTSTATUS status = CaptureProcessCr3(Breakpoint->pid, Breakpoint->cr3, &vmcallinfo.cr3);
    if (!NT_SUCCESS(status) || vmcallinfo.cr3 == 0)
    {
        Breakpoint->watchid = -1;
        Breakpoint->errorCode = 90;
        return false;
    }

    const bool boSuccess = current_vmcall(&vmcallinfo);
    Breakpoint->watchid = boSuccess ? vmcallinfo.watchid : -1;
    Breakpoint->errorCode = boSuccess ? 0 : vmcallinfo.errorCode;
    if (!boSuccess && Breakpoint->errorCode == 0)
    {
        Breakpoint->errorCode = 91;
    }

    return boSuccess;
}

void SetHardwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    PBREAKPOINT_RECORD output = (PBREAKPOINT_RECORD)pIrp->AssociatedIrp.SystemBuffer; //内核的缓冲区，输入输出都用的这个
    RtlZeroMemory(output, sizeof(BREAKPOINT_RECORD));
    output->watchid = -1; //即使没有可处理记录，也必须向用户态表达“未安装成功”

    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(
        user,
        sizeof(BREAKPOINT_RECORD),
        &numElements);
    if (aucPlainText == nullptr)
    {
        output->errorCode = 520;
        return;
    }

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PBREAKPOINT_RECORD pInfo = reinterpret_cast<PBREAKPOINT_RECORD>(aucPlainText + i * sizeof(BREAKPOINT_RECORD));

        if (AddHardwareBreakpoint(pInfo))
        {
            *output = *pInfo; //成功
        }
        else
        {
            *output = *pInfo; //保留驱动填写的失败记录
            output->watchid = -1;
        }
        break;
    }

    free_pool(aucPlainText);
}

bool DeleteHardwareBreakpoint(PBREAKPOINT_RECORD Breakpoint)
{
    if (Breakpoint == NULL || Breakpoint->watchid < 0)
    {
        //outLog("DispatchOffsetToHost key mismatch: 0x%llX\n", verifyKey);
        if (Breakpoint != NULL)
        {
            Breakpoint->errorCode = 99;
        }
        return false;
    }

    VT_BREAK_POINT vmcallinfo = { 0 };
    vmcallinfo.cr3 = Breakpoint->cr3;
    vmcallinfo.VirtualAddress = Breakpoint->Address;
    vmcallinfo.Size = Breakpoint->length;
    vmcallinfo.command = VMCALL_WATCH_DELETE;
    vmcallinfo.LoopUserMode = Breakpoint->LoopUserMode;
    vmcallinfo.watchid = Breakpoint->watchid;
    vmcallinfo.CPUCount = Breakpoint->CPUCount;

    const bool success = current_vmcall(&vmcallinfo);
    Breakpoint->errorCode = success ? 0 : vmcallinfo.errorCode;
    if (!success && Breakpoint->errorCode == 0)
    {
        Breakpoint->errorCode = 91;
    }
    return success;
}

//移除硬件断点
void RemoveHardwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    PHOOK_DEBUG_RESULT output = (PHOOK_DEBUG_RESULT)pIrp->AssociatedIrp.SystemBuffer; //内核的缓冲区，输入输出都用的这个
    RtlZeroMemory(output, sizeof(HOOK_DEBUG_RESULT));

    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(
        user,
        sizeof(BREAKPOINT_RECORD),
        &numElements);
    if (aucPlainText == nullptr)
    {
        output->status = 520;
        output->errorCode = 520;
        return;
    }

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PBREAKPOINT_RECORD pInfo = reinterpret_cast<PBREAKPOINT_RECORD>(aucPlainText + i * sizeof(BREAKPOINT_RECORD));

        if (DeleteHardwareBreakpoint(pInfo))
        {
            output->status = 1998; //成功
            output->errorCode = pInfo->errorCode;
        }
        else
        {
            output->status = 520; //失败
            output->errorCode = pInfo->errorCode;
        }
        break;
    }

    free_pool(aucPlainText);
}

#include "../Core/DebugBreakInternal.h"

// 调整说明：旧硬件断点路径会在 KeStackAttachProcess 后强读目标地址、再 _disable/_enable 读取 CR3，并在 attach 状态内执行 VMCALL；CE 查找地址访问时异常频繁，容易造成 APC_INDEX_MISMATCH。
// 旧实现先保留在这里，方便回溯和对照；当前版本不再编译这段逻辑。
bool AddHardwareBreakpoint(PBREAKPOINT_RECORD Breakpoint)
{
    if (Breakpoint == NULL || Breakpoint->Address == 0)
    {
        //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
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
    vmcallinfo.watchid = -1;

    NTSTATUS status = CaptureProcessCr3(Breakpoint->pid, Breakpoint->cr3, &vmcallinfo.cr3);
    if (!NT_SUCCESS(status) || vmcallinfo.cr3 == 0)
    {
        Breakpoint->watchid = -1;
        outLog("\xBB\xF1\xC8\xA1\xC4\xBF\xB1\xEA" "CR3\xCA\xA7\xB0\xDC\xA3\xAC\xD3\xB2\xBC\xFE\xB6\xCF\xB5\xE3\xCE\xB4\xC9\xE8\xD6\xC3 status:0x%X", status);
        return false;
    }

    const bool boSuccess = current_vmcall(&vmcallinfo);
    Breakpoint->watchid = boSuccess ? vmcallinfo.watchid : -1;

    if (!boSuccess)
    {
        outLog("current_vmcall \xCA\xA7\xB0\xDC!  errorCode:%d", vmcallinfo.errorCode);
    }
    else
    {
        outLog("current_vmcall \xB3\xC9\xB9\xA6!  errorCode:%d", vmcallinfo.errorCode);
    }

    return boSuccess;
}

void SetHardwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    PBREAKPOINT_RECORD output = (PBREAKPOINT_RECORD)pIrp->AssociatedIrp.SystemBuffer; //内核的缓冲区，输入输出都用的这个
    RtlZeroMemory(output, sizeof(BREAKPOINT_RECORD));

    //分配明文缓存区
    BYTE* aucPlainText = allocate_pool<BYTE*>(AlignUserDataToBlowfishBlock(user.uSize));
    DecryptData((PVOID)user.pUserData, aucPlainText);

    // 计算明文缓冲区中的结构体数量
    size_t numElements = user.uSize / sizeof(BREAKPOINT_RECORD);

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PBREAKPOINT_RECORD pInfo = reinterpret_cast<PBREAKPOINT_RECORD>(aucPlainText + i * sizeof(BREAKPOINT_RECORD));

        if (AddHardwareBreakpoint(pInfo))
        {
            *output = *pInfo; //成功
        }
        break;
    }

    free_pool(aucPlainText);
}

bool DeleteHardwareBreakpoint(PBREAKPOINT_RECORD Breakpoint)
{
    if (Breakpoint == NULL || Breakpoint->watchid < 0)
    {
        //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
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

    return current_vmcall(&vmcallinfo);
}

//移除硬件断点
void RemoveHardwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    DWORD* output = (DWORD*)pIrp->AssociatedIrp.SystemBuffer; //内核的缓冲区，输入输出都用的这个
    RtlZeroMemory(output, sizeof(DWORD));

    //分配明文缓存区
    BYTE* aucPlainText = allocate_pool<BYTE*>(AlignUserDataToBlowfishBlock(user.uSize));
    DecryptData((PVOID)user.pUserData, aucPlainText);

    // 计算明文缓冲区中的结构体数量
    size_t numElements = user.uSize / sizeof(BREAKPOINT_RECORD);

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PBREAKPOINT_RECORD pInfo = reinterpret_cast<PBREAKPOINT_RECORD>(aucPlainText + i * sizeof(BREAKPOINT_RECORD));

        if (DeleteHardwareBreakpoint(pInfo))
        {
            *output = 1998; //成功
        }
        else
        {
            *output = 520; //失败
        }
        break;
    }

    free_pool(aucPlainText);
}

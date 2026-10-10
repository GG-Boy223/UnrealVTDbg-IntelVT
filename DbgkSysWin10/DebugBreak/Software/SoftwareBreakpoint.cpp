#include "../Core/DebugBreakInternal.h"

// 调整说明：旧软件断点添加路径同样在 attach 状态内探测目标地址并关中断，异常路径不利于保持 APC/attach 平衡。
// 旧实现先保留在这里，方便回溯和对照；当前版本不再编译这段逻辑。
bool AddSoftwareBreakpoint(PVT_BREAK_POINT vmcallinfo)
{
    if (vmcallinfo == NULL || vmcallinfo->VirtualAddress == 0)
    {
        //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
        if (vmcallinfo != NULL)
        {
            vmcallinfo->errorCode = 99;
        }
        return false;
    }

    NTSTATUS status = CaptureProcessCr3(vmcallinfo->pid, vmcallinfo->cr3, &vmcallinfo->cr3);
    if (!NT_SUCCESS(status) || vmcallinfo->cr3 == 0)
    {
        vmcallinfo->errorCode = 90;
        return false;
    }

    const bool boSuccess = current_vmcall(vmcallinfo);
    if (!boSuccess && vmcallinfo->errorCode == 0)
    {
        vmcallinfo->errorCode = 91;
    }
    return boSuccess;
}

void SetSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    PVT_BREAK_POINT output = (PVT_BREAK_POINT)pIrp->AssociatedIrp.SystemBuffer; //内核的缓冲区，输入输出都用的这个
    RtlZeroMemory(output, sizeof(VT_BREAK_POINT));
    output->watchid = -1;

    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(
        user,
        sizeof(VT_BREAK_POINT),
        &numElements);
    if (aucPlainText == nullptr)
    {
        output->errorCode = 520; // 解密失败,明确回写错误码。
        return;
    }

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PVT_BREAK_POINT pInfo = reinterpret_cast<PVT_BREAK_POINT>(aucPlainText + i * sizeof(VT_BREAK_POINT));

        if (AddSoftwareBreakpoint(pInfo))
        {
            *output = *pInfo; //成功
        }
        else
        {
            *output = *pInfo; //保留驱动填写的失败原因
            output->watchid = -1;
            if (output->errorCode == 0)
            {
                output->errorCode = 520; //设置失败
            }
        }
        break;
    }

    free_pool(aucPlainText);
}

// 调整说明：旧软件断点删除路径在 try/except 两边手动 detach，若 attach 未完成或异常点变化，存在重复 detach/失衡风险。
// 旧实现先保留在这里，方便回溯和对照；当前版本不再编译这段逻辑。
bool DeleteSoftwareBreakpoint(PVT_BREAK_POINT vmcallinfo)
{
    if (vmcallinfo == NULL)
    {
        //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
        return false;
    }

    // 删除只按 watchid + VT 侧 watch 表执行（ept_watch_deactivate 不消费 cr3）。
    // 目标进程闪退/退出后 CaptureProcessCr3 必然失败，不能因此阻止删除，
    // 否则隐藏 INT3 的 EPT watch 会残留在物理页上。
    const unsigned __int64 recordedCr3 = vmcallinfo->cr3;
    NTSTATUS status = CaptureProcessCr3(vmcallinfo->pid, recordedCr3, &vmcallinfo->cr3);
    if (!NT_SUCCESS(status) || vmcallinfo->cr3 == 0)
    {
        if (recordedCr3 == 0)
        {
            vmcallinfo->errorCode = 90;
            outLog("获取目标" "CR3失败，软件断点未删除 status:0x%X", status);
            return false;
        }

        vmcallinfo->cr3 = recordedCr3;
    }

    const bool success = current_vmcall(vmcallinfo);
    vmcallinfo->errorCode = success ? 0 : vmcallinfo->errorCode;
    if (!success && vmcallinfo->errorCode == 0)
    {
        vmcallinfo->errorCode = 91;
    }
    return success;
}

//移除软件断点
void RemoveSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    PHOOK_DEBUG_RESULT output = (PHOOK_DEBUG_RESULT)pIrp->AssociatedIrp.SystemBuffer; //内核的缓冲区，输入输出都用的这个
    RtlZeroMemory(output, sizeof(HOOK_DEBUG_RESULT));

    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(
        user,
        sizeof(VT_BREAK_POINT),
        &numElements);
    if (aucPlainText == nullptr)
    {
        output->status = 520; // 解密失败,明确回写错误码。
        output->errorCode = 520;
        return;
    }

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PVT_BREAK_POINT pInfo = reinterpret_cast<PVT_BREAK_POINT>(aucPlainText + i * sizeof(VT_BREAK_POINT));

        if (DeleteSoftwareBreakpoint(pInfo))
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

// 调整说明：旧软件断点读取路径在 attach 后探测地址并关中断，读取场景不需要触碰目标地址。
// 旧实现先保留在这里，方便回溯和对照；当前版本不再编译这段逻辑。
bool GetSoftwareBreakpoint(PVT_BREAK_POINT vmcallinfo)
{
    if (vmcallinfo == NULL)
    {
        //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
        return false;
    }

    NTSTATUS status = CaptureProcessCr3(vmcallinfo->pid, vmcallinfo->cr3, &vmcallinfo->cr3);
    if (!NT_SUCCESS(status) || vmcallinfo->cr3 == 0)
    {
        outLog("获取目标" "CR3失败，软件断点未读取 status:0x%X", status);
        return false;
    }

    const bool boSuccess = current_vmcall(vmcallinfo);
    if (boSuccess)
    {
        vmcallinfo->errorCode = 1998; // 与现有用户态协议保持一致。
    }
    return boSuccess;
}

//读int3断点
void ReadSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    PVT_BREAK_POINT output = (PVT_BREAK_POINT)pIrp->AssociatedIrp.SystemBuffer; //内核的缓冲区，输入输出都用的这个
    RtlZeroMemory(output, sizeof(VT_BREAK_POINT));

    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(
        user,
        sizeof(VT_BREAK_POINT),
        &numElements);
    if (aucPlainText == nullptr)
    {
        output->errorCode = 520; // 解密失败,明确回写错误码。
        return;
    }

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PVT_BREAK_POINT pInfo = reinterpret_cast<PVT_BREAK_POINT>(aucPlainText + i * sizeof(VT_BREAK_POINT));

        if (GetSoftwareBreakpoint(pInfo))
        {
            *output = *pInfo; //成功
        }
        else
        {
            output->errorCode = 520; //读取失败
        }
        break;
    }

    free_pool(aucPlainText);
}

#include "../Core/DebugBreakInternal.h"

// 调整说明：旧软件断点添加路径同样在 attach 状态内探测目标地址并关中断，异常路径不利于保持 APC/attach 平衡。
// 旧实现先保留在这里，方便回溯和对照；当前版本不再编译这段逻辑。
bool AddSoftwareBreakpoint(PVT_BREAK_POINT vmcallinfo)
{
    if (vmcallinfo == NULL || vmcallinfo->VirtualAddress == 0)
    {
        //outLog("DispatchOffsetToHost 密钥错误 : 0x%llX\n", verifyKey);
        return false;
    }

    NTSTATUS status = CaptureProcessCr3(vmcallinfo->pid, vmcallinfo->cr3, &vmcallinfo->cr3);
    if (!NT_SUCCESS(status) || vmcallinfo->cr3 == 0)
    {
        outLog("\xBB\xF1\xC8\xA1\xC4\xBF\xB1\xEA" "CR3\xCA\xA7\xB0\xDC\xA3\xAC\xC8\xED\xBC\xFE\xB6\xCF\xB5\xE3\xCE\xB4\xC9\xE8\xD6\xC3 status:0x%X", status);
        return false;
    }

    return current_vmcall(vmcallinfo);
}

void SetSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
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

    NTSTATUS status = CaptureProcessCr3(vmcallinfo->pid, vmcallinfo->cr3, &vmcallinfo->cr3);
    if (!NT_SUCCESS(status) || vmcallinfo->cr3 == 0)
    {
        outLog("\xBB\xF1\xC8\xA1\xC4\xBF\xB1\xEA" "CR3\xCA\xA7\xB0\xDC\xA3\xAC\xC8\xED\xBC\xFE\xB6\xCF\xB5\xE3\xCE\xB4\xC9\xBE\xB3\xFD status:0x%X", status);
        return false;
    }

    return current_vmcall(vmcallinfo);
}

//移除软件断点
void RemoveSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    DWORD* output = (DWORD*)pIrp->AssociatedIrp.SystemBuffer; //内核的缓冲区，输入输出都用的这个
    RtlZeroMemory(output, sizeof(DWORD));

    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(
        user,
        sizeof(VT_BREAK_POINT),
        &numElements);
    if (aucPlainText == nullptr)
    {
        return;
    }

    // 遍历明文缓冲区中的结构体
    for (size_t i = 0; i < numElements; i++)
    {
        PVT_BREAK_POINT pInfo = reinterpret_cast<PVT_BREAK_POINT>(aucPlainText + i * sizeof(VT_BREAK_POINT));

        if (DeleteSoftwareBreakpoint(pInfo))
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
        outLog("\xBB\xF1\xC8\xA1\xC4\xBF\xB1\xEA" "CR3\xCA\xA7\xB0\xDC\xA3\xAC\xC8\xED\xBC\xFE\xB6\xCF\xB5\xE3\xCE\xB4\xB6\xC1\xC8\xA1 status:0x%X", status);
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
        break;
    }

    free_pool(aucPlainText);
}

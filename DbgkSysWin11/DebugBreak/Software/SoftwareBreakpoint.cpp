#include "../Core/DebugBreakInternal.h"

bool AddSoftwareBreakpoint(PVT_BREAK_POINT vmcallinfo)
{
    if (vmcallinfo == NULL || vmcallinfo->VirtualAddress == 0)
    {
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

    PVT_BREAK_POINT output = (PVT_BREAK_POINT)pIrp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(output, sizeof(VT_BREAK_POINT));

    BYTE* aucPlainText = allocate_pool<BYTE*>(AlignUserDataToBlowfishBlock(user.uSize));
    DecryptData((PVOID)user.pUserData, aucPlainText);

    size_t numElements = user.uSize / sizeof(VT_BREAK_POINT);

    for (size_t i = 0; i < numElements; i++)
    {
        PVT_BREAK_POINT pInfo = reinterpret_cast<PVT_BREAK_POINT>(aucPlainText + i * sizeof(VT_BREAK_POINT));

        if (AddSoftwareBreakpoint(pInfo))
        {
            *output = *pInfo;
        }
        break;
    }

    free_pool(aucPlainText);
}

bool DeleteSoftwareBreakpoint(PVT_BREAK_POINT vmcallinfo)
{
    if (vmcallinfo == NULL)
    {
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

void RemoveSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    DWORD* output = (DWORD*)pIrp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(output, sizeof(DWORD));

    BYTE* aucPlainText = allocate_pool<BYTE*>(AlignUserDataToBlowfishBlock(user.uSize));
    DecryptData((PVOID)user.pUserData, aucPlainText);

    size_t numElements = user.uSize / sizeof(VT_BREAK_POINT);

    for (size_t i = 0; i < numElements; i++)
    {
        PVT_BREAK_POINT pInfo = reinterpret_cast<PVT_BREAK_POINT>(aucPlainText + i * sizeof(VT_BREAK_POINT));

        if (DeleteSoftwareBreakpoint(pInfo))
        {
            *output = 1998;
        }
        else
        {
            *output = 520;
        }
        break;
    }

    free_pool(aucPlainText);
}

bool GetSoftwareBreakpoint(PVT_BREAK_POINT vmcallinfo)
{
    if (vmcallinfo == NULL)
    {
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
        vmcallinfo->errorCode = 1998;
    }
    return boSuccess;
}

void ReadSoftwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    PVT_BREAK_POINT output = (PVT_BREAK_POINT)pIrp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(output, sizeof(VT_BREAK_POINT));

    BYTE* aucPlainText = allocate_pool<BYTE*>(AlignUserDataToBlowfishBlock(user.uSize));
    DecryptData((PVOID)user.pUserData, aucPlainText);

    size_t numElements = user.uSize / sizeof(VT_BREAK_POINT);

    for (size_t i = 0; i < numElements; i++)
    {
        PVT_BREAK_POINT pInfo = reinterpret_cast<PVT_BREAK_POINT>(aucPlainText + i * sizeof(VT_BREAK_POINT));

        if (GetSoftwareBreakpoint(pInfo))
        {
            *output = *pInfo;
        }
        break;
    }

    free_pool(aucPlainText);
}

#include "../Core/DebugBreakInternal.h"

bool AddHardwareBreakpoint(PBREAKPOINT_RECORD Breakpoint)
{
    if (Breakpoint == NULL || Breakpoint->Address == 0)
    {
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

    PBREAKPOINT_RECORD output = (PBREAKPOINT_RECORD)pIrp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(output, sizeof(BREAKPOINT_RECORD));

    BYTE* aucPlainText = allocate_pool<BYTE*>(AlignUserDataToBlowfishBlock(user.uSize));
    DecryptData((PVOID)user.pUserData, aucPlainText);

    size_t numElements = user.uSize / sizeof(BREAKPOINT_RECORD);

    for (size_t i = 0; i < numElements; i++)
    {
        PBREAKPOINT_RECORD pInfo = reinterpret_cast<PBREAKPOINT_RECORD>(aucPlainText + i * sizeof(BREAKPOINT_RECORD));

        if (AddHardwareBreakpoint(pInfo))
        {
            *output = *pInfo;
        }
        break;
    }

    free_pool(aucPlainText);
}

bool DeleteHardwareBreakpoint(PBREAKPOINT_RECORD Breakpoint)
{
    if (Breakpoint == NULL || Breakpoint->watchid < 0)
    {
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

void RemoveHardwareBreakpoint(IN PUSER_DATA userData, IN PIRP pIrp)
{
    USER_DATA user = GetUserData(userData);

    DWORD* output = (DWORD*)pIrp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(output, sizeof(DWORD));

    BYTE* aucPlainText = allocate_pool<BYTE*>(AlignUserDataToBlowfishBlock(user.uSize));
    DecryptData((PVOID)user.pUserData, aucPlainText);

    size_t numElements = user.uSize / sizeof(BREAKPOINT_RECORD);

    for (size_t i = 0; i < numElements; i++)
    {
        PBREAKPOINT_RECORD pInfo = reinterpret_cast<PBREAKPOINT_RECORD>(aucPlainText + i * sizeof(BREAKPOINT_RECORD));

        if (DeleteHardwareBreakpoint(pInfo))
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

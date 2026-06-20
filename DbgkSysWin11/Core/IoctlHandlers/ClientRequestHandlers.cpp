#include "../../Driver.h"
#include "IoctlHandlerCommon.h"
#include "../../Globals.h"
#include "../../../Common/Ring0/Inject/ApcInject/Api/ApcInjectRemoteThreadApi.h"

void VerifyHookClient(IN PUSER_DATA userData, IN PIRP pIrp)
{
    if (pIrp == nullptr || pIrp->AssociatedIrp.SystemBuffer == nullptr)
    {
        return;
    }

    PHOOK_DRIVER_HANDSHAKE_REPLY output = (PHOOK_DRIVER_HANDSHAKE_REPLY)pIrp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(output, sizeof(HOOK_DRIVER_HANDSHAKE_REPLY));

    ULONG major = 0;
    ULONG minor = 0;
    ULONG build = 0;
    PsGetVersion(&major, &minor, &build, nullptr);

    output->magic = HOOK_DRIVER_HANDSHAKE_MAGIC;
    output->version = HOOK_DRIVER_HANDSHAKE_VERSION;
    output->size = sizeof(HOOK_DRIVER_HANDSHAKE_REPLY);
    output->status = HOOK_DRIVER_HANDSHAKE_STATUS_FAIL;
    output->pid = (ULONG)(ULONG_PTR)PsGetCurrentProcessId();
    output->build = build;
    output->flags = 0;

    USER_DATA user = GetUserData(userData);
    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(user, sizeof(RING3_VERIFY), &numElements);
    if (aucPlainText == nullptr)
    {
        return;
    }

    for (size_t i = 0; i < numElements; i++)
    {
        PRING3_VERIFY info = reinterpret_cast<PRING3_VERIFY>(aucPlainText + i * sizeof(RING3_VERIFY));
        if (info->key == HOOK_DRIVER_HANDSHAKE_KEY)
        {
            output->status = HOOK_DRIVER_HANDSHAKE_STATUS_OK;
            output->flags = 1;
            break;
        }
    }

    free_pool(aucPlainText);
}

void GetProcessInfo(IN PUSER_DATA userData, IN PIRP pIrp)
{
    if (pIrp == nullptr || pIrp->AssociatedIrp.SystemBuffer == nullptr)
    {
        return;
    }

    USER_DATA user = GetUserData(userData);
    PRING3_PROCESS_INFO output = (PRING3_PROCESS_INFO)pIrp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(output, sizeof(RING3_PROCESS_INFO));

    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(user, sizeof(RING3_PROCESS_INFO), &numElements);
    if (aucPlainText == nullptr)
    {
        return;
    }

    for (size_t i = 0; i < numElements; i++)
    {
        PRING3_PROCESS_INFO pInfo = reinterpret_cast<PRING3_PROCESS_INFO>(aucPlainText + i * sizeof(RING3_PROCESS_INFO));
        if (pInfo->ProcessHandle == 0)
        {
            continue;
        }

        _EPROCESS* Process = nullptr;
        NTSTATUS Status = ObReferenceObjectByHandle((HANDLE)pInfo->ProcessHandle,
            PROCESS_ALL_ACCESS,
            *PsProcessType,
            KernelMode,
            (PVOID*)&Process,
            NULL);
        if (!NT_SUCCESS(Status) || Process == nullptr)
        {
            continue;
        }

        size_t ptr_DirectoryTableBase = (size_t)Process + kprocess_offset::DirectoryTableBase;
        if (ptr_DirectoryTableBase != 0)
        {
            output->cr3 = *(size_t*)ptr_DirectoryTableBase;
        }
        ObDereferenceObject(Process);
        break;
    }

    free_pool(aucPlainText);
}

void TL_BlockGameResumeThread(IN PUSER_DATA userData, IN PIRP pIrp)
{
    UNREFERENCED_PARAMETER(pIrp);

    USER_DATA user = GetUserData(userData);
    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(user, sizeof(RING3_TL_GAME_TABLE_ENTRY), &numElements);
    if (aucPlainText == nullptr)
    {
        return;
    }

    for (size_t i = 0; i < numElements; i++)
    {
        PRING3_TL_GAME_TABLE_ENTRY pInfo = reinterpret_cast<PRING3_TL_GAME_TABLE_ENTRY>(aucPlainText + i * sizeof(RING3_TL_GAME_TABLE_ENTRY));
        if (pInfo->dwPid != 0)
        {
            InterlockedExchange(&g_TL_Game_pid, pInfo->dwPid);
            break;
        }
    }

    free_pool(aucPlainText);
}

void CreateRemoteThread(IN PUSER_DATA userData)
{
    USER_DATA user = GetUserData(userData);
    size_t numElements = 0;
    UCHAR* aucPlainText = AllocateAndDecryptUserRecords(user, sizeof(RING3_REMOTE_THREAD), &numElements);
    if (aucPlainText == nullptr)
    {
        return;
    }

    for (size_t i = 0; i < numElements; i++)
    {
        PRING3_REMOTE_THREAD pInfo = reinterpret_cast<PRING3_REMOTE_THREAD>(aucPlainText + i * sizeof(RING3_REMOTE_THREAD));
        if (pInfo->hProcess != 0 && pInfo->pUserFunc != 0)
        {
            ApcCreateRemoteThread((HANDLE)pInfo->hProcess, (PVOID)pInfo->pUserFunc);
        }
    }

    free_pool(aucPlainText);
}

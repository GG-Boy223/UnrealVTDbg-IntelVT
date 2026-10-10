#include "../Driver.h"
#include "IoctlDispatchInternal.h"
#include "../Init/Symbolic/InitSymbolic.h"

NTSTATUS DispatchClientIoctl(ULONG Code, PIOCTL_DISPATCH_CONTEXT Context, BOOLEAN* Handled)
{
    *Handled = TRUE;

    switch (Code)
    {
    case IOCTL_VERIFY_HOOK_CLIENT:
    {
        if (!HasInputUserData(*Context) || !HasOutput(*Context, sizeof(HOOK_DRIVER_HANDSHAKE_REPLY)))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        VerifyHookClient(ResolvedUserData(Context), Context->Irp);
        Context->Information = sizeof(HOOK_DRIVER_HANDSHAKE_REPLY);
        return STATUS_SUCCESS;
    }
    case IOCTL_LOAD_SYMBOLS_TABLE:
    {
        if (!HasInputUserData(*Context) || !HasOutput(*Context, sizeof(DWORD)))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        InitSymbolsTable(ResolvedUserData(Context), Context->Irp);
        Context->Information = sizeof(DWORD);
        return STATUS_SUCCESS;
    }
    case IOCTL_LOAD_DEBUGGER_STATE:
    {
        if (!HasInputUserData(*Context))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        Context->Information = 0;
        return STATUS_SUCCESS;
    }
    case IOCTL_LOAD_PROTECT_OBJ_DATA:
    {
        if (!HasInputUserData(*Context))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        //保护对象列表原本在此分支调用 InitProtectList(ResolvedUserData(Context), Context->Irp)。
        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        InitProtectList(ResolvedUserData(Context), Context->Irp);
        Context->Information = 0;
        return STATUS_SUCCESS;
    }
    case IOCTL_LOAD_DEBUGGER_DATA:
    {
        if (!HasInputUserData(*Context))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        InitDebuggerInfo(ResolvedUserData(Context));
        Context->Information = 0;
        return STATUS_SUCCESS;
    }
    case IOCTL_CREATE_REMOTE_THREAD:
    {
        if (!HasInputUserData(*Context))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        CreateRemoteThread(ResolvedUserData(Context));
        Context->Information = 0;
        return STATUS_SUCCESS;
    }
    case IOCTL_GET_PROCESS_INFO:
    {
        if (!HasInputUserData(*Context) || !HasOutput(*Context, sizeof(RING3_PROCESS_INFO)))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        GetProcessInfo(ResolvedUserData(Context), Context->Irp);
        Context->Information = sizeof(RING3_PROCESS_INFO);
        return STATUS_SUCCESS;
    }
    case IOCTL_TL_BLOCK_RESUME_THREAD:
    {
        if (!HasInputUserData(*Context))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        TL_BlockGameResumeThread(ResolvedUserData(Context), Context->Irp);
        Context->Information = 0;
        return STATUS_SUCCESS;
    }
    // /* No special handling for other messages */
    default:
    {
        *Handled = FALSE;
        return STATUS_SUCCESS;
    }
    }
}

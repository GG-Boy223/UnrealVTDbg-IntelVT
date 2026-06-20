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

        // 旧项目保留代码：保护对象列表原本在此分支调用 InitProtectList(ResolvedUserData(Context), Context->Irp)。
        // 注释原因：旧版本此分支后来被改成只返回成功，当前重构保持该行为；直接放开需要先 ResolveInputUserData 并确认输出缓冲大小。
        // 放开好处：ring3 下发的文件/窗口保护名单会进入 g_ProtectFileObjList/g_ProtectWndObjList。
        // 放开坏处：当前保护回调未启用，单独装载名单收益有限；若输入校验不完整，会带来内核解析越界风险。
        // 项目影响：继续注释时 IOCTL 只是兼容占位；放开会恢复保护名单状态变更。
        // 当前建议：继续注释，待 InitProtect/SetProcessCallbacks 一起恢复。
        //NTSTATUS status = ResolveInputUserData(Context);
        //if (!NT_SUCCESS(status))
        //{
        //    return status;
        //}
        //InitProtectList(ResolvedUserData(Context), Context->Irp);
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
    default:
    {
        *Handled = FALSE;
        return STATUS_SUCCESS;
    }
    }
}

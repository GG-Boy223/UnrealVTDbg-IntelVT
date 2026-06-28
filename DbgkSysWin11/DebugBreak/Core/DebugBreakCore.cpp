#include "DebugBreakInternal.h"

bool vmcall_internal(PVOID vmcallinfo)
{
    unsigned long ecode = 0;
    bool boSuccess = false;

    __try
    {
        //若非vmx模式则该指令会触发#UD异常
        boSuccess = __vm_call(((PVMCALLINFO)vmcallinfo)->command, (unsigned __int64)vmcallinfo, 0, 0);
    }
    __except (ecode = GetExceptionCode(), 1)
    {
        outToFile("\xD6\xB4\xD0\xD0vmcall\xCA\xB1\xB7\xA2\xC9\xFA\xC1\xCB\xB4\xED\xCE\xF3 (error: 0x%X)", ecode);
    }

    return boSuccess;
}

//只会向当前逻辑处理器发送请求
bool current_vmcall(PVOID vmcallinfo)
{
    return vmcall_internal(vmcallinfo);
}

// 捕获目标进程 CR3 的公共 helper。
// 调整原因：CE 查找地址访问会频繁设置/删除监视点，旧路径在 attach 状态内探测目标地址并关中断，异常时容易留下 APC/attach 状态不平衡。
static VOID AttachProcessForCr3(PEPROCESS Process, KAPC_STATE* ApcState)
{
    if (Original_KeStackAttachProcess != NULL)
    {
        Original_KeStackAttachProcess((PRKPROCESS)Process, ApcState);
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    KeStackAttachProcess((PRKPROCESS)Process, ApcState);
}

NTSTATUS CaptureProcessCr3(unsigned long pid, unsigned __int64 fallbackCr3, unsigned __int64* capturedCr3)
{
    if (capturedCr3 == NULL)
    {
        return STATUS_INVALID_PARAMETER; // 参数不合法时直接拒绝。
    }

    *capturedCr3 = fallbackCr3;
    // *capturedCr3 = fallbackCr3;
    if (pid == 0)
    {
        return fallbackCr3 != 0 ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
    }

    PEPROCESS Process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)pid, &Process);
    if (!NT_SUCCESS(status))
    {
        return fallbackCr3 != 0 ? STATUS_SUCCESS : status;
    }

    BOOLEAN attached = FALSE; // 记录是否完成 Attach，确保异常路径也能 Detach。
    KAPC_STATE ApcState;
    RtlZeroMemory(&ApcState, sizeof(ApcState));

    __try
    {
        __try
        {
            AttachProcessForCr3(Process, &ApcState);
            attached = TRUE;
            *capturedCr3 = __readcr3();
        }
        __finally
        {
            if (attached)
            {
                KeUnstackDetachProcess(&ApcState);
            }
        }
    }
    __except (status = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        outToFile("\xBB\xF1\xC8\xA1\xC4\xBF\xB1\xEA" "CR3\xCA\xB1\xB7\xA2\xC9\xFA\xD2\xEC\xB3\xA3 (error: 0x%X)", status);
    }

    ObDereferenceObject(Process); //释放进程对象

    if (*capturedCr3 == 0 && fallbackCr3 != 0)
    {
        *capturedCr3 = fallbackCr3;
    }

    return *capturedCr3 != 0 ? STATUS_SUCCESS : status;
}

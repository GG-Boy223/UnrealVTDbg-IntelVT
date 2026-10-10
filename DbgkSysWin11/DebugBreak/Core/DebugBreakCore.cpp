#include "DebugBreakInternal.h"
#include "../../Hvm/hypervisor_gateway.h"
#include "../../../Common/Ring0/Hvm/VmcallDispatchPolicy.h"

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
        outToFile("执行vmcall时发生了错误 (error: 0x%X)", ecode);
    }

    return boSuccess;
}

//只会向当前逻辑处理器发送请求
bool current_vmcall(PVOID vmcallinfo)
{
    if (vmcallinfo == nullptr)
    {
        return false;
    }

    unsigned __int64 command = 0;
    __try
    {
        command = reinterpret_cast<PVMCALLINFO>(vmcallinfo)->command;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return hv::vmcall_dispatch_policy::IsEptMutation(command) ?
        hvgt::vmcall(vmcallinfo) :
        vmcall_internal(vmcallinfo);
}

NTSTATUS CaptureProcessCr3(unsigned long pid, unsigned __int64 fallbackCr3, unsigned __int64* capturedCr3)
{
    if (capturedCr3 == NULL || pid == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *capturedCr3 = 0;
    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)pid, &process);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = Cr3ResolveProcess(process, fallbackCr3, capturedCr3);
    ObDereferenceObject(process);
    return status;
}

#include "DebugEventHandlersInternal.h"

namespace
{
    BOOLEAN SafeReadUserBytes(PVOID Address, PUCHAR Buffer, SIZE_T Size)
    {
        if (Address == NULL || Buffer == NULL || Size == 0)
        {
            return FALSE;
        }

        __try
        {
            ProbeForRead(Address, Size, 1);
            RtlCopyMemory(Buffer, Address, Size);
            return TRUE;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return FALSE;
        }
    }

    BOOLEAN IsInstructionBytesAt(ULONG_PTR Address, UCHAR FirstByte, UCHAR SecondByte)
    {
        UCHAR bytes[2] = { 0 };

        if (Address == 0 || Address >= (ULONG_PTR)MM_USER_PROBE_ADDRESS)
        {
            return FALSE;
        }

        if (!SafeReadUserBytes((PVOID)Address, bytes, sizeof(bytes)))
        {
            return FALSE;
        }

        return bytes[0] == FirstByte && bytes[1] == SecondByte;
    }

    BOOLEAN IsByteAt(ULONG_PTR Address, UCHAR Value)
    {
        UCHAR byte = 0;

        if (Address == 0 || Address >= (ULONG_PTR)MM_USER_PROBE_ADDRESS)
        {
            return FALSE;
        }

        return SafeReadUserBytes((PVOID)Address, &byte, sizeof(byte)) && byte == Value;
    }

    // INT 2C (CD 2C)：异常地址指向 CD 字节本身
    BOOLEAN IsInt2cInstructionAt(ULONG_PTR Address)
    {
        return IsInstructionBytesAt(Address, 0xCD, 0x2C);
    }

    BOOLEAN IsInt2dInstructionAt(ULONG_PTR Address)
    {
        return IsInstructionBytesAt(Address, 0xCD, 0x2D);
    }

    BOOLEAN IsInt2dBreakpointException(PEXCEPTION_RECORD ExceptionRecord)
    {
        ULONG_PTR exceptionAddress;

        if (ExceptionRecord == NULL)
        {
            return FALSE;
        }

        if (ExceptionRecord->ExceptionCode != STATUS_BREAKPOINT)
        {
            return FALSE;
        }

        exceptionAddress = (ULONG_PTR)ExceptionRecord->ExceptionAddress;

        // 真实 INT3 断点的异常地址就是 0xCC 字节本身（本机实测）。
        // 若异常地址处是 0xCC，说明命中的是真实断点：-2/-3 规则不能把它当成 INT 2D 吞掉。
        if (IsByteAt(exceptionAddress, 0xCC))
        {
            return FALSE;
        }

        if (exceptionAddress >= 2 && IsInt2dInstructionAt(exceptionAddress - 2))
        {
            return TRUE;
        }

        if (exceptionAddress >= 3 && IsInt2dInstructionAt(exceptionAddress - 3))
        {
            return TRUE;
        }

        // /* Fail */
        return FALSE;
    }

    // INT 2C 触发的断言异常：异常码 STATUS_ASSERTION_FAILURE，异常地址指向 CD 字节
    // （本机 Win10 22H2 x64 实测：CD 2C 会产生 0xC0000420，而非 STATUS_BREAKPOINT）
    BOOLEAN IsInt2cAssertionException(PEXCEPTION_RECORD ExceptionRecord)
    {
        if (ExceptionRecord == NULL || ExceptionRecord->ExceptionCode != STATUS_ASSERTION_FAILURE)
        {
            return FALSE;
        }

        return IsInt2cInstructionAt((ULONG_PTR)ExceptionRecord->ExceptionAddress);
    }

    // INT 2C/INT 2D 是目标进程自身触发的断言/反调试流程，不能转发给调试器
    BOOLEAN IsInt2cOrInt2dException(PEXCEPTION_RECORD ExceptionRecord)
    {
        return IsInt2cAssertionException(ExceptionRecord) || IsInt2dBreakpointException(ExceptionRecord);
    }
}

//转发异常
BOOLEAN
DbgkForwardException(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOLEAN IsUseDebugPort,
    IN BOOLEAN SecondChance)
{
    DBGKM_APIMSG ApiMessage = { 0 };
    PDBGKM_EXCEPTION DbgKmException = &ApiMessage.u.Exception;
    NTSTATUS Status; // eax
    _EPROCESS* Process = (_EPROCESS*)PsGetCurrentProcess();
    PVOID Port;
    BOOLEAN UseLpc = FALSE;
    PDEBUG_PROCESS DebugProcess;
    __int64 v13 = 1;

    PAGED_CODE();
    DbgkStateOpGuard stateGuard;

    if (IsDbgkUnloadInProgress())
    {
        return FALSE;
    }

    DBGKTRACE(DBGK_EXCEPTION_DEBUG,
        "ExceptionRecord: %p Port: %u\n", ExceptionRecord, IsUseDebugPort);

    ApiMessage.h.u1.Length = 0xD000A8;
    ApiMessage.h.u2.ZeroInit = 8;
    ApiMessage.ApiNumber = DbgKmExceptionApi;

    if (SecondChance)
    {
        PsSetProcessFaultInformation(Process, (int*)&v13);
    }

    // /* Check if this is to be sent on the debug port */
    if (IsUseDebugPort) //是否将异常转发到调试端口
    {
        // /* Use the debug port, unless the thread is being hidden */
        //outLog(("转发异常........\n"));
        if (IsDebugTargetProcess(Process, &DebugProcess))
        {
            Port = DebugProcess->DebugObject;
            outLog("DbgkDebugObject: %llX", DebugProcess->DebugObject);
        }
        else
        {
            size_t ptr_DebugPort = (size_t)Process + eprocess_offset::DebugPort;
            Port = *(PDEBUG_OBJECT*)ptr_DebugPort;
        }
    }
    else
    {
        //否则，使用异常端口
        // /* Otherwise, use the exception port */
        ApiMessage.h.u2.ZeroInit = LPC_EXCEPTION;
        UseLpc = TRUE;
        Port = PsCaptureExceptionPort(Process);
    }

    if (!Port && IsUseDebugPort)
    {
        return FALSE;
    }

    //DbgBreakPoint();
    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        // INT 2C/INT 2D 属于目标进程自身的断言/反调试流程，首轮异常就地丢弃，
        // 交给目标进程自己的 VEH/SEH 处理，避免调试器被同一异常反复打断。
        // 只过滤首轮异常：二次机会异常仍转发，避免掩盖真实未处理崩溃。
        if ((ExceptionRecord->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION) ||
            (ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION) ||
            (!SecondChance && IsInt2cOrInt2dException(ExceptionRecord)))
        {
            // 该提前返回可能发生在 LPC 分支之后：PsCaptureExceptionPort 返回的是
            // 已引用对象，正常路径在 DbgkpSendApiMessageLpc 之后 ObDereferenceObject。
            // 若在此直接返回，异常端口对象引用会永久泄漏（每次异常泄漏一次）。
            if (UseLpc && Port != NULL)
            {
                ObDereferenceObject(Port);
            }
            return FALSE;
        }

    }

    // /* Fill out the exception information */
    //DbgKmException->ExceptionRecord = *ExceptionRecord;
    KeCopyExceptionRecord(&ApiMessage.u.Exception.ExceptionRecord, ExceptionRecord);
    DbgKmException->FirstChance = !SecondChance;

    // /* Check if we should use LPC */
    if (UseLpc)
    {
        // /* Send the message on the LPC Port */
        if (!Port)
        {
            Status = STATUS_SUCCESS;
            ApiMessage.ReturnedStatus = DBG_EXCEPTION_NOT_HANDLED;
        }
        else
        {
            Status = DbgkpSendApiMessageLpc(&ApiMessage, Port, IsUseDebugPort);
            ObDereferenceObject(Port);
        }
    }
    else
    {
        // /* Use native debug object */
        Status = DbgkpSendApiMessage(Process, IsUseDebugPort != 0, &ApiMessage);
    }

    // /* Check if we failed, and for a debug port, also check the return status */
    if (!(NT_SUCCESS(Status)))
    {
        return FALSE;
    }

    if (ApiMessage.ReturnedStatus == DBG_EXCEPTION_NOT_HANDLED)
    {
        if (!IsUseDebugPort)
        {
            Status = DbgkpSendErrorMessage(ExceptionRecord, 2, &ApiMessage);
            // /* Otherwise, we're ok */
            return NT_SUCCESS(Status);
        }

        if (ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
        {
            return TRUE;
        }

        // /* Fail */
        return FALSE;
    }
    Status = ApiMessage.ReturnedStatus;

    // /* Otherwise, we're ok */
    return NT_SUCCESS(Status);
}

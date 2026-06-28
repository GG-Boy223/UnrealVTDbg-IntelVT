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

    BOOLEAN IsInt2dInstructionAt(ULONG_PTR Address)
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

        return bytes[0] == 0xCD && bytes[1] == 0x2D;
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
        if ((ExceptionRecord->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION) ||
            (ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION))
        {
            return FALSE;
        }

        if (ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
        {
            _ETHREAD* Thread = (_ETHREAD*)PsGetCurrentThread();
            //    || Thread->Tcb.ApcStateIndex == 1)
            //if (Thread->Tcb.MiscFlags & PS_MISC_FLAGS_SYSTEM_THREAD
            //{
            //    Teb = NULL;
            //}
            //else
            size_t kthread_base = (size_t)Thread + ethread_offset::Tcb;
            size_t ptr_TrapFrame = kthread_base + kthread_offset::TrapFrame;
            PKTRAP_FRAME TrapFrame = *(PKTRAP_FRAME*)ptr_TrapFrame;

            Dr6 dr6;
            dr6.flags = TrapFrame->Dr6;
            if (dr6.BS == 0) //判断是否是tf单步执行
            {
                //在vt中通过向guest注入#DB异常并不会修改dr6寄存器
                if (dr6.B0 || dr6.B1 || dr6.B2 || dr6.B3)
                {
                    return FALSE;
                }
            }
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

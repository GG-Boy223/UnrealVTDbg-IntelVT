#include "DebugEventHandlersInternal.h"

BOOLEAN DbgkForwardException(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOLEAN IsUseDebugPort,
    IN BOOLEAN SecondChance)
{
    DBGKM_APIMSG ApiMessage = { 0 };
    PDBGKM_EXCEPTION DbgKmException = &ApiMessage.u.Exception;
    NTSTATUS Status;
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

    if (IsUseDebugPort)
    {
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
        ApiMessage.h.u2.ZeroInit = LPC_EXCEPTION;
        UseLpc = TRUE;
        Port = PsCaptureExceptionPort(Process);
    }

    if (!Port && IsUseDebugPort)
    {
        return FALSE;
    }

    if (IsDebugTargetProcess(Process, &DebugProcess))
    {
        if ((ExceptionRecord->ExceptionCode == STATUS_ILLEGAL_INSTRUCTION) ||
            (ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION))
        {
            return FALSE;
        }

        if (ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
        {
            DbgPrint("#DB exception address: %p\n", ExceptionRecord->ExceptionAddress);
            _ETHREAD* Thread = (_ETHREAD*)PsGetCurrentThread();
            size_t kthread_base = (size_t)Thread + ethread_offset::Tcb;
            size_t ptr_TrapFrame = kthread_base + kthread_offset::TrapFrame;
            PKTRAP_FRAME TrapFrame = *(PKTRAP_FRAME*)ptr_TrapFrame;

            Dr6 dr6;
            dr6.flags = TrapFrame->Dr6;
            if (dr6.BS == 0)
            {
                if (dr6.B0 || dr6.B1 || dr6.B2 || dr6.B3)
                {
                    return FALSE;
                }
            }
        }
    }

    KeCopyExceptionRecord(&ApiMessage.u.Exception.ExceptionRecord, ExceptionRecord);
    DbgKmException->FirstChance = !SecondChance;

    if (UseLpc)
    {
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
        Status = DbgkpSendApiMessage(Process, IsUseDebugPort != 0, &ApiMessage);
    }

    if (!(NT_SUCCESS(Status)))
    {
        return FALSE;
    }

    if (ApiMessage.ReturnedStatus == DBG_EXCEPTION_NOT_HANDLED)
    {
        if (!IsUseDebugPort)
        {
            Status = DbgkpSendErrorMessage(ExceptionRecord, 2, &ApiMessage);
            return NT_SUCCESS(Status);
        }

        if (ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
        {
            return TRUE;
        }

        return FALSE;
    }

    Status = ApiMessage.ReturnedStatus;
    return NT_SUCCESS(Status);
}

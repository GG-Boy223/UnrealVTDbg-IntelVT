#include "../Driver.h"
#include "IoctlDispatchInternal.h"
#include "../Globals.h"
#include "../DbgkApi/DbgkApi.h"

NTSTATUS HandlerDispatchRoutin(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    UNREFERENCED_PARAMETER(pDevObj);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN ioctlEntered = FALSE;
    BOOLEAN handled = FALSE;

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
    IOCTL_DISPATCH_CONTEXT context = { 0 };
    context.Irp = pIrp;
    context.SystemBuffer = pIrp->AssociatedIrp.SystemBuffer;
    context.InputLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    context.OutputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;

    InterlockedIncrement(&g_DbgkActiveIoctlCount);
    ioctlEntered = TRUE;

    if (DbgkIsDriverRuntimeUnloading())
    {
        ntStatus = STATUS_DELETE_PENDING;
        goto Complete;
    }

    if (context.SystemBuffer == NULL)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Complete;
    }

    ntStatus = DispatchClientIoctl(stack->Parameters.DeviceIoControl.IoControlCode, &context, &handled);
    if (handled)
    {
        goto Complete;
    }

    ntStatus = DispatchBreakpointIoctl(stack->Parameters.DeviceIoControl.IoControlCode, &context, &handled);
    if (!handled)
    {
        ntStatus = STATUS_INVALID_VARIANT;
        context.Information = 0;
    }

Complete:
    FreeKernelUserDataPayload(&context.SafeUserData);
    if (ioctlEntered)
    {
        InterlockedDecrement(&g_DbgkActiveIoctlCount);
    }

    pIrp->IoStatus.Status = ntStatus;
    pIrp->IoStatus.Information = NT_SUCCESS(ntStatus) ? context.Information : 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return ntStatus;
}

#pragma once

#include "../../Common/Ring0/UserDataPayload.h"

typedef struct _IOCTL_DISPATCH_CONTEXT
{
    PIRP Irp;
    PVOID SystemBuffer;
    ULONG InputLength;
    ULONG OutputLength;
    KERNEL_USER_DATA_PAYLOAD SafeUserData;
    BOOLEAN SafeUserDataReady;
    ULONG Information;
} IOCTL_DISPATCH_CONTEXT, *PIOCTL_DISPATCH_CONTEXT;

inline BOOLEAN HasInputUserData(const IOCTL_DISPATCH_CONTEXT& Context)
{
    return Context.InputLength >= sizeof(USER_DATA);
}

inline BOOLEAN HasOutput(const IOCTL_DISPATCH_CONTEXT& Context, ULONG Expected)
{
    return Context.OutputLength >= Expected;
}

inline NTSTATUS ResolveInputUserData(PIOCTL_DISPATCH_CONTEXT Context)
{
    if (Context->SafeUserDataReady)
    {
        return STATUS_SUCCESS;
    }

    NTSTATUS status = ResolveKernelUserDataPayload(Context->SystemBuffer, Context->InputLength, &Context->SafeUserData);
    if (NT_SUCCESS(status))
    {
        Context->SafeUserDataReady = TRUE;
    }
    //ExReleaseFastMutex(&LongFlagsMutex);
    return status;
}

inline PUSER_DATA ResolvedUserData(PIOCTL_DISPATCH_CONTEXT Context)
{
    return &Context->SafeUserData.User;
}

NTSTATUS DispatchClientIoctl(ULONG Code, PIOCTL_DISPATCH_CONTEXT Context, BOOLEAN* Handled);
NTSTATUS DispatchBreakpointIoctl(ULONG Code, PIOCTL_DISPATCH_CONTEXT Context, BOOLEAN* Handled);

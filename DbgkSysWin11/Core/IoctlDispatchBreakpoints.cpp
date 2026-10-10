#include "IoctlDispatchInternal.h"
#include <Shared/IOCTLs.h>
#include "../DebugBreak/DebugBreak.h"

NTSTATUS DispatchBreakpointIoctl(ULONG Code, PIOCTL_DISPATCH_CONTEXT Context, BOOLEAN* Handled)
{
    *Handled = TRUE;

    switch (Code)
    {
    case IOCTL_SET_HARDWARE_BREAKPOINT:
    {
        if (!HasInputUserData(*Context) || !HasOutput(*Context, sizeof(BREAKPOINT_RECORD)))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        SetHardwareBreakpoint(ResolvedUserData(Context), Context->Irp);
        Context->Information = sizeof(BREAKPOINT_RECORD);
        return STATUS_SUCCESS;
    }
    case IOCTL_DEL_HARDWARE_BREAKPOINT:
    {
        if (!HasInputUserData(*Context) || !HasOutput(*Context, sizeof(HOOK_DEBUG_RESULT)))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        RemoveHardwareBreakpoint(ResolvedUserData(Context), Context->Irp);
        Context->Information = sizeof(HOOK_DEBUG_RESULT);
        return STATUS_SUCCESS;
    }
    case IOCTL_SET_SOFTWARE_BREAKPOINT:
    {
        if (!HasInputUserData(*Context) || !HasOutput(*Context, sizeof(VT_BREAK_POINT)))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        SetSoftwareBreakpoint(ResolvedUserData(Context), Context->Irp);
        Context->Information = sizeof(VT_BREAK_POINT);
        return STATUS_SUCCESS;
    }
    case IOCTL_DEL_SOFTWARE_BREAKPOINT:
    {
        if (!HasInputUserData(*Context) || !HasOutput(*Context, sizeof(HOOK_DEBUG_RESULT)))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        RemoveSoftwareBreakpoint(ResolvedUserData(Context), Context->Irp);
        Context->Information = sizeof(HOOK_DEBUG_RESULT);
        return STATUS_SUCCESS;
    }
    case IOCTL_READ_SOFTWARE_BREAKPOINT:
    {
        if (!HasInputUserData(*Context) || !HasOutput(*Context, sizeof(VT_BREAK_POINT)))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        NTSTATUS status = ResolveInputUserData(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        ReadSoftwareBreakpoint(ResolvedUserData(Context), Context->Irp);
        Context->Information = sizeof(VT_BREAK_POINT);
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

#pragma once

#ifndef _PROCESS_H
#define _PROCESS_H

NTSTATUS PspInsertProcess(IN PEPROCESS TargetProcess,
    IN PEPROCESS ParentProcess,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG Flags,
    IN HANDLE DebugObjectHandle,
    IN ULONG CrossThreadFlags,
    IN int a7,
    IN PPSP_OBJECT_CREATION_STATE ObjectCreationState);

#endif // !_PROCESS_H

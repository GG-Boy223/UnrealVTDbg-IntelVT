#pragma once
#include "../../Driver.h"
#include "../../Globals.h"

// NtDebugActiveProcess - 附加到调试进程
EXTERN_C
NTSTATUS NtDebugActiveProcess(
    IN HANDLE ProcessHandle,
    IN HANDLE DebugHandle
);

// 设置进程调试对象
NTSTATUS DbgkpSetProcessDebugObject(
    IN _EPROCESS* Process,
    IN PDEBUG_OBJECT DebugObject,
    IN NTSTATUS MsgStatus,
    IN _ETHREAD* LastThread
);

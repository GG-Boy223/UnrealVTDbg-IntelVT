#pragma once
#include "../../Driver.h"
#include "../../Globals.h"

// NtWaitForDebugEvent - 等待调试事件
EXTERN_C
NTSTATUS NtWaitForDebugEvent(
    IN HANDLE DebugHandle,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Timeout OPTIONAL,
    OUT PDBGUI_WAIT_STATE_CHANGE StateChange
);

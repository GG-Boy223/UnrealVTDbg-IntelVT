#pragma once

#include "../../Driver.h"
#include "../../ntos/inc/ntdbg.h"

EXTERN_C
NTSTATUS NtWaitForDebugEvent(
    IN HANDLE DebugHandle,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Timeout OPTIONAL,
    OUT PDBGUI_WAIT_STATE_CHANGE StateChange);

#pragma once

#include "../../Driver.h"
#include "../../Globals.h"

EXTERN_C
NTSTATUS NtDebugActiveProcess(
    IN HANDLE ProcessHandle,
    IN HANDLE DebugHandle);

NTSTATUS DbgkpSetProcessDebugObject(
    IN _EPROCESS* Process,
    IN PDEBUG_OBJECT DebugObject,
    IN NTSTATUS MsgStatus,
    IN _ETHREAD* LastThread);

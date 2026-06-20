#pragma once

#include "../../Driver.h"
#include "../../Globals.h"

NTSTATUS DbgkInitialize();
VOID DbgkUnInitialize();

POBJECT_TYPE GetDebugObjectType(UNICODE_STRING Name);

BOOLEAN IsDebugTargetProcess(
    IN _EPROCESS* Process,
    OUT PDEBUG_PROCESS* DebugProcess);

BOOLEAN InsertDebuggerList(PDEBUGGER_TABLE_ENTRY Debugger);

VOID DeleteDebugProcess(PDEBUG_OBJECT DebugObject);

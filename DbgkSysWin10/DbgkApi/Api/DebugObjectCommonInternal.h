#pragma once

#include "DebugObjectCommon.h"

BOOLEAN InsertDebugProcessList(PDEBUG_PROCESS debug_process);
VOID ReleaseDebugProcessList();

BOOLEAN SetDebugTargetProcess(_EPROCESS* Process, PDEBUG_OBJECT DebugObject);
BOOLEAN LookupDebugTargetProcessObject(_EPROCESS* Process, PDEBUG_OBJECT* DebugObject);

VOID WakeDebugObjectEvents(PDEBUG_OBJECT DebugObject);
BOOLEAN IsDbgkUnloadInProgress();

VOID QueueDebugProcessListForUnload();
VOID DetachDebugProcessUnloadList();

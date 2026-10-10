#pragma once

#include "DebugObjectCommon.h"

BOOLEAN InsertDebugProcessList(PDEBUG_PROCESS debug_process);
VOID ReleaseDebugProcessList();

BOOLEAN SetDebugTargetProcess(_EPROCESS* Process, PDEBUG_OBJECT DebugObject);
BOOLEAN LookupDebugTargetProcessObject(_EPROCESS* Process, PDEBUG_OBJECT* DebugObject);
BOOLEAN DetachDebugTargetProcessObject(_EPROCESS* Process, PDEBUG_OBJECT DebugObject, PBOOLEAN DebugObjectReferenced);
BOOLEAN MarkDebugProcessDebugObjectReferenced(_EPROCESS* Process, PDEBUG_OBJECT DebugObject);

VOID WakeDebugObjectEvents(PDEBUG_OBJECT DebugObject);
BOOLEAN IsDbgkUnloadInProgress();

VOID QueueDebugProcessListForUnload();
VOID DetachDebugProcessUnloadList();

// 释放断点表（BreakpointList.cpp）：条目持有目标 EPROCESS 引用，需成对释放。
VOID ReleaseBreakpointList();

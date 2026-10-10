#pragma once

#include "../../Driver.h"
#include "../../ntos/inc/ntosdef.h"
#include "../../ntos/inc/ketypes.h"
#include "../../ntos/inc/amd64.h"
#include "../../ntos/inc/extypes.h"
#include "../../ntos/inc/mi.h"
#include "../../ntos/inc/pstypes.h"
#include "../../ntos/inc/obtypes.h"
#include "../../ntos/inc/peb_teb.h"
#include "../../ntos/inc/mmtypes.h"
#include "../../ntos/inc/ntdbg.h"
#include "../../List/MyList.h"
#include "../../ntos/inc/ntlpcapi.h"
#include "../../ntos/inc/ke.h"
#include "../../ntos/inc/ki.h"
#include "../../ntos/inc/pecoff.h"
#include "../../Log/log.h"
#include "../../ntos/inc/psp.h"
#include "../../Globals.h"
#include "../../Hvm/hypervisor_gateway.h"
#include "../../../Common/Ring0/String/StringHandler.h"
#include "../DbgkApi.h"
#include "DebugObjectCommon.h"
#include "CreateDebugObject.h"

using DbgkStateOpGuard = DbgkRuntimeCallGuard;

BOOLEAN IsDbgkUnloadInProgress();
BOOLEAN InsertDebugProcessList(PDEBUG_PROCESS debug_process);
VOID ReleaseDebugProcessList();

BOOLEAN SetDebugTargetProcess(_EPROCESS* Process, PDEBUG_OBJECT DebugObject);
BOOLEAN LookupDebugTargetProcessObject(_EPROCESS* Process, PDEBUG_OBJECT* DebugObject);
BOOLEAN DetachDebugTargetProcessObject(_EPROCESS* Process, PDEBUG_OBJECT DebugObject, PBOOLEAN DebugObjectReferenced);
BOOLEAN MarkDebugProcessDebugObjectReferenced(_EPROCESS* Process, PDEBUG_OBJECT DebugObject);

VOID WakeDebugObjectEvents(PDEBUG_OBJECT DebugObject);
VOID QueueDebugProcessListForUnload();
VOID DetachDebugProcessUnloadList();

// 释放断点表（BreakpointList.cpp）：条目持有目标 EPROCESS 引用，需成对释放。
VOID ReleaseBreakpointList();

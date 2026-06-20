#include "DebugObjectCommon.h"

// Implementations are split by responsibility:
// - DebugObjectLifecycle.cpp: initialization, close procedure, unload drain.
// - DebugObjectEvents.cpp: debug object event wake-up.
// - DebugProcessTable.cpp: debugger/debug-process table management.

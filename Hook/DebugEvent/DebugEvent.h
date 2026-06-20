#pragma once

#ifndef _DEBUG_EVENT_H
#define _DEBUG_EVENT_H

BOOL
WINAPI
NewWaitForDebugEvent(
    __in LPDEBUG_EVENT lpDebugEvent,
    __in DWORD dwMilliseconds
);

BOOL
WINAPI
NewContinueDebugEvent(
    _In_ DWORD dwProcessId,
    _In_ DWORD dwThreadId,
    _In_ DWORD dwContinueStatus
);

#endif // !_DEBUG_EVENT_H

#pragma once
#include "../../Driver.h"

// NtDebugContinue - 继续被调试线程
EXTERN_C
NTSTATUS NtDebugContinue(
    IN HANDLE DebugObjectHandle,
    IN PCLIENT_ID ClientId,
    IN NTSTATUS ContinueStatus
// __in PCONTEXT ThreadContext  /*此参数是用户层提供的缓冲区*/
);

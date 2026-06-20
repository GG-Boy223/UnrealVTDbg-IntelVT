#pragma once
#include "../../Driver.h"

// NtRemoveProcessDebug - 移除进程调试对象
EXTERN_C
NTSTATUS NtRemoveProcessDebug(
    IN HANDLE ProcessHandle,
    IN HANDLE DebugObjectHandle
);

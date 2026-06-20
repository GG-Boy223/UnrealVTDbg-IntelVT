#pragma once

#include "../../Driver.h"

EXTERN_C
NTSTATUS DbgkOpenProcessDebugPort(
    IN PEPROCESS Process,
    IN KPROCESSOR_MODE PreviousMode,
    OUT HANDLE* DebugHandle);

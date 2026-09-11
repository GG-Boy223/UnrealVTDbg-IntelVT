#pragma once

#include <ntifs.h>

NTSTATUS Cr3ResolveProcess(
    PEPROCESS process,
    unsigned __int64 suppliedCr3,
    unsigned __int64* resolvedCr3);

VOID Cr3ResolverReset();

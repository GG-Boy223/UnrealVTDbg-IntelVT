#pragma once

#include "../../Driver.h"
#include "../../Globals.h"
#include "../../ntos/inc/ntdbg.h"

NTSTATUS
NtSetInformationDebugObject(
    IN HANDLE DebugObjectHandle,
    IN DEBUGOBJECTINFOCLASS DebugObjectInformationClass,
    IN PVOID DebugInformation,
    IN ULONG DebugInformationLength,
    OUT PULONG ReturnLength OPTIONAL);

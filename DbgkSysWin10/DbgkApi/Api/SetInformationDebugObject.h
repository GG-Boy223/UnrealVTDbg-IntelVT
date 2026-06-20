#pragma once
#include "../../Driver.h"
#include "../../Globals.h"

// NtSetInformationDebugObject - 设置调试对象信息
EXTERN_C
NTSTATUS NtSetInformationDebugObject(
    IN HANDLE DebugObjectHandle,
    IN DEBUGOBJECTINFOCLASS DebugObjectInformationClass,
    IN PVOID DebugInformation,
    IN ULONG DebugInformationLength,
    OUT PULONG ReturnLength OPTIONAL
);

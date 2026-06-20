#pragma once
#include "../../Driver.h"

// NtCreateDebugObject - 创建调试对象
EXTERN_C
NTSTATUS NtCreateDebugObject(
    OUT PHANDLE DebugHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN ULONG Flags
);

// 获取调试对象类型
POBJECT_TYPE GetDebugObjectType(UNICODE_STRING Name);

// 调试对象Close回调
EXTERN_C
VOID DbgkpCloseObject(
    IN _EPROCESS* Process,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG_PTR SystemHandleCount
);

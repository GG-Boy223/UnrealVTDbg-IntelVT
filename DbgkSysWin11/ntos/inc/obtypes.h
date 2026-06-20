#pragma once

#ifndef _OBTYPES_H
#define _OBTYPES_H

//
// Dump Control Structure for Object Debugging
//
typedef struct _OB_DUMP_CONTROL
{
    PVOID Stream;
    ULONG Detail;
} OB_DUMP_CONTROL, * POB_DUMP_CONTROL;

//
// Object Type Callbacks
//
typedef VOID
(NTAPI* OB_DUMP_METHOD)(
    _In_ PVOID Object,
    _In_opt_ POB_DUMP_CONTROL Control
    );

//typedef NTSTATUS
//(NTAPI* OB_OPEN_METHOD)(
//    _In_ OB_OPEN_REASON Reason,
//    _In_opt_ PEPROCESS Process,
//    _In_ PVOID ObjectBody,
//    _In_ ACCESS_MASK GrantedAccess,
//    _In_ ULONG HandleCount
//    );

typedef VOID
(NTAPI* OB_CLOSE_METHOD)(
    _In_opt_ PEPROCESS Process,
    _In_ PVOID Object,
    _In_ ACCESS_MASK GrantedAccess,
    _In_ ULONG ProcessHandleCount,
    _In_ ULONG SystemHandleCount
    );

typedef VOID
(NTAPI* OB_DELETE_METHOD)(
    _In_ PVOID Object
    );

typedef NTSTATUS
(NTAPI* OB_PARSE_METHOD)(
    _In_ PVOID ParseObject,
    _In_ PVOID ObjectType,
    _Inout_ PACCESS_STATE AccessState,
    _In_ KPROCESSOR_MODE AccessMode,
    _In_ ULONG Attributes,
    _Inout_ PUNICODE_STRING CompleteName,
    _Inout_ PUNICODE_STRING RemainingName,
    _Inout_opt_ PVOID Context,
    _In_opt_ PSECURITY_QUALITY_OF_SERVICE SecurityQos,
    _Out_ PVOID* Object
    );

typedef NTSTATUS
(NTAPI* OB_SECURITY_METHOD)(
    _In_ PVOID Object,
    _In_ SECURITY_OPERATION_CODE OperationType,
    _In_ PVOID SecurityInformation,
    _In_ PSECURITY_DESCRIPTOR SecurityDescriptor,
    _Inout_ PULONG CapturedLength,
    _Inout_ PSECURITY_DESCRIPTOR* ObjectSecurityDescriptor,
    _In_ POOL_TYPE PoolType,
    _In_ PGENERIC_MAPPING GenericMapping
    );

typedef NTSTATUS
(NTAPI* OB_QUERYNAME_METHOD)(
    _In_ PVOID Object,
    _In_ BOOLEAN HasObjectName,
    _Out_ POBJECT_NAME_INFORMATION ObjectNameInfo,
    _In_ ULONG Length,
    _Out_ PULONG ReturnLength,
    _In_ KPROCESSOR_MODE AccessMode
    );

typedef BOOLEAN
(NTAPI* OB_OKAYTOCLOSE_METHOD)(
    _In_opt_ PEPROCESS Process,
    _In_ PVOID Object,
    _In_ HANDLE Handle,
    _In_ KPROCESSOR_MODE AccessMode
    );

#endif // !_OBTYPES_H

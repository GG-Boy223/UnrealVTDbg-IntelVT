#pragma once

#ifndef _PSP_H
#define _PSP_H

#define PSP_MAX_CREATE_THREAD_NOTIFY 64
#define PSP_MAX_CREATE_PROCESS_NOTIFY 64
#define PS_GET_THREAD_CREATE_TIME(Thread) ((Thread)->CreateTime.QuadPart)


//EXTERN_C
//NTSTATUS PspInsertThread(_ETHREAD* Thread, //rcx
//    _EPROCESS* CurrentProcess, //rdx
//    PINITIAL_TEB InitialTeb, //r8
//    PUCHAR ProcessFlags, //r9
//    ACCESS_MASK DesiredAccess,
//    PUCHAR Flag,
//    PPSP_CREATE_PROCESS_CONTEXT CreateProcessContext,
//    PPSP_OBJECT_CREATION_STATE AccessState,
//    PGROUP_AFFINITY GroupAffinity,
//    HANDLE* ThreadHandle,
//    PCLIENT_ID ClientID);

//EXTERN_C
//NTSTATUS PspCreateThread(_Out_ HANDLE* ThreadHandle,
//    _In_ ACCESS_MASK DesiredAccess,
//    _In_ POBJECT_ATTRIBUTES ObjectAttributes,
//    _In_ HANDLE ProcessHandle,
//    _In_ _EPROCESS* ProcessPointer,
//    _In_ PPSP_CREATE_PROCESS_CONTEXT CreateProcessContext,
//    _In_ PGROUP_AFFINITY GroupAffinity,
//    _Out_ PCLIENT_ID ClientID,
//    _In_ POBJECT_ATTRIBUTES ObjectAttributes2,
//    _In_ PINITIAL_TEB InitialTeb,
//    _In_ ULONG_PTR ProcessFlags,
//    _In_ PKSTART_ROUTINE StartRoutine,
//    _In_ PVOID StartContext,
//    _In_ PUCHAR Flag);

EXTERN_C
NTSTATUS PspCreateThread(HANDLE* ThreadHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ProcessHandle,
    _EPROCESS* arg_Process,
    __int64 CreateProcessContext,
    __int64 a7,
    __int64 a8,
    __int64 a9,
    unsigned int a10,
    PKSTART_ROUTINE StartRoutine,
    __int64 a12,
    __int64 a13);

EXTERN_C
NTSTATUS NtCreateThreadEx(unsigned __int64 a1,
    int a2,
    __int64 a3,
    ULONG_PTR a4,
    __int64 a5,
    __int64 _StartAddress,
    unsigned int a7,
    __int64 a8,
    __int64 a9,
    __int64 a10,
    __int64 a11);

#endif // !_PSP_H

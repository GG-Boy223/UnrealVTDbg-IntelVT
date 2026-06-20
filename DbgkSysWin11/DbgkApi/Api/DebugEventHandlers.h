#pragma once

#include "../../Driver.h"

EXTERN_C VOID DbgkCreateThread(IN PETHREAD Thread);
EXTERN_C VOID DbgkExitThread(NTSTATUS ExitStatus);
EXTERN_C VOID DbgkExitProcess(NTSTATUS ExitStatus);
EXTERN_C VOID PspExitThread(_In_ NTSTATUS ExitStatus);

EXTERN_C
VOID DbgkMapViewOfSection(
    IN _EPROCESS* Process,
    IN PVOID SectionObject,
    IN PVOID BaseAddress,
    IN ULONG SectionOffset,
    IN ULONG_PTR ViewSize);

EXTERN_C
VOID DbgkUnMapViewOfSection(
    IN _EPROCESS* Process,
    IN PVOID BaseAddress);

BOOLEAN DbgkForwardException(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOLEAN IsUseDebugPort,
    IN BOOLEAN SecondChance);

NTSTATUS DbgkpQueueMessage(
    IN _EPROCESS* Process,
    IN _ETHREAD* Thread,
    IN PDBGKM_APIMSG Message,
    IN ULONG Flags,
    IN PDEBUG_OBJECT TargetObject OPTIONAL);

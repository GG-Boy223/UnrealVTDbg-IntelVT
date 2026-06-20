#pragma once
#include <ntifs.h>
#include <stdarg.h>

/// <summary>
/// x86/x64 Shellcode Generation Functions
/// </summary>
ULONG GenPrologue32(IN PUCHAR pBuf);
ULONG GenEpilogue32(IN PUCHAR pBuf, IN INT retSize);
ULONG GenCall32(IN PUCHAR pBuf, IN PVOID pFn, IN INT argc, ...);
ULONG GenCall32V(IN PUCHAR pBuf, IN PVOID pFn, IN INT argc, IN va_list vl);
ULONG GenSync32(IN PUCHAR pBuf, IN PNTSTATUS pStatus, IN PVOID pSetEvent, IN HANDLE hEvent);

ULONG GenPrologue64(IN PUCHAR pBuf);
ULONG GenEpilogue64(IN PUCHAR pBuf, IN INT retSize);
ULONG GenCall64(IN PUCHAR pBuf, IN PVOID pFn, INT argc, ...);
ULONG GenCall64V(IN PUCHAR pBuf, IN PVOID pFn, IN INT argc, IN va_list vl);
ULONG GenSync64(IN PUCHAR pBuf, IN PNTSTATUS pStatus, IN PVOID pSetEvent, IN HANDLE hEvent);

ULONG GenPrologueT(IN BOOLEAN wow64, IN PUCHAR pBuf);
ULONG GenEpilogueT(IN BOOLEAN wow64, IN PUCHAR pBuf, IN INT retSize);
ULONG GenCallT(IN BOOLEAN wow64, IN PUCHAR pBuf, IN PVOID pFn, IN INT argc, ...);
ULONG GenCallTV(IN BOOLEAN wow64, IN PUCHAR pBuf, IN PVOID pFn, IN INT argc, IN va_list vl);
ULONG GenSyncT(IN BOOLEAN wow64, IN PUCHAR pBuf, IN PNTSTATUS pStatus, IN PVOID pSetEvent, IN HANDLE hEvent);
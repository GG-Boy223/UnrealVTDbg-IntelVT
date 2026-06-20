#pragma once

#ifndef _PROTECT_DRX_X64_H
#define _PROTECT_DRX_X64_H

#ifndef _WIN64
#error ProtectDrx is x64-only.
#endif

NTSTATUS NtQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _Inout_ PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength
);

NTSTATUS NtGetContextThread(_In_ HANDLE hThread, _Inout_ PCONTEXT ThreadContext);

NTSTATUS NtSetContextThread(
    __in HANDLE ThreadHandle,
    __in PCONTEXT ThreadContext
);

#endif

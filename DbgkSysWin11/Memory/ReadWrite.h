#pragma once

#ifndef _READ_WRITE_H
#define _READ_WRITE_H

NTSTATUS NtWriteVirtualMemory(_In_ HANDLE ProcessHandle,
	_In_opt_ PVOID BaseAddress,
	_In_ PVOID Buffer,
	_In_ SIZE_T BufferSize,
	_Out_opt_ PSIZE_T NumberOfBytesWritten);

NTSTATUS NtReadVirtualMemory(_In_ HANDLE ProcessHandle,
	_In_opt_ PVOID BaseAddress,
	_Out_opt_ PVOID Buffer,
	_In_ SIZE_T BufferSize,
	_Out_opt_ PSIZE_T NumberOfBytesRead);


NTSTATUS NewMmCopyVirtualMemory(_EPROCESS* FromProcess,
	PVOID FromAddress,
	_EPROCESS* ToProcess,
	PVOID ToAddress,
	size_t BufferSize,
	KPROCESSOR_MODE PreviousMode,
	size_t* NumberOfBytesCopied);

#endif // !_READ_WRITE_H

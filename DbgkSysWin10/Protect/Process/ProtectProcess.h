#pragma once

#ifndef _PROTECT_PROCESS_H
#define _PROTECT_PROCESS_H

//是白名单进程
NTSTATUS IsWhiteListProcess(_In_ HANDLE ProcessHandle,  //要读取的目标进程
    _In_opt_ PVOID BaseAddress,
    _Out_opt_ PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead);

#endif // !_PROTECT_PROCESS_H

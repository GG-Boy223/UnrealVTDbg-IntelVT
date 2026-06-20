#pragma once

#include <ntifs.h>

NTSTATUS ApcCreateRemoteThread(HANDLE pid, PVOID pUserFunc);

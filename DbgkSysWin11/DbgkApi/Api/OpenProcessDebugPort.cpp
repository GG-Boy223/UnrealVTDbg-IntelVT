#include "OpenProcessDebugPort.h"
#include "DebugObjectCommonInternal.h"

EXTERN_C
NTSTATUS DbgkOpenProcessDebugPort(
    IN PEPROCESS Process,
    IN KPROCESSOR_MODE PreviousMode,
    OUT HANDLE* DebugHandle)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(PreviousMode);
    UNREFERENCED_PARAMETER(DebugHandle);

    DbgPrint("\xB5\xF7\xD3\xC3\xC1\xCB\xD5\xE2\xB8\xF6\xBA\xAF\xCA\xFD");

    return STATUS_PORT_NOT_SET;
}

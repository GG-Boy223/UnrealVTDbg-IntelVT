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

    DbgPrint("调用了这个函数");

    return STATUS_PORT_NOT_SET;
}

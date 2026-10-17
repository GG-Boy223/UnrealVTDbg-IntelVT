#include "OpenProcessDebugPort.h"
#include "DebugObjectCommonInternal.h"
#include "../../Hooks/EptHook/EptHook.h"

EXTERN_C
NTSTATUS DbgkOpenProcessDebugPort(
    IN PEPROCESS Process,
    IN KPROCESSOR_MODE PreviousMode,
    OUT HANDLE* DebugHandle)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(DebugHandle);

    // TitanHide 对照：只对虚拟调试目标返回未设置调试端口。
    // 非目标进程必须走原始实现，否则会把系统上其他正常调试会话一起打断。
    if (PreviousMode != KernelMode &&
        Process != NULL &&
        HvmIsVirtualDebugTargetProcess((_EPROCESS*)Process, NULL))
    {
        return STATUS_PORT_NOT_SET;
    }

    if (Original_DbgkOpenProcessDebugPort != NULL)
    {
        return Original_DbgkOpenProcessDebugPort((PMY_EPROCESS)Process, PreviousMode, DebugHandle);
    }
    return STATUS_PORT_NOT_SET;
}

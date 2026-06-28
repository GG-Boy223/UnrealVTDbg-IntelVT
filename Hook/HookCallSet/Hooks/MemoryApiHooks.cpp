#include "HookInternal.h"

BOOL
WINAPI
NewVirtualProtectEx(
    _In_ HANDLE hProcess,
    _In_ LPVOID lpAddress,
    _In_ SIZE_T dwSize,
    _In_ DWORD flNewProtect,
    _Out_ PDWORD lpflOldProtect
)
{
    // 旧项目保留代码：MAP_PROTECT 直接返回 TRUE，模拟自映射保护页的成功改保护。
    // 注释原因：MAP_PROTECT 是旧内存保护绕过分支，当前代码没有在此文件定义该常量，直接放开会造成编译或语义不一致。
    // 放开好处：可兼容旧的自映射/隐藏断点页保护语义，减少 VirtualProtectEx 对假页面的失败暴露。
    // 放开坏处：会让调用方误以为真实页面保护已改变，可能掩盖写入失败或破坏现代 hidden-int3 状态机。
    // 项目影响：继续注释时优先依赖当前 HiddenInt3 读写处理；放开会改变 VirtualProtectEx 的错误语义。
    // 当前建议：继续注释。
    //if (flNewProtect == MAP_PROTECT)
    //{
    //    return TRUE;
    //}

    //if (flNewProtect == MAP_PROTECT)
    //{
    //    return TRUE;
    //}
    BOOL boResult = Sys_VirtualProtectEx(hProcess, lpAddress, dwSize, flNewProtect, lpflOldProtect);
    if (!boResult)
    {
        // 旧项目保留代码：GetLastError()==87 时写回 MAP_PROTECT 并返回 TRUE，判断“可能是由于自映射导致的”。
        // 注释原因：错误 87 只表示参数无效，不足以证明一定是自映射页；当前无条件 TRUE 已经保持旧兼容，但不再写假 old protect。
        // 放开好处：旧 UI/调用方如果依赖 MAP_PROTECT old protect，可恢复兼容。
        // 放开坏处：会把真实参数错误伪装成保护成功，影响内存操作问题定位。
        // 项目影响：继续注释避免传播伪保护值；放开会影响上层对失败原因的判断。
        // 当前建议：继续注释。
        //if (GetLastError() == 87)
        //{
        //    *lpflOldProtect = MAP_PROTECT;
        //    return TRUE;
        //}
        return TRUE;
    }
    return boResult;
}

BOOL
WINAPI
NewWriteProcessMemory(
    _In_ HANDLE hProcess,
    _In_ LPVOID lpBaseAddress,
    _In_reads_bytes_(nSize) LPCVOID lpBuffer,
    _In_ SIZE_T nSize,
    _Out_opt_ SIZE_T* lpNumberOfBytesWritten
)
{
    HookInternal::HiddenInt3WriteResult hiddenResult =
        HookInternal::HandleHiddenInt3Write(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten);
    if (hiddenResult.handled)
    {
        return hiddenResult.success ? TRUE : FALSE;
    }

    return Sys_WriteProcessMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten);
}

BOOL
WINAPI
NewReadProcessMemory(
    _In_ HANDLE hProcess,
    _In_ LPCVOID lpBaseAddress,
    _Out_writes_bytes_to_(nSize, *lpNumberOfBytesRead) LPVOID lpBuffer,
    _In_ SIZE_T nSize,
    _Out_opt_ SIZE_T* lpNumberOfBytesRead
)
{
    if (HookInternal::HandleHiddenInt3Read(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead))
    {
        return TRUE;
    }

    return Sys_ReadProcessMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead);
}

NTSTATUS
NTAPI
NewNtWriteVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _In_ CONST VOID* Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesWritten
)
{
    HookInternal::HiddenInt3WriteResult hiddenResult =
        HookInternal::HandleHiddenInt3Write(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesWritten);
    if (hiddenResult.handled)
    {
        return hiddenResult.success ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
    }

    return Sys_NtWriteVirtualMemory(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesWritten);
}

NTSTATUS
NTAPI
NewNtReadVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _Out_ PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_opt_ PSIZE_T NumberOfBytesRead
)
{
    if (HookInternal::HandleHiddenInt3Read(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead))
    {
        return STATUS_SUCCESS;
    }

    return Sys_NtReadVirtualMemory(ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead);
}

bool TryHook_VirtualProtectEx()
{
    Sys_VirtualProtectEx = (PFN_VIRTUALPROTECTEX)VirtualProtectEx;
    assert(Sys_VirtualProtectEx);
    if (Sys_VirtualProtectEx)
    {
        return HookOn((PVOID*)&Sys_VirtualProtectEx, NewVirtualProtectEx, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("Sys_VirtualProtectEx空指针");
        return false;
    }
}

bool TryUnHook_VirtualProtectEx()
{
    if (Sys_VirtualProtectEx)
    {
        return HookOff((PVOID*)&Sys_VirtualProtectEx, NewVirtualProtectEx, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_WriteProcessMemory()
{
    Sys_WriteProcessMemory = (PFN_WRITEPROCESSMEMORY)WriteProcessMemory;
    assert(Sys_WriteProcessMemory);
    if (Sys_WriteProcessMemory)
    {
        return HookOn((PVOID*)&Sys_WriteProcessMemory, NewWriteProcessMemory, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("Sys_WriteProcessMemory空指针");
        return false;
    }
}

bool TryUnHook_WriteProcessMemory()
{
    if (Sys_WriteProcessMemory)
    {
        return HookOff((PVOID*)&Sys_WriteProcessMemory, NewWriteProcessMemory, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_ReadProcessMemory()
{
    Sys_ReadProcessMemory = (PFN_READPROCESSMEMORY)ReadProcessMemory;
    assert(Sys_ReadProcessMemory);
    if (Sys_ReadProcessMemory)
    {
        return HookOn((PVOID*)&Sys_ReadProcessMemory, NewReadProcessMemory, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("Sys_ReadProcessMemory空指针");
        return false;
    }
}

bool TryUnHook_ReadProcessMemory()
{
    if (Sys_ReadProcessMemory)
    {
        return HookOff((PVOID*)&Sys_ReadProcessMemory, NewReadProcessMemory, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_NtWriteVirtualMemory()
{
    Sys_NtWriteVirtualMemory = ResolveNtWriteVirtualMemory();
    assert(Sys_NtWriteVirtualMemory);
    if (Sys_NtWriteVirtualMemory)
    {
        return HookOn((PVOID*)&Sys_NtWriteVirtualMemory, NewNtWriteVirtualMemory, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("Sys_NtWriteVirtualMemory空指针");
        return false;
    }
}

bool TryUnHook_NtWriteVirtualMemory()
{
    if (Sys_NtWriteVirtualMemory)
    {
        return HookOff((PVOID*)&Sys_NtWriteVirtualMemory, NewNtWriteVirtualMemory, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

bool TryHook_NtReadVirtualMemory()
{
    Sys_NtReadVirtualMemory = ResolveNtReadVirtualMemory();
    assert(Sys_NtReadVirtualMemory);
    if (Sys_NtReadVirtualMemory)
    {
        return HookOn((PVOID*)&Sys_NtReadVirtualMemory, NewNtReadVirtualMemory, GetCurrentThread());
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        logger.Log("Sys_NtReadVirtualMemory空指针");
        return false;
    }
}

bool TryUnHook_NtReadVirtualMemory()
{
    if (Sys_NtReadVirtualMemory)
    {
        return HookOff((PVOID*)&Sys_NtReadVirtualMemory, NewNtReadVirtualMemory, GetCurrentThread());
    }

    // 调试器会高频读取断点附近字节，回填路径保持静默，避免日志拖慢步进和反汇编刷新。
    return true;
}

void Hook_VirtualProtectEx()
{
    (void)TryHook_VirtualProtectEx();
}

void UnHook_VirtualProtectEx()
{
    (void)TryUnHook_VirtualProtectEx();
}

void Hook_WriteProcessMemory()
{
    (void)TryHook_WriteProcessMemory();
}

void UnHook_WriteProcessMemory()
{
    (void)TryUnHook_WriteProcessMemory();
}

void Hook_ReadProcessMemory()
{
    (void)TryHook_ReadProcessMemory();
}

void UnHook_ReadProcessMemory()
{
    (void)TryUnHook_ReadProcessMemory();
}

void Hook_NtWriteVirtualMemory()
{
    (void)TryHook_NtWriteVirtualMemory();
}

void UnHook_NtWriteVirtualMemory()
{
    (void)TryUnHook_NtWriteVirtualMemory();
}

void Hook_NtReadVirtualMemory()
{
    (void)TryHook_NtReadVirtualMemory();
}

void UnHook_NtReadVirtualMemory()
{
    (void)TryUnHook_NtReadVirtualMemory();
}

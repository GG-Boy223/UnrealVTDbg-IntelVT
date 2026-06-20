#include "VmxInternal.h"


//会广播给所有逻辑处理器
bool vmcall(PVOID vmcallinfo)
{
    LONG status = 0;
    DWORD_PTR process_mask = 0;
    DWORD_PTR system_mask = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask))
    {
        logger.Log("GetProcessAffinityMask failed (error: %u)", GetLastError());
        return false;
    }
    (void)process_mask;

    DWORD cpu_count = HookVmxInternal::CountAffinityBits(system_mask);
    DWORD max_affinity_bits = static_cast<DWORD>(sizeof(DWORD_PTR) * 8);
    if (cpu_count == 0)
    {
        return false;
    }

    for (DWORD i = 0; i < max_affinity_bits; i++)
    {
        if ((system_mask & (static_cast<DWORD_PTR>(1) << i)) == 0)
        {
            continue;
        }

        bool call_succeeded = false;
        if (HookVmxInternal::VmcallOnCpu(vmcallinfo, static_cast<int>(i), &call_succeeded) && call_succeeded)
        {
            InterlockedIncrement(&status);
        }
    }
    return status == static_cast<LONG>(cpu_count);
}

//遍历逻辑处理器找到返回事件成功的那个
bool vmcall2(PVOID vmcallinfo)
{
    bool boSuccess = false;
    DWORD_PTR process_mask = 0;
    DWORD_PTR system_mask = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask))
    {
        logger.Log("GetProcessAffinityMask failed (error: %u)", GetLastError());
        return false;
    }
    (void)process_mask;

    DWORD cpu_count = HookVmxInternal::CountAffinityBits(system_mask);
    DWORD max_affinity_bits = static_cast<DWORD>(sizeof(DWORD_PTR) * 8);
    if (cpu_count == 0)
    {
        return false;
    }

    for (DWORD i = 0; i < max_affinity_bits; i++)
    {
        if ((system_mask & (static_cast<DWORD_PTR>(1) << i)) == 0)
        {
            continue;
        }

        bool call_succeeded = false;
        if (HookVmxInternal::VmcallOnCpu(vmcallinfo, static_cast<int>(i), &call_succeeded) && call_succeeded)
        {
            boSuccess = true;
            break;
        }
    }
    return boSuccess;
}

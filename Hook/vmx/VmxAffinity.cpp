#include "VmxInternal.h"

#include <stdexcept>


namespace HookVmxInternal
{
    bool BuildAffinityMask(int CpuNo, DWORD_PTR* Mask)
    {
        if (Mask == nullptr || CpuNo < 0 || CpuNo >= static_cast<int>(sizeof(DWORD_PTR) * 8))
        {
            return false;
        }

        *Mask = static_cast<DWORD_PTR>(1) << CpuNo;
        return true;
    }

    DWORD CountAffinityBits(DWORD_PTR Mask)
    {
        DWORD count = 0;
        while (Mask != 0)
        {
            count += static_cast<DWORD>(Mask & 1);
            Mask >>= 1;
        }
        return count;
    }

    bool VmcallOnCpu(PVOID vmcallinfo, int CpuNo, bool* CallSucceeded)
    {
        if (CallSucceeded == nullptr)
        {
            return false;
        }

        *CallSucceeded = false;

        DWORD_PTR target_mask = 0;
        if (!BuildAffinityMask(CpuNo, &target_mask))
        {
            return false;
        }

        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();
        DWORD_PTR original_process_mask = 0;
        DWORD_PTR system_mask = 0;

        if (!GetProcessAffinityMask(process, &original_process_mask, &system_mask))
        {
            logger.Log("GetProcessAffinityMask failed (error: %u)", GetLastError());
            return false;
        }

        if ((system_mask & target_mask) == 0)
        {
            logger.Log("cpu index is outside current processor group: %d", CpuNo);
            return false;
        }

        bool process_mask_changed = false;
        if ((original_process_mask & target_mask) == 0)
        {
            if (!SetProcessAffinityMask(process, target_mask))
            {
                logger.Log("SetProcessAffinityMask failed (error: %u)", GetLastError());
                return false;
            }
            process_mask_changed = true;
        }

        DWORD_PTR previous_thread_mask = SetThreadAffinityMask(thread, target_mask);
        if (!previous_thread_mask)
        {
            DWORD error = GetLastError();
            if (process_mask_changed)
            {
                SetProcessAffinityMask(process, original_process_mask);
            }
            logger.Log("SetThreadAffinityMask failed (error: %u)", error);
            return false;
        }

        *CallSucceeded = vmcall_internal(vmcallinfo);

        if (!SetThreadAffinityMask(thread, previous_thread_mask))
        {
            logger.Log("restore thread affinity failed (error: %u)", GetLastError());
        }

        if (process_mask_changed && !SetProcessAffinityMask(process, original_process_mask))
        {
            logger.Log("restore process affinity failed (error: %u)", GetLastError());
        }

        return true;
    }
}

//将线程运行在指定cpu核上
//CPU核数从0开始
void RunOnCPU(HANDLE hThread, int CpuNo)
{
    try
    {
        DWORD_PTR target_mask = 0;
        if (!HookVmxInternal::BuildAffinityMask(CpuNo, &target_mask))
        {
            throw std::runtime_error("invalid cpu index");
        }

        if (SetProcessAffinityMask(GetCurrentProcess(), target_mask))
        {
            DWORD_PTR previous_mask = SetThreadAffinityMask(hThread, target_mask);
            if (!previous_mask)
            {
                throw std::runtime_error("设置线程亲和性失败");
            }
        }
        else
        {
            throw std::runtime_error("设置进程亲和性失败");
        }
    }
    catch (const std::exception& e)
    {
        ReportSeriousError(e.what());
    }
}

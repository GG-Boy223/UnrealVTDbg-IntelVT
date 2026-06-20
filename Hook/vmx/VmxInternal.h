#pragma once

#ifndef _HOOK_VMX_INTERNAL_H
#define _HOOK_VMX_INTERNAL_H

#include "../dllmain.h"
#include "../Log/Log.h"
#include "vmx.h"

namespace HookVmxInternal
{
    bool BuildAffinityMask(int CpuNo, DWORD_PTR* Mask);
    DWORD CountAffinityBits(DWORD_PTR Mask);
    bool VmcallOnCpu(PVOID vmcallinfo, int CpuNo, bool* CallSucceeded);
}

bool vmcall_internal(PVOID vmcallinfo);

#endif // !_HOOK_VMX_INTERNAL_H

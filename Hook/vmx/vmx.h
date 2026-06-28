#pragma once

#ifndef _VMX_H
#define _VMX_H

#include "../../Common/Shared/VmcallReason.h"

EXTERN_C
{
//若非vmx模式则该指令会触发#UD异常
#ifdef _WIN64
	bool __vm_call(unsigned __int64 vmcall_reason, unsigned __int64 rdx, unsigned __int64 r8, unsigned __int64 r9);
	bool __vm_call_ex(unsigned __int64 vmcall_reason, unsigned __int64 rdx, unsigned __int64 r8, unsigned __int64 r9, unsigned __int64 r10, unsigned __int64 r11, unsigned __int64 r12, unsigned __int64 r13, unsigned __int64 r14, unsigned __int64 r15);
#else
    bool __stdcall __vm_call(DWORD vmcall_reason, PVOID vmcallinfo);
#endif // _WIN64
}

//将线程运行在指定cpu核上
//CPU核数从0开始
void RunOnCPU(HANDLE hThread, int CpuNo);

//会广播给所有逻辑处理器
bool vmcall(PVOID vmcallinfo);

//遍历逻辑处理器找到返回事件成功的那个
bool vmcall2(PVOID vmcallinfo);

//只会向当前逻辑处理器发送请求
bool current_vmcall(PVOID vmcallinfo);

//清理 VT 侧断点残留状态，不改变 watch 表和现有结构体布局
bool clear_vt_breakpoint_state(
    unsigned __int64 threadId = 0,
    unsigned __int64 virtualAddress = 0,
    unsigned __int64 cr3 = 0,
    int watchId = -1);


#endif // !_VMX_H

#include "VmxInternal.h"


// 旧项目保留的禁用路径：在 C++ 中用 MSVC 内联汇编直接执行 vmcall。
// 为什么注释：x64 MSVC 不支持内联汇编，x86/x64 现在分别由 AsmCallsetX86.asm/AsmCallset.asm 提供 __vm_call，
// 能保持调用约定清晰，也避免 Hook64.dll 编译失败。
// 放开的好处：仅在 32 位编译下可以直接看到最短 vmcall 实现，便于旧代码对照。
// 放开的坏处：x64 会直接不可编译，x86 也缺少参数搬运和返回值约定封装。
// 项目影响：Hook.dll 与 Hook64.dll 的 ABI 会不一致，用户态到 VMM 的通道稳定性下降。
// 结论：继续注释，实际调用保持走汇编文件。
//bool __vm_call(unsigned __int64 vmcall_reason, unsigned __int64 rdx, unsigned __int64 r8, unsigned __int64 r9)
//{
//    __asm
//    {
//        vmcall
//    }
//}

bool vmcall_internal(PVOID vmcallinfo) // 执行用户态到 VMM 的 VMCALL，外层用 SEH 捕获非 VMX 环境下的 #UD。
{
    unsigned long ecode = 0; // 保存 SEH 捕获到的异常码，常见为非 VMX 模式下执行 VMCALL 的非法指令异常。
    bool boSuccess = false; // 保存 __vm_call 返回值，表示驱动/VMM 是否接受并处理该请求。
    __try
    {
        //若非vmx模式则该指令会触发#UD异常
#ifdef _WIN64
        boSuccess = __vm_call(((PVMCALLINFO)vmcallinfo)->command, (unsigned __int64)vmcallinfo, 0, 0); // x64 ABI 使用 RCX/RDX/R8/R9 传参，后两个参数保留为 0。
#else
        boSuccess = __vm_call(static_cast<DWORD>(((PVMCALLINFO)vmcallinfo)->command), vmcallinfo); // x86 路径调用 AsmCallsetX86.asm，ECX/EDX 传命令和请求指针。
#endif
    }
    __except (ecode = GetExceptionCode(), 1)
    {
        logger.Log("执行vmcall时遇到了错误 (error: 0x%x)", ecode); // 未进入 VT 或 VMM 未接管时记录异常，不让 Hook DLL 崩溃宿主进程。
    }
    return boSuccess; // 返回 VMCALL 执行结果，调用方据此判断驱动通信是否成功。
}

//只会向当前逻辑处理器发送请求
bool current_vmcall(PVOID vmcallinfo) // 只向当前逻辑处理器发起 VMCALL，不做跨核广播。
{
    return vmcall_internal(vmcallinfo); // 保持单核请求语义，跨核逻辑由其他 VmxBroadcast 路径负责。
}

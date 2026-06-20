#include "DllMainInternal.h"

void InitFunction()
{
    BaseThreadInitThunk = GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "BaseThreadInitThunk");

    // 旧项目保留的禁用路径：通过目标进程模块枚举和手工导出表解析获取 BaseThreadInitThunk。
    // 为什么注释：当前 Hook DLL 已在目标进程内执行，GetProcAddress(GetModuleHandle("kernel32.dll")) 能直接取到本进程
    // 已加载 kernel32 的导出地址；旧路径需要打开磁盘文件、映射 PE、再按 RVA 计算地址，错误面更大。
    // 放开的好处：可以在需要对照远程模块路径或磁盘 PE 导出表时复用，适合排查模块重定位/导出异常。
    // 放开的坏处：依赖 DbgHelp/ImageRvaToVa 或手工导出解析，文件路径、WOW64 模块枚举、句柄清理都会引入额外失败点。
    // 项目影响：BaseThreadInitThunk 地址用于调试事件里的断点补丁，地址解析错会直接影响进程启动断点与 continue 流程。
    // 结论：继续注释，保持当前本进程导出解析路径；如要恢复旧路径，必须先补齐编码、WOW64 与句柄生命周期测试。
    //BaseThreadInitThunk = GetRoutinePointer((TCHAR*)_T("kernel32.dll"), (LPCSTR)_T("BaseThreadInitThunk"));

    // 旧项目 MyGetProcAddress 内部曾禁用 DbgHelp.dll 的 ImageRvaToVa 动态加载分支。
    // 为什么注释：重构后不再保留 MyGetProcAddress 作为主路径，且工程已有 ntdll/RtlImageRvaToVa 声明可供需要时使用。
    // 放开的好处：能显式从 DbgHelp 取 ImageRvaToVa，便于复用旧版手工导出表解析。
    // 放开的坏处：给 Hook 初始化增加 DbgHelp 依赖，加载器锁和 DLL 依赖顺序下更容易失败。
    // 项目影响：Hook 初始化越早越敏感，额外 LoadLibrary 会影响注入时序和目标进程模块状态。
    // 结论：继续注释，只作为旧版导出解析依赖说明。
    //hModule = LoadLibrary(L"DbgHelp.dll");
    //if (hModule)
    //{
    //    ImageRvaToVa = (PFN_GETPROCADDRESS)GetProcAddress(hModule, "ImageRvaToVa");
    //    if (ImageRvaToVa == NULL)
    //    {
    //        FreeLibrary(hModule);
    //        return NULL;
    //    }
    //}
    //else
    //{
    //    return NULL;
    //}
}

void InitGlobalVariables()
{
    g_process_info = { 0 };
    SYSTEM_INFO SysInfo = { 0 };
    GetSystemInfo(&SysInfo);
    g_dwNumberOfProcessors = SysInfo.dwNumberOfProcessors;
}

#include "DllMainInternal.h"

namespace
{
//若非vmx模式则该指令会触发#UD异常
#ifdef _WIN64
    constexpr wchar_t kHookLogFileName[] = L"Hook64.log"; // x64 Hook DLL 使用独立日志名，避免和 Win32 Hook.dll 日志混写。
#else
    constexpr wchar_t kHookLogFileName[] = L"Hook.log"; // Win32/x86 Hook DLL 保持旧日志名，兼容原部署和排查习惯。
#endif

    std::string WidePathToLocalString(const wchar_t* path) // 将宽字符路径转为本地 ANSI 字符串，匹配 Logger 旧接口。
    {
        if (path == nullptr || path[0] == L'\0')
        {
            return {}; // 空路径没有可写日志目标，调用方会回退默认文件名。
        }

        const int required = WideCharToMultiByte(CP_ACP, 0, path, -1, nullptr, 0, nullptr, nullptr); // 先查询包含 NUL 的目标字节数。
        if (required <= 1)
        {
            return {}; // 转换结果只有结尾 NUL 或失败时视为不可用。
        }

        std::string result(required, '\0'); // 预分配包含 NUL 的缓冲区，避免二次扩容。
        if (WideCharToMultiByte(CP_ACP, 0, path, -1, &result[0], required, nullptr, nullptr) == 0)
        {
            return {}; // 转换失败时不返回半截路径。
        }

        if (!result.empty() && result.back() == '\0')
        {
            result.pop_back(); // Logger 接收普通 std::string，不需要保留结尾 NUL。
        }
        return result; // 返回本地代码页路径。
    }

    bool EnsureWritableLogFile(const wchar_t* logPath) // 探测日志文件是否可创建/追加，避免 Logger 初始化到不可写路径。
    {
        if (logPath == nullptr || logPath[0] == L'\0')
        {
            // 处理打开进程失败的情况
            return false; // 空路径不能作为日志文件。
        }

        HANDLE fileHandle = CreateFileW( // 以追加权限打开，最小化对其他读写日志工具的干扰。
            logPath,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            // 处理打开进程失败的情况
            return false; // 目录无权限或路径非法时通知调用方回退。
        }

        CloseHandle(fileHandle); // 探测句柄立即释放，真实写入由 Logger 管理。
        return true; // 文件可写。
    }

    bool TryBuildModuleLogPath(HINSTANCE moduleHandle, wchar_t* outPath, DWORD outPathCount) // 优先把日志写到 Hook DLL 所在目录。
    {
        if (outPath == nullptr || outPathCount == 0)
        {
            // 处理打开进程失败的情况
            return false; // 输出缓冲无效时不能构造路径。
        }

        const DWORD length = GetModuleFileNameW(moduleHandle, outPath, outPathCount); // 获取当前 Hook DLL 完整路径。
        if (length == 0 || length >= outPathCount)
        {
            outPath[0] = L'\0'; // 清空输出，防止调用方误用截断路径。
            return false; // 获取失败或缓冲不足时回退。
        }

        if (!PathRemoveFileSpecW(outPath))
        {
            outPath[0] = L'\0'; // 目录提取失败时清空输出。
            return false; // 路径结构异常，不能继续追加日志文件名。
        }

        return PathAppendW(outPath, kHookLogFileName) == TRUE; // 在模块目录下追加 Hook.log/Hook64.log。
    }

    bool TryBuildTempLogPath(wchar_t* outPath, DWORD outPathCount) // 模块目录不可写时回退到用户临时目录。
    {
        if (outPath == nullptr || outPathCount == 0)
        {
            // 处理打开进程失败的情况
            return false; // 输出缓冲无效时直接失败。
        }

        const DWORD length = GetTempPathW(outPathCount, outPath); // 获取当前用户可写临时目录。
        if (length == 0 || length >= outPathCount)
        {
            outPath[0] = L'\0'; // 避免保留截断路径。
            return false; // 临时路径不可用时回退默认文件名。
        }

        if (!PathAppendW(outPath, L"UnrealVTDbg"))
        {
            outPath[0] = L'\0'; // 子目录追加失败时清空输出。
            return false; // 无法构造临时日志目录。
        }

        CreateDirectoryW(outPath, nullptr); // 目录已存在也可继续，失败由后续 EnsureWritableLogFile 判断。
        return PathAppendW(outPath, kHookLogFileName) == TRUE; // 临时目录下使用与架构匹配的日志名。
    }

    std::string ResolveHookLogPath(HINSTANCE moduleHandle) // 解析最终日志路径：模块目录优先，临时目录次之，最后当前目录文件名。
    {
        wchar_t logPath[MAX_PATH] = {}; // 固定 MAX_PATH 缓冲，匹配 Win32 路径 API 使用方式。
        if (TryBuildModuleLogPath(moduleHandle, logPath, MAX_PATH) && EnsureWritableLogFile(logPath))
        {
            return WidePathToLocalString(logPath); // 模块目录可写时使用同目录日志，便于随 DLL 一起排查。
        }

        RtlZeroMemory(logPath, sizeof(logPath)); // 清理第一次尝试留下的路径内容。
        if (TryBuildTempLogPath(logPath, MAX_PATH) && EnsureWritableLogFile(logPath))
        {
            return WidePathToLocalString(logPath); // 模块目录不可写时使用临时目录日志。
        }

        return WidePathToLocalString(kHookLogFileName); // 最后回退到当前工作目录下的 Hook.log/Hook64.log。
    }
}

namespace HookDllMainInternal
{
    void EarlyHookLog(const wchar_t* message) // Logger 初始化前的早期调试输出，仅走 OutputDebugString。
    {
        if (message == nullptr)
        {
            return; // 空消息不输出，避免无意义调试噪音。
        }

        OutputDebugStringW(L"[UnrealVTDbg Hook] "); // 统一前缀，便于 DebugView/调试器过滤。
        OutputDebugStringW(message); // 输出调用方消息。
        OutputDebugStringW(L"\n"); // 补换行，保持每条早期日志独立。
    }

    void InitializeHookLogger(HINSTANCE moduleHandle) // 初始化全局 Hook 日志对象，按 DLL 架构选择日志文件名。
    {
        const std::string logPath = ResolveHookLogPath(moduleHandle); // 解析可写日志路径。
        logger.~Logger(); // 显式析构旧 Logger，支持 DLL 重入初始化时更新路径。
        new (&logger) Logger(logPath.empty() ? WidePathToLocalString(kHookLogFileName) : logPath); // 原地重建 Logger，避免全局对象地址变化。
        // 日志路径只用于初始化 Logger，不再写入日志文件。
        // logger.Log("Hook log path: %s", logPath.c_str());
    }
}

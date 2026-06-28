#include "CommonInternal.h"

namespace Common
{
    //查找窗口信息
    BOOL FindWindowInfo(LPCWSTR lpClassName, LPCWSTR titleName)
    {
        if (FindWindow(lpClassName, titleName))
        {
            return TRUE;
        }
        // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
        else
        {
            return FALSE;
        }
    }

    void ReportSeriousError(const char* format, ...)
    {
        // 线程同步：使用互斥锁保护临界区
        std::lock_guard<std::mutex> lock(mutex);

        va_list args;
        va_start(args, format);
        char message[1024] = { 0 };
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);

        std::ostringstream oss;
        oss << message;

        std::string logMessage = oss.str();
        if (!logMessage.empty())
        {
            MessageBoxA(NULL, logMessage.c_str(), "严重错误:", MB_ICONERROR | MB_SYSTEMMODAL);
        }
    }

    bool fileExists(const std::wstring& path)
    {
        HANDLE hFile = CreateFile(
            path.c_str(),
            GENERIC_READ,
            0, // 不共享
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL // 附加数据指针
        );

        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            return true; // 文件存在
        }
        // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
        else
        {
            // 处理打开进程失败的情况
            return false; // 文件不存在
        }
    }

    //获取系统版本信息
    BOOL GetNtVersionNumbers(DWORD& dwMajorVer, DWORD& dwMinorVer, DWORD& dwBuildNumber)
    {
        BOOL bRet = FALSE;
        HMODULE hModNtdll = GetModuleHandle(L"ntdll.dll");
        if (hModNtdll)
        {
            typedef VOID(NTAPI* PFN_RTLGETNTVERSIONNUMBERS)(OUT PULONG pMajorVersion,
                OUT PULONG pMinorVersion,
                OUT PULONG pBuildNumber);
            PFN_RTLGETNTVERSIONNUMBERS pfnRtlGetNtVersionNumbers;
            pfnRtlGetNtVersionNumbers = (PFN_RTLGETNTVERSIONNUMBERS)GetProcAddress(hModNtdll, "RtlGetNtVersionNumbers");
            if (pfnRtlGetNtVersionNumbers)
            {
                pfnRtlGetNtVersionNumbers(&dwMajorVer, &dwMinorVer, &dwBuildNumber);
                dwBuildNumber &= 0x0ffff;
                bRet = TRUE;
            }
        }

        return bRet;
    }
}

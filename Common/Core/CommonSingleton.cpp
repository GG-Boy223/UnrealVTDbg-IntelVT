#include "CommonInternal.h"

namespace Common
{
    //单例模式
    //防止程序多开
    BOOL SingletonPattern(const wchar_t* mutexName)
    {
        BOOL boRet = FALSE;

        // 创建互斥体
        hMutex = CreateMutexW(nullptr, TRUE, mutexName);

        // 检查互斥体是否已存在
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            // 关闭互斥体句柄并退出程序
            CloseHandle(hMutex);
        }
        else
        {
            boRet = TRUE;
        }
        return boRet;
    }

    //退出单例
    void SingletonProgramEnd()
    {
        // 关闭互斥体句柄
        if (hMutex)
        {
            CloseHandle(hMutex);
        }
    }
}

#pragma once

#ifndef _LOGGER_H
#define _LOGGER_H

typedef void(__stdcall* PFN_PRINTLOG)(TCHAR* sText);

extern PFN_PRINTLOG pfnPrintLog;


class Logger {
public:
    Logger(const std::string& filename) : filename(filename)
    {
        logFile.open(filename, std::ios::out | std::ios::app);
        if (!logFile.is_open())
        {
            ::MessageBoxA(NULL, "Error opening log file: ", filename.c_str(), MB_ICONWARNING);
        }
    }

    Logger()
    {

    }

    ~Logger() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    //void _outDebug(TCHAR* sText)
    //{
    //    // 线程同步：使用互斥锁保护临界区
    //    std::lock_guard<std::mutex> lock(mutex);
    //    TCHAR szBuf[1024] = { 0 };

    //    if (m_modName.empty())
    //    {
    //        m_modName = Common::stringToWideString(FileSystem::GetSelfModuleName());
    //    }

    //    wcscat(szBuf, _T("["));
    //    wcscat(szBuf, m_modName.c_str());
    //    wcscat(szBuf, _T("] "));
    //    wcscat(szBuf, sText);
    //    //logger.Log(Common::wideStringToString(sText));
    //    OutputDebugString(szBuf);
    //    OutputDebugString(_T("\n"));
    //}

    void _outDebug(TCHAR* sText)
    {
        // 线程同步：使用互斥锁保护临界区
        std::lock_guard<std::mutex> lock(mutex);
        TCHAR szBuf[1024] = { 0 };

        if (m_modName.empty())
        {
            m_modName = Common::stringToWideString(FileSystem::GetSelfModuleName());
        }

        wcscat(szBuf, _T("["));
        wcscat(szBuf, m_modName.c_str());
        wcscat(szBuf, _T("] "));
        wcscat(szBuf, sText);

        try
        {
            if (!pfnPrintLog)
            {
                pfnPrintLog = (PFN_PRINTLOG)GetProcAddress(GetModuleHandle(NULL), "PrintLog");
            }
            pfnPrintLog(szBuf);
        }
        catch (...)
        {
            ::MessageBox(NULL, _T("定位不到PrintLog入口!"), _T("错误:"), MB_ICONWARNING);
        }
    }

    int outDebug(const TCHAR* _Format, ...)
    {
        __try
        {
            int iRet;
            va_list list;
            TCHAR szBuf[1024] = { 0 };
            va_start(list, _Format);
            iRet = vswprintf(szBuf, sizeof(szBuf), _Format, list);
            _outDebug(szBuf);
            va_end(list);
            return iRet;
        }
        __except (1)
        {
            ::MessageBox(NULL, _T("日志缓冲区溢出!"), _T("栈溢出"), MB_ICONWARNING);
        }
        return 0;
    }

    ////英文日期
    //void Log(const std::string& message) {
    //    if (logFile.is_open()) {
    //        std::time_t now = std::time(nullptr);
    //        std::string timestamp = std::ctime(&now);
    //        timestamp.resize(timestamp.length() - 1);  // Remove trailing newline

    //        logFile << "[" << timestamp << "] " << message << std::endl;
    //        logFile.flush();
    //    }
    //}

    void Log(const char* format, ...) {
        // 线程同步：使用互斥锁保护临界区
        std::lock_guard<std::mutex> lock(mutex);

        if (logFile.is_open()) {
            std::time_t now = std::time(nullptr);
            std::tm* localTime = std::localtime(&now);

            char buffer[100];
            std::strftime(buffer, sizeof(buffer), "%Y年%m月%d日 %H:%M:%S", localTime);

            //std::string logMessage = "[" + std::string(buffer) + "] " + format;

            std::ostringstream oss;
            oss << "[" << buffer << "] ";

            va_list args;
            va_start(args, format);
            char message[1024] = { 0 };
            vsnprintf(message, sizeof(message), format, args);
            va_end(args);

            oss << message;

            std::string logMessage = oss.str();
            logFile << logMessage << std::endl;
            logFile.flush();  //将缓冲区中的数据立即刷新到磁盘，确保数据写入文件。
            //OutputDebugStringA(logMessage.c_str());
        }
    }

private:
    std::ofstream logFile;
    std::string filename;
    std::wstring m_modName;
    std::mutex mutex; // 互斥锁
};

#endif // !_LOGGER_H

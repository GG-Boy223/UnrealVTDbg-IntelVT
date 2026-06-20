#include "../dllmain.h"
#include "DriverService.h"

#include "DriverImagePath.h"

#include <vector>

namespace
{
#ifndef ERROR_INVALID_IMAGE_HASH
    constexpr DWORD ERROR_INVALID_IMAGE_HASH = 577;
#endif
}

namespace DriverService
{
    std::wstring QueryImagePath(const std::wstring& serviceName)
    {
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (scm == nullptr)
        {
            logger.outDebug(L"OpenSCManager failed while querying %s. (error:%lu)", serviceName.c_str(), GetLastError());
            return L"";
        }

        SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_QUERY_CONFIG);
        if (service == nullptr)
        {
            DWORD error = GetLastError();
            if (error != ERROR_SERVICE_DOES_NOT_EXIST)
            {
                logger.outDebug(L"OpenService query failed: %s. (error:%lu)", serviceName.c_str(), error);
            }
            CloseServiceHandle(scm);
            return L"";
        }

        DWORD bytesNeeded = 0;
        QueryServiceConfigW(service, nullptr, 0, &bytesNeeded);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytesNeeded == 0)
        {
            logger.outDebug(L"QueryServiceConfig size failed: %s. (error:%lu)", serviceName.c_str(), GetLastError());
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return L"";
        }

        std::vector<BYTE> buffer(bytesNeeded);
        auto config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());
        std::wstring imagePath;
        if (QueryServiceConfigW(service, config, bytesNeeded, &bytesNeeded))
        {
            imagePath = config->lpBinaryPathName != nullptr ? config->lpBinaryPathName : L"";
        }
        else
        {
            logger.outDebug(L"QueryServiceConfig failed: %s. (error:%lu)", serviceName.c_str(), GetLastError());
        }

        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return imagePath;
    }

    bool Start(const std::wstring& serviceName)
    {
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (scm == nullptr)
        {
            logger.outDebug(L"OpenSCManager failed while starting %s. (error:%lu)", serviceName.c_str(), GetLastError());
            return false;
        }

        SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
        if (service == nullptr)
        {
            DWORD error = GetLastError();
            CloseServiceHandle(scm);
            logger.outDebug(L"OpenService start failed: %s. (error:%lu)", serviceName.c_str(), error);
            return false;
        }

        SERVICE_STATUS_PROCESS status = {};
        DWORD bytesNeeded = 0;
        if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded) &&
            status.dwCurrentState == SERVICE_RUNNING)
        {
            logger.outDebug(L"服务已运行，复用现有服务: %s", serviceName.c_str());
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return true;
        }

        if (!StartServiceW(service, 0, nullptr))
        {
            DWORD error = GetLastError();
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            if (error == ERROR_SERVICE_ALREADY_RUNNING)
            {
                logger.outDebug(L"服务已运行，复用现有服务: %s", serviceName.c_str());
                return true;
            }

            if (error == ERROR_INVALID_IMAGE_HASH)
            {
                logger.outDebug(L"StartService failed: %s. (error:%lu, 驱动未签名或签名不可验证，Windows 已拒绝加载)", serviceName.c_str(), error);
            }
            else
            {
                logger.outDebug(L"StartService failed: %s. (error:%lu)", serviceName.c_str(), error);
            }
            return false;
        }

        bool running = false;
        for (int i = 0; i < 30; ++i)
        {
            Sleep(100);
            if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded))
            {
                break;
            }

            if (status.dwCurrentState == SERVICE_RUNNING)
            {
                running = true;
                break;
            }

            if (status.dwCurrentState == SERVICE_STOPPED)
            {
                logger.outDebug(
                    L"Service stopped after start: %s. win32=%lu service=%lu",
                    serviceName.c_str(),
                    status.dwWin32ExitCode,
                    status.dwServiceSpecificExitCode);
                break;
            }
        }

        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return running;
    }

    bool StopIfRunning(const std::wstring& serviceName)
    {
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (scm == nullptr)
        {
            logger.outDebug(L"OpenSCManager failed while stopping %s. (error:%lu)", serviceName.c_str(), GetLastError());
            return false;
        }

        SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (service == nullptr)
        {
            DWORD error = GetLastError();
            CloseServiceHandle(scm);
            if (error == ERROR_SERVICE_DOES_NOT_EXIST)
            {
                return true;
            }

            logger.outDebug(L"OpenService stop failed: %s. (error:%lu)", serviceName.c_str(), error);
            return false;
        }

        SERVICE_STATUS_PROCESS status = {};
        DWORD bytesNeeded = 0;
        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded))
        {
            logger.outDebug(L"QueryServiceStatusEx stop failed: %s. (error:%lu)", serviceName.c_str(), GetLastError());
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return false;
        }

        if (status.dwCurrentState == SERVICE_STOPPED)
        {
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return true;
        }

        SERVICE_STATUS stopStatus = {};
        if (!ControlService(service, SERVICE_CONTROL_STOP, &stopStatus))
        {
            DWORD error = GetLastError();
            if (error != ERROR_SERVICE_NOT_ACTIVE)
            {
                logger.outDebug(L"Stop service failed: %s. (error:%lu)", serviceName.c_str(), error);
                CloseServiceHandle(service);
                CloseServiceHandle(scm);
                return false;
            }
        }

        bool stopped = false;
        for (int i = 0; i < 30; ++i)
        {
            Sleep(100);
            if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded))
            {
                break;
            }

            if (status.dwCurrentState == SERVICE_STOPPED)
            {
                stopped = true;
                break;
            }
        }

        CloseServiceHandle(service);
        CloseServiceHandle(scm);

        if (!stopped)
        {
            logger.outDebug(L"Stop service timed out: %s", serviceName.c_str());
        }

        return stopped;
    }

    bool EnsureImagePath(const std::wstring& serviceName, const std::wstring& desiredImagePath)
    {
        std::wstring currentImagePath = QueryImagePath(serviceName);
        if (currentImagePath.empty())
        {
            return true;
        }

        if (DriverImagePath::Same(currentImagePath, desiredImagePath))
        {
            return true;
        }

        logger.outDebug(
            L"检测到旧驱动服务路径，准备修正: %s, current=%s, desired=%s",
            serviceName.c_str(),
            currentImagePath.c_str(),
            desiredImagePath.c_str());

        if (!StopIfRunning(serviceName))
        {
            logger.outDebug(L"旧驱动服务仍在运行，无法安全修正路径: %s", serviceName.c_str());
            return false;
        }

        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (scm == nullptr)
        {
            logger.outDebug(L"OpenSCManager failed while reconfiguring %s. (error:%lu)", serviceName.c_str(), GetLastError());
            return false;
        }

        SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG);
        if (service == nullptr)
        {
            DWORD error = GetLastError();
            CloseServiceHandle(scm);
            if (error == ERROR_SERVICE_DOES_NOT_EXIST)
            {
                return true;
            }

            logger.outDebug(L"OpenService reconfigure failed: %s. (error:%lu)", serviceName.c_str(), error);
            return false;
        }

        std::wstring nativeDesiredImagePath = DriverImagePath::ToNativeImagePath(desiredImagePath);
        BOOL changed = ChangeServiceConfigW(
            service,
            SERVICE_NO_CHANGE,
            SERVICE_DEMAND_START,
            SERVICE_NO_CHANGE,
            nativeDesiredImagePath.c_str(),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            serviceName.c_str());
        if (!changed)
        {
            logger.outDebug(L"ChangeServiceConfig failed: %s. (error:%lu)", serviceName.c_str(), GetLastError());
            CloseServiceHandle(service);
            CloseServiceHandle(scm);
            return false;
        }

        CloseServiceHandle(service);
        CloseServiceHandle(scm);

        std::wstring updatedImagePath = QueryImagePath(serviceName);
        if (!DriverImagePath::Same(updatedImagePath, desiredImagePath))
        {
            logger.outDebug(
                L"驱动服务路径修正后仍不匹配: %s, current=%s, desired=%s",
                serviceName.c_str(),
                updatedImagePath.c_str(),
                desiredImagePath.c_str());
            return false;
        }

        logger.outDebug(L"旧驱动服务路径修正完成: %s", serviceName.c_str());
        return true;
    }

    void LogLoadFailure(const wchar_t* source, const std::wstring& serviceName, const std::wstring& imagePath, DWORD error)
    {
        if (error == ERROR_SERVICE_ALREADY_RUNNING)
        {
            logger.outDebug(L"%s: 服务已运行，准备复用: %s", source, serviceName.c_str());
            return;
        }

        if (error == ERROR_INVALID_IMAGE_HASH)
        {
            logger.outDebug(
                L"%s: 驱动未签名或签名不可验证，Windows 拒绝加载: service=%s, path=%s. (error:%lu)",
                source,
                serviceName.c_str(),
                imagePath.c_str(),
                error);
            return;
        }

        logger.outDebug(
            L"%s: 驱动加载失败: service=%s, path=%s. (error:%lu)",
            source,
            serviceName.c_str(),
            imagePath.c_str(),
            error);
    }
}

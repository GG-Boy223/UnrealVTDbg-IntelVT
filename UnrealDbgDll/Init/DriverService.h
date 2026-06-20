#pragma once

#ifndef _DRIVER_SERVICE_H
#define _DRIVER_SERVICE_H

#include <string>

namespace DriverService
{
    std::wstring QueryImagePath(const std::wstring& serviceName);
    bool Start(const std::wstring& serviceName);
    bool StopIfRunning(const std::wstring& serviceName);
    bool EnsureImagePath(const std::wstring& serviceName, const std::wstring& desiredImagePath);
    void LogLoadFailure(const wchar_t* source, const std::wstring& serviceName, const std::wstring& imagePath, DWORD error);
}

#endif // !_DRIVER_SERVICE_H

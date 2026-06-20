#include "../dllmain.h"
#include "InitNTDevice.h"
#include "DriverService.h"
#include "../Crypter/xorstr.hpp"

//加载NT驱动
typedef BOOL(__stdcall* PFN_LOADNT)(const std::wstring DriveImagePath, const std::wstring ServiceName);
//卸载NT驱动
typedef BOOL(__stdcall* PFN_UNLOADNT)(const std::wstring ServiceName);
typedef int(__stdcall* PFN_OUTDEBUG)(const TCHAR* _Format, ...);

namespace
{
    PFN_LOADNT pfnLoadNT;
    PFN_UNLOADNT pfnUnloadNT;
    PFN_OUTDEBUG outDebug;

    HANDLE CreateDeviceHandle(const std::wstring& driveImagePath, const std::wstring& serviceName)
    {
        HANDLE device = CreateFile(
            SYMBOLICLINK,
            GENERIC_READ,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (device != INVALID_HANDLE_VALUE)
        {
            return device;
        }

        if (!DriverService::EnsureImagePath(serviceName, driveImagePath))
        {
            return INVALID_HANDLE_VALUE;
        }

        if (!pfnLoadNT(driveImagePath, serviceName))
        {
            DWORD loadError = GetLastError();
            DriverService::LogLoadFailure(L"AIHelper LoadNT", serviceName, driveImagePath, loadError);
            if (!DriverService::Start(serviceName))
            {
                return INVALID_HANDLE_VALUE;
            }
        }

        logger.outDebug(_xor_(L"正在连接符号..."));
        device = CreateFile(
            SYMBOLICLINK,
            GENERIC_READ,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (device != INVALID_HANDLE_VALUE)
        {
            logger.outDebug(_xor_(L"连接驱动成功！"));
            return device;
        }

        DWORD error = GetLastError();
        logger.outDebug(L"连接驱动失败，设备符号链接不存在或调试驱动未运行! (error:%d)", error);
        return INVALID_HANDLE_VALUE;
    }

    BOOL LoadVT(const std::wstring& driveImagePath, const std::wstring& serviceName)
    {
        if (!DriverService::EnsureImagePath(serviceName, driveImagePath))
        {
            return FALSE;
        }

        if (!pfnLoadNT(driveImagePath, serviceName))
        {
            DWORD loadError = GetLastError();
            DriverService::LogLoadFailure(L"AIHelper LoadNT", serviceName, driveImagePath, loadError);
            if (!DriverService::Start(serviceName))
            {
                return FALSE;
            }
        }

        return TRUE;
    }

    BOOL LoadGeneralDriver(const std::wstring& driveImagePath, const std::wstring& serviceName)
    {
        g_hGeneralDriverDevice = CreateDeviceHandle(driveImagePath, serviceName);
        return g_hGeneralDriverDevice != INVALID_HANDLE_VALUE ? TRUE : FALSE;
    }

    BOOL InitInterface()
    {
        HMODULE helperModule = LoadLibrary(_xor_(L"AIHelper.dll"));
        if (!helperModule)
        {
            ::MessageBox(nullptr, L"没有找到 AIHelper.dll", L"错误信息:", MB_ICONWARNING);
            return FALSE;
        }

        pfnLoadNT = reinterpret_cast<PFN_LOADNT>(GetProcAddress(helperModule, _xor_("LoadNT")));
        pfnUnloadNT = reinterpret_cast<PFN_UNLOADNT>(GetProcAddress(helperModule, _xor_("UnloadNT")));
        outDebug = reinterpret_cast<PFN_OUTDEBUG>(GetProcAddress(helperModule, _xor_("outDebug")));
        if (pfnLoadNT == nullptr || pfnUnloadNT == nullptr || outDebug == nullptr)
        {
            ::MessageBox(nullptr, L"AIHelper.dll 接口不完整", _xor_(L"错误信息:"), MB_ICONWARNING);
            return FALSE;
        }

        return TRUE;
    }
}

HANDLE g_hGeneralDriverDevice = INVALID_HANDLE_VALUE;

//初始化驱动设备
BOOL InitializeDevice(const std::wstring DriveImagePath)
{
    if (!LoadVT(DriveImagePath + _xor_(L"VT_Driver.sys"), _xor_(L"VT_Driver")))
    {
        return FALSE;
    }

    DWORD majorVersion = 0;
    DWORD minorVersion = 0;
    DWORD buildNumber = 0;
    if (!Common::GetNtVersionNumbers(majorVersion, minorVersion, buildNumber))
    {
        DWORD error = GetLastError();
        logger.outDebug(_xor_(L"无法获取系统版本信息! (error:%d)"), error);
        return FALSE;
    }

    if (buildNumber >= 22000)
    {
        return LoadGeneralDriver(DriveImagePath + _xor_(L"DbgkSysWin11.sys"), _xor_(L"UnrealDevice"));
    }

    return LoadGeneralDriver(DriveImagePath + _xor_(L"DbgkSysWin10.sys"), _xor_(L"UnrealDevice"));
}

BOOL _Initialize(const TCHAR* sPath)
{
    if (sPath == nullptr)
    {
        logger.outDebug(L"_Initialize path is null");
        return FALSE;
    }

    std::wstring drivePath(sPath);
    logger.outDebug(L"_Initialize path: %s", drivePath.c_str());
    if (drivePath.empty())
    {
        return FALSE;
    }

    if (!InitInterface())
    {
        return FALSE;
    }

    return InitializeDevice(drivePath);
}

BOOL UnInitialize()
{
    BOOL uninitialized = TRUE;

    if (g_hGeneralDriverDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hGeneralDriverDevice);
        g_hGeneralDriverDevice = INVALID_HANDLE_VALUE;
    }

    if (pfnUnloadNT == nullptr)
    {
        logger.outDebug(_xor_(L"卸载接口未初始化，跳过驱动停止。"));
        return FALSE;
    }

    if (pfnUnloadNT(L"UnrealDevice"))
    {
        outDebug(_xor_(L"Win10/Win11 调试服务停止成功。"));
    }
    else
    {
        outDebug(_xor_(L"Win10/Win11 调试服务停止失败或未运行。"));
        uninitialized = FALSE;
    }

    if (pfnUnloadNT(_xor_(L"VT_Driver")))
    {
        outDebug(_xor_(L"VT 服务停止成功。"));
    }
    else
    {
        outDebug(L"VT 服务停止失败或未运行。");
        uninitialized = FALSE;
    }

    if (pfnUnloadNT(L"BACDevice"))
    {
        outDebug(_xor_(L"旧版 BACDevice 服务停止成功。"));
    }

    return uninitialized;
}

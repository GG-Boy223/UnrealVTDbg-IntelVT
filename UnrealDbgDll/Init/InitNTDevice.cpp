#include "../dllmain.h"
#include "InitNTDevice.h"
#include "../Crypter/xorstr.hpp"


//加载NT驱动
typedef BOOL(__stdcall* PFN_LOADNT)(const std::wstring DriveImagePath, const std::wstring ServiceName);
//卸载NT驱动
typedef BOOL(__stdcall* PFN_UNLOADNT)(const std::wstring ServiceName);

typedef int(__stdcall* PFN_OUTDEBUG)(const TCHAR* _Format, ...);

PFN_LOADNT pfnLoadNT;
PFN_UNLOADNT pfnUnloadNT;
PFN_OUTDEBUG outDebug;
HMODULE g_aiHelperModule;

HANDLE g_hGeneralDriverDevice = INVALID_HANDLE_VALUE;


namespace
{
    constexpr DWORD kDriverServiceStateWaitTimeoutMs = 10000;
    constexpr DWORD kDriverServiceStatePollIntervalMs = 100;

    enum class DriverServiceState
    {
        QueryFailed,
        NotFound,
        Stopped,
        Running
    };

    //查询驱动服务状态
    DriverServiceState QueryDriverServiceState(const std::wstring& ServiceName)
    {
        if (ServiceName.empty())
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return DriverServiceState::QueryFailed;
        }

        SC_HANDLE hServiceManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (hServiceManager == nullptr)
        {
            logger.outDebug(_xor_(L"打开服务控制管理器失败！(service:%s error:%lu)"), ServiceName.c_str(), GetLastError());
            return DriverServiceState::QueryFailed;
        }

        SC_HANDLE hService = OpenServiceW(hServiceManager, ServiceName.c_str(), SERVICE_QUERY_STATUS);
        DWORD openError = GetLastError();
        if (hService == nullptr)
        {
            CloseServiceHandle(hServiceManager);
            if (openError == ERROR_SERVICE_DOES_NOT_EXIST)
            {
                return DriverServiceState::NotFound;
            }

            logger.outDebug(_xor_(L"查询驱动服务失败！(service:%s error:%lu)"), ServiceName.c_str(), openError);
            SetLastError(openError);
            return DriverServiceState::QueryFailed;
        }

        SERVICE_STATUS serviceStatus = { 0 };
        BOOL queryResult = QueryServiceStatus(hService, &serviceStatus);
        DWORD queryError = GetLastError();
        CloseServiceHandle(hService);
        CloseServiceHandle(hServiceManager);

        if (!queryResult)
        {
            logger.outDebug(_xor_(L"查询驱动服务状态失败！(service:%s error:%lu)"), ServiceName.c_str(), queryError);
            SetLastError(queryError);
            return DriverServiceState::QueryFailed;
        }

        return serviceStatus.dwCurrentState == SERVICE_RUNNING
            ? DriverServiceState::Running
            : DriverServiceState::Stopped;
    }

    //等待驱动服务停止
    BOOL WaitDriverServiceStopped(SC_HANDLE hService, const std::wstring& ServiceName)
    {
        const ULONGLONG deadline = GetTickCount64() + kDriverServiceStateWaitTimeoutMs;
        for (;;)
        {
            SERVICE_STATUS serviceStatus = { 0 };
            if (!QueryServiceStatus(hService, &serviceStatus))
            {
                logger.outDebug(_xor_(L"等待驱动服务停止时查询状态失败！(service:%s error:%lu)"), ServiceName.c_str(), GetLastError());
                return FALSE;
            }

            if (serviceStatus.dwCurrentState == SERVICE_STOPPED)
            {
                return TRUE;
            }

            if (GetTickCount64() >= deadline)
            {
                SetLastError(ERROR_TIMEOUT);
                return FALSE;
            }

            Sleep(kDriverServiceStatePollIntervalMs);
        }
    }

    //等待驱动服务删除完成
    BOOL WaitDriverServiceRemoved(SC_HANDLE hServiceManager, const std::wstring& ServiceName)
    {
        const ULONGLONG deadline = GetTickCount64() + kDriverServiceStateWaitTimeoutMs;
        for (;;)
        {
            SC_HANDLE hService = OpenServiceW(hServiceManager, ServiceName.c_str(), SERVICE_QUERY_STATUS);
            if (hService == nullptr)
            {
                const DWORD openError = GetLastError();
                if (openError == ERROR_SERVICE_DOES_NOT_EXIST)
                {
                    return TRUE;
                }

                if (openError != ERROR_SERVICE_MARKED_FOR_DELETE)
                {
                    logger.outDebug(_xor_(L"等待驱动服务删除时查询失败！(service:%s error:%lu)"), ServiceName.c_str(), openError);
                    SetLastError(openError);
                    return FALSE;
                }
            }
            else
            {
                CloseServiceHandle(hService);
            }

            if (GetTickCount64() >= deadline)
            {
                SetLastError(ERROR_TIMEOUT);
                return FALSE;
            }

            Sleep(kDriverServiceStatePollIntervalMs);
        }
    }

    //删除驱动服务：运行中先停止，停止后再删除
    BOOL DeleteDriverService(const std::wstring& ServiceName)
    {
        if (ServiceName.empty())
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        SC_HANDLE hServiceManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (hServiceManager == nullptr)
        {
            logger.outDebug(_xor_(L"打开服务控制管理器失败！(service:%s error:%lu)"), ServiceName.c_str(), GetLastError());
            return FALSE;
        }

        SC_HANDLE hService = OpenServiceW(hServiceManager, ServiceName.c_str(), DELETE | SERVICE_QUERY_STATUS | SERVICE_STOP);
        DWORD openError = GetLastError();
        if (hService == nullptr)
        {
            CloseServiceHandle(hServiceManager);
            if (openError == ERROR_SERVICE_DOES_NOT_EXIST)
            {
                logger.outDebug(_xor_(L"驱动服务不存在，无需删除。(service:%s)"), ServiceName.c_str());
                return TRUE;
            }

            logger.outDebug(_xor_(L"打开驱动服务失败！(service:%s error:%lu)"), ServiceName.c_str(), openError);
            SetLastError(openError);
            return FALSE;
        }

        SERVICE_STATUS serviceStatus = { 0 };
        if (!QueryServiceStatus(hService, &serviceStatus))
        {
            const DWORD queryError = GetLastError();
            logger.outDebug(_xor_(L"删除驱动服务前查询状态失败！(service:%s error:%lu)"), ServiceName.c_str(), queryError);
            CloseServiceHandle(hService);
            CloseServiceHandle(hServiceManager);
            SetLastError(queryError);
            return FALSE;
        }

        if (serviceStatus.dwCurrentState != SERVICE_STOPPED)
        {
            if (serviceStatus.dwCurrentState != SERVICE_STOP_PENDING)
            {
                if (!ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus))
                {
                    const DWORD stopError = GetLastError();
                    if (stopError != ERROR_SERVICE_NOT_ACTIVE)
                    {
                        logger.outDebug(_xor_(L"停止驱动服务失败！(service:%s error:%lu)"), ServiceName.c_str(), stopError);
                        CloseServiceHandle(hService);
                        CloseServiceHandle(hServiceManager);
                        SetLastError(stopError);
                        return FALSE;
                    }
                }
            }

            if (!WaitDriverServiceStopped(hService, ServiceName))
            {
                logger.outDebug(_xor_(L"等待驱动服务停止超时！(service:%s)"), ServiceName.c_str());
                CloseServiceHandle(hService);
                CloseServiceHandle(hServiceManager);
                return FALSE;
            }
        }

        if (!DeleteService(hService))
        {
            const DWORD deleteError = GetLastError();
            if (deleteError != ERROR_SERVICE_MARKED_FOR_DELETE)
            {
                logger.outDebug(_xor_(L"删除驱动服务失败！(service:%s error:%lu)"), ServiceName.c_str(), deleteError);
                CloseServiceHandle(hService);
                CloseServiceHandle(hServiceManager);
                SetLastError(deleteError);
                return FALSE;
            }
        }

        CloseServiceHandle(hService);
        const BOOL removed = WaitDriverServiceRemoved(hServiceManager, ServiceName);
        CloseServiceHandle(hServiceManager);
        if (removed)
        {
            logger.outDebug(_xor_(L"驱动服务删除完成。(service:%s)"), ServiceName.c_str());
        }
        else
        {
            logger.outDebug(_xor_(L"驱动服务删除等待超时。(service:%s)"), ServiceName.c_str());
        }
        return removed;
    }
}

//连接驱动
HANDLE CreateDeviceHandle(const std::wstring DriveImagePath, const std::wstring ServiceName)
{
    DWORD error = 0;
    //设备句柄正常时判定驱动已运行，无需重复安装
    HANDLE hDevice = CreateFile(SYMBOLICLINK, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDevice != INVALID_HANDLE_VALUE)
    {
        logger.outDebug(_xor_(L"设备句柄正常，判定驱动已运行，跳过重复安装。(service:%s)"), ServiceName.c_str());
        return hDevice;
    }

    error = GetLastError();
    logger.outDebug(_xor_(L"设备句柄打开失败！(service:%s error:%d)，准备检查驱动服务状态。"), ServiceName.c_str(), error);

    const DriverServiceState serviceState = QueryDriverServiceState(ServiceName);
    if (serviceState == DriverServiceState::QueryFailed)
    {
        return INVALID_HANDLE_VALUE;
    }

    //服务存在但句柄异常，先删除旧服务再执行原始安装流程
    if (serviceState != DriverServiceState::NotFound)
    {
        logger.outDebug(_xor_(L"驱动服务存在但设备句柄异常，准备删除旧服务。(service:%s)"), ServiceName.c_str());
        if (!DeleteDriverService(ServiceName))
        {
            return INVALID_HANDLE_VALUE;
        }
        logger.outDebug(_xor_(L"旧驱动服务已删除，开始重新安装。(service:%s)"), ServiceName.c_str());
    }
    else
    {
        logger.outDebug(_xor_(L"驱动服务不存在，执行原始安装流程。(service:%s)"), ServiceName.c_str());
    }

    const BOOL loadResult = pfnLoadNT(DriveImagePath, ServiceName);
    if (!loadResult)
    {
        logger.outDebug(_xor_(L"驱动服务安装失败。(service:%s error:%lu)"), ServiceName.c_str(), GetLastError());
    }

    logger.outDebug(_xor_(L"正在连接符号...(service:%s)"), ServiceName.c_str());
    hDevice = CreateFile(SYMBOLICLINK, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDevice != INVALID_HANDLE_VALUE)
    {
        logger.outDebug(_xor_(L"连接驱动成功！(service:%s)"), ServiceName.c_str());
        return hDevice;
    }

    error = GetLastError();
    logger.outDebug(_xor_(L"连接驱动失败！(service:%s error:%d)"), ServiceName.c_str(), error);
    return INVALID_HANDLE_VALUE;
}

//加载VT驱动
BOOL LoadVT(const std::wstring DriveImagePath, const std::wstring ServiceName)
{
    const DriverServiceState serviceState = QueryDriverServiceState(ServiceName);
    if (serviceState == DriverServiceState::QueryFailed)
    {
        return FALSE;
    }

    if (serviceState == DriverServiceState::Running)
    {
        logger.outDebug(_xor_(L"VT 驱动服务正常运行，判定驱动已加载，跳过重复安装。(service:%s)"), ServiceName.c_str());
        return TRUE;
    }

    //服务存在但不是运行状态时，先删除旧服务再执行原始安装流程
    if (serviceState == DriverServiceState::Stopped)
    {
        logger.outDebug(_xor_(L"VT 驱动服务不是运行状态，准备删除旧服务。(service:%s)"), ServiceName.c_str());
        if (!DeleteDriverService(ServiceName))
        {
            return FALSE;
        }
        logger.outDebug(_xor_(L"VT 旧驱动服务已删除，开始重新安装。(service:%s)"), ServiceName.c_str());
    }
    else if (serviceState == DriverServiceState::NotFound)
    {
        logger.outDebug(_xor_(L"VT 驱动服务不存在，执行原始安装流程。(service:%s)"), ServiceName.c_str());
    }

    const BOOL loadResult = pfnLoadNT(DriveImagePath, ServiceName);
    if (!loadResult)
    {
        logger.outDebug(_xor_(L"VT 驱动服务安装失败。(service:%s error:%lu)"), ServiceName.c_str(), GetLastError());
    }
    return loadResult;
}

//常规驱动加载
BOOL LoadGeneralDriver(const std::wstring DriveImagePath, const std::wstring ServiceName)
{
    g_hGeneralDriverDevice = CreateDeviceHandle(DriveImagePath, ServiceName);
    if (g_hGeneralDriverDevice == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

//初始化驱动设备
BOOL InitializeDevice(const std::wstring DriveImagePath)
{
    BOOL bRet = TRUE;

    //if (!UnInitialize_END) {
    //    return FALSE;
    //}

    if (!LoadVT(DriveImagePath + _xor_(L"VT_Driver.sys"), _xor_(L"VT_Driver")))
    {
        return FALSE;
    }

    DWORD dwMajorVer, dwMinorVer, dwBuildNumber, error;
    if (Common::GetNtVersionNumbers(dwMajorVer, dwMinorVer, dwBuildNumber))
    {
        if (dwBuildNumber >= 22000)
        {
            //Win11
            if (!LoadGeneralDriver(DriveImagePath + _xor_(L"DbgkSysWin11.sys"), _xor_(L"UnrealDevice")))
            {
                bRet = FALSE;
            }
        }
        else
        {
            //Win10
            if (!LoadGeneralDriver(DriveImagePath + _xor_(L"DbgkSysWin10.sys"), _xor_(L"UnrealDevice")))
            {
                bRet = FALSE;
            }
        }
    }
    else
    {
        error = GetLastError();
        logger.outDebug(_xor_(L"无法获取系统版本信息! (error:%d)"), error);
        bRet = FALSE;
    }
    return bRet;
}

//初始化接口
BOOL InitInterface(const std::wstring& runtimePath)
{
    if (g_aiHelperModule != nullptr)
    {
        return pfnLoadNT != nullptr && pfnUnloadNT != nullptr && outDebug != nullptr;
    }

    const std::wstring aiHelperPath = runtimePath + _xor_(L"AIHelper.dll");
    g_aiHelperModule = LoadLibraryExW(
        aiHelperPath.c_str(),
        nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!g_aiHelperModule)
    {
        ::MessageBox(NULL, (L"没有找到 AIHelper.dll"), (L"错误信息:"), MB_ICONWARNING);
        return FALSE;
    }
    pfnLoadNT = (PFN_LOADNT)GetProcAddress(g_aiHelperModule,     _xor_("LoadNT"));
    pfnUnloadNT = (PFN_UNLOADNT)GetProcAddress(g_aiHelperModule, _xor_("UnloadNT"));
    outDebug = (PFN_OUTDEBUG)GetProcAddress(g_aiHelperModule,    _xor_("outDebug"));

    if (pfnLoadNT == nullptr || pfnUnloadNT == nullptr || outDebug == nullptr)
    {
        FreeLibrary(g_aiHelperModule);
        g_aiHelperModule = nullptr;
        pfnLoadNT = nullptr;
        pfnUnloadNT = nullptr;
        outDebug = nullptr;
        ::MessageBox(NULL, (L"AIHelper.dll 接口不完整"), _xor_(L"错误信息:"), MB_ICONWARNING);
        return FALSE;
    }

    return TRUE;
}

BOOL _Initialize(const TCHAR* sPath)
{
    BOOL boInit = FALSE;
    std::wstring sDrivePath(sPath);
    if (!sDrivePath.empty())
    {
        if (InitInterface(sDrivePath))
        {
            boInit = InitializeDevice(sDrivePath);
        }
    }
    return boInit;
}

//结束时的扫尾工作
//BOOL UnInitialize()
//{
//    BOOL boUnInit = FALSE;
//    if (g_hGeneralDriverDevice != INVALID_HANDLE_VALUE)
//    {
//        CloseHandle(g_hGeneralDriverDevice);
//        g_hGeneralDriverDevice = INVALID_HANDLE_VALUE;
//    }
//
//    if (pfnUnloadNT(L"BACDevice"))
//    {
//        outDebug(L"停止虚幻引擎服务成功..");
//        boUnInit = TRUE;
//    }
//    else
//    {
//        outDebug(L"停止虚幻引擎服务失败!");
//        boUnInit = FALSE;
//    }
//
//    if (pfnUnloadNT(L"VT_Driver"))
//    {
//        outDebug(L"停止VT服务成功..");
//        boUnInit = TRUE;
//    }
//    else
//    {
//        outDebug(L"停止VT服务失败!");
//        boUnInit = FALSE;
//    }
//    return boUnInit;
//}

BOOL UnInitialize()
{
    BOOL boUnInit = TRUE;

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

    if (pfnUnloadNT((L"UnrealDevice")))
    {
        outDebug(_xor_(L"Win10/Win11 调试服务停止成功。"));
    }
    else
    {
        outDebug(_xor_(L"Win10/Win11 调试服务停止失败或未运行。"));
        boUnInit = FALSE;
    }

    if (pfnUnloadNT(_xor_(L"VT_Driver")))
    {
        outDebug(_xor_(L"VT 服务停止成功。"));
    }
    else
    {
        outDebug((L"VT 服务停止失败或未运行。"));
        boUnInit = FALSE;
    }

    // 兼容旧版本残留服务名。此服务不是当前加载链路的必需依赖，放到最后清理。
    if (pfnUnloadNT((L"BACDevice")))
    {
        outDebug(_xor_(L"旧版 BACDevice 服务停止成功。"));
    }

    return boUnInit;
}

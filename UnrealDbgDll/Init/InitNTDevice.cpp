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


//检测驱动服务是否已经存在
bool IsDriverServicePresent(const std::wstring& ServiceName)
{
    if (ServiceName.empty())
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    SC_HANDLE hServiceManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hServiceManager == nullptr)
    {
        logger.outDebug(_xor_(L"打开服务控制管理器失败！(error:%lu)"), GetLastError());
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hServiceManager, ServiceName.c_str(), SERVICE_QUERY_STATUS);
    DWORD openError = GetLastError();
    if (hService != nullptr)
    {
        CloseServiceHandle(hService);
    }
    CloseServiceHandle(hServiceManager);

    if (hService != nullptr)
    {
        return true;
    }

    if (openError != ERROR_SERVICE_DOES_NOT_EXIST)
    {
        logger.outDebug(_xor_(L"查询驱动服务失败！(service:%s error:%lu)"), ServiceName.c_str(), openError);
    }
    SetLastError(openError);
    return false;
}

//连接驱动
HANDLE CreateDeviceHandle(const std::wstring DriveImagePath, const std::wstring ServiceName)
{
    DWORD error = 0;
    //创建符号连接
    HANDLE hDevice = CreateFile(SYMBOLICLINK, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDevice != INVALID_HANDLE_VALUE)
    {
        return hDevice;
    }
    else
    {
        //驱动服务已存在时判定驱动已经加载，避免重复创建或启动服务
        if (IsDriverServicePresent(ServiceName))
        {
            logger.outDebug(_xor_(L"驱动服务已存在，判定驱动已加载，跳过重复加载。"));
            hDevice = CreateFile(SYMBOLICLINK, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hDevice != INVALID_HANDLE_VALUE)
            {
                logger.outDebug(_xor_(L"连接驱动成功！"));
                return hDevice;
            }

            error = GetLastError();
            logger.outDebug(_xor_(L"驱动服务已存在，但设备连接失败！(error:%d)"), error);
            return INVALID_HANDLE_VALUE;
        }

        pfnLoadNT(DriveImagePath, ServiceName);

        logger.outDebug(_xor_(L"正在连接符号..."));
        hDevice = CreateFile(SYMBOLICLINK, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hDevice != INVALID_HANDLE_VALUE)
        {
            logger.outDebug(_xor_(L"连接驱动成功！"));
            return hDevice;
        }
        else
        {
            error = GetLastError();
            logger.outDebug(L"连接驱动失败，可能是驱动服务尚未安装! (error:%d)", error);
        }
    }

    return INVALID_HANDLE_VALUE;
}

//加载VT驱动
BOOL LoadVT(const std::wstring DriveImagePath, const std::wstring ServiceName)
{
    //VT 驱动服务已存在时判定驱动已经加载，避免重复加载失败
    if (IsDriverServicePresent(ServiceName))
    {
        logger.outDebug(_xor_(L"VT 驱动服务已存在，判定驱动已加载，跳过重复加载。"));
        return TRUE;
    }

    if (!pfnLoadNT(DriveImagePath, ServiceName))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
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

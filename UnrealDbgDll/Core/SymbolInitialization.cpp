#include "DllMainInternal.h"
#include "../../Common/Shared/DriverProtocol.h"

void InitSymbol()
{
    g_SymbolTable = { 0 };
    HMODULE hNtdll = GetModuleHandle(L"ntdll.dll");
    g_SymbolTable.lpDbgUiRemoteBreakin = (size_t)GetProcAddress(hNtdll, "DbgUiRemoteBreakin");
    g_SymbolTable.lpDbgBreakPoint = (size_t)GetProcAddress(hNtdll, "DbgBreakPoint");
}

void DispatchSymbol()
{
    InitSymbol();

    DWORD BytesReturned = 0;
    UNREFERENCED_PARAMETER(BytesReturned);
    // SendUserDataToDriver(IOCTL_DISPATCH_SYMBOL, &g_SymbolTable, sizeof(SYMBOL_TABLE), NULL, 0, &BytesReturned);
}

static thread_local const wchar_t* g_InitializeStage = L"not-started";
static constexpr ULONG64 kDriverHandshakeKey = VT_INIT_HANDSHAKE_KEY;

static const wchar_t* DescribeInitSymbolsStatus(DWORD status)
{
    switch (status)
    {
    case INIT_SYMBOLS_STATUS_SUCCESS:
        return L"success";
    case INIT_SYMBOLS_STATUS_INVALID_VERIFY:
        return L"invalid verify payload or key";
    case INIT_SYMBOLS_STATUS_NTOSKRNL_SYMBOLS:
        return L"InitNtoskrnlSymbolsTable failed";
    case INIT_SYMBOLS_STATUS_WIN32KBASE_SYMBOLS:
        return L"InitWin32kbaseSymbolsTable failed";
    case INIT_SYMBOLS_STATUS_WIN32KFULL_SYMBOLS:
        return L"InitWin32kfullSymbolsTable failed";
    case INIT_SYMBOLS_STATUS_DISPATCH_OFFSET:
        return L"DispatchOffsetToHost failed";
    case INIT_SYMBOLS_STATUS_DBGK_INITIALIZE:
        return L"DbgkInitialize failed";
    default:
        return L"unknown";
    }
}

static void LogInitializeFileMessage(const std::wstring& message)
{
    logger.Log("%s", Common::wideStringToString2(message).c_str());
}

static void SetInitializeStage(const wchar_t* stage)
{
    g_InitializeStage = stage;

    std::wstring message = L"[Initialize_F3] ";
    message += stage != nullptr ? stage : L"(null)";

    logger.outDebug(L"%s", message.c_str());
    LogInitializeFileMessage(message);
}

static int LogInitializeException(EXCEPTION_POINTERS* exceptionInfo)
{
    DWORD exceptionCode = 0;
    PVOID exceptionAddress = nullptr;
    DWORD64 instructionPointer = 0;

    if (exceptionInfo != nullptr)
    {
        if (exceptionInfo->ExceptionRecord != nullptr)
        {
            exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;
            exceptionAddress = exceptionInfo->ExceptionRecord->ExceptionAddress;
        }

        if (exceptionInfo->ContextRecord != nullptr)
        {
#if defined(_M_X64)
            instructionPointer = exceptionInfo->ContextRecord->Rip;
#elif defined(_M_IX86)
            instructionPointer = exceptionInfo->ContextRecord->Eip;
#endif
        }
    }

    wchar_t message[512] = {};
    swprintf_s(
        message,
        _countof(message),
        L"[Initialize_F3] SEH exception, stage=%s, code=0x%08lX, address=%p, ip=0x%llX",
        g_InitializeStage,
        exceptionCode,
        exceptionAddress,
        instructionPointer);

    logger.outDebug(L"%s", message);
    LogInitializeFileMessage(message);

    return EXCEPTION_EXECUTE_HANDLER;
}

static BOOL Initialize_F3_Impl(ULONG64 key)
{
    SetInitializeStage(L"begin");
    // VMProtectBegin("Initialize_F3");
    // logger.outDebug(L"Key 十六进制数据: 0x%016llX", key);
    if (key != kDriverHandshakeKey)
    {
        logger.outDebug(
            L"Initialize_F3 入口 key 已归一化: input=0x%016llX, driver=0x%016llX",
            key,
            kDriverHandshakeKey);
    }

    SetInitializeStage(L"EnsureMacCode");
    if (!EnsureMacCode())
    {
        return false;
    }

    SetInitializeStage(L"IniSoftInfo");
    if (!IniSoftInfo())
    {
        return false;
    }

    cardnum = (const char*)_xor_("cc-e6bdbf07bcb5dc7e");

    SetInitializeStage(L"LoginByCard");
    if (!LoginByCard(cardnum))
    {
        return false;
    }

    BOOL boSuccess = TRUE;
    std::wstring processPath = FileSystem::GetModuleDirectory(NULL);
    // VMProtectEnd();

    SetInitializeStage(L"_Initialize");
    if (_Initialize(processPath.c_str()))
    {
        SetInitializeStage(L"InitSymbolsTable");
        if (InitSymbolsTable(kDriverHandshakeKey))
        {
            SetInitializeStage(L"SendDebuggerDataToDriver");
            if (!SendDebuggerDataToDriver(GetCurrentProcessId()))
            {
                logger.outDebug(L"SendDebuggerDataToDriver failed. (error:%lu)", GetLastError());
                return false;
            }
        }
        else
        {
            return false;
        }

        SetInitializeStage(L"HeartBeatThread");
        std::thread HeartBeatThread(HeartBeat);
        HeartBeatThread.detach();
    }
    else
    {
        SetInitializeStage(L"_Initialize failed");
        return false;
    }

    SetInitializeStage(L"success");
    logger.outDebug(_xor_(L"初始化成功"));
    return boSuccess;
}

BOOL Initialize_F3(ULONG64 key)
{
    __try
    {
        return Initialize_F3_Impl(key);
    }
    __except (LogInitializeException(GetExceptionInformation()))
    {
        return FALSE;
    }
}

BOOL Initialize(ULONG64 key)
{
    UNREFERENCED_PARAMETER(key);
    logger.outDebug(L"兼容旧 UI Initialize 导出，使用当前驱动握手 key。");
    return Initialize_F3(kDriverHandshakeKey);
}

BOOL InitSymbolsTable(ULONG64 key)
{
    return LoadSymbolsTable(key);
}

BOOL LoadSymbolsTable(ULONG64 key)
{
    VMProtectBeginVirtualization("VMP");
    BOOL bRet;
    DWORD lastError = ERROR_SUCCESS;
    DWORD BytesReturned = 0;
    RING3_VERIFY info = { 0 };
    info.key = key;
    DWORD dwSuccess = 520;
    bRet = SendUserDataToDriver(IOCTL_LOAD_SYMBOLS_TABLE, &info, sizeof(RING3_VERIFY), &dwSuccess, sizeof(DWORD), &BytesReturned);
    lastError = GetLastError();
    if (dwSuccess != INIT_SYMBOLS_STATUS_SUCCESS)
    {
        logger.outDebug(
            L"InitSymbolsTable 失败: DeviceIoControl=%d, LastError=%lu, BytesReturned=%lu, DriverStatus=0x%08lX, Reason=%s",
            bRet,
            lastError,
            BytesReturned,
            dwSuccess,
            DescribeInitSymbolsStatus(dwSuccess));
        VMProtectEnd();
        return FALSE;
    }
    VMProtectEnd();
    return bRet;
}

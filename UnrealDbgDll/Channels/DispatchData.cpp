#include "../dllmain.h"
#include "../Init/InitNTDevice.h"
#include "DispatchData.h"

//派遣数据到驱动
BOOL DispatchDataToDriver(DWORD dwIoControlCode,
    PVOID inputBuffer,
    DWORD inputBufferSize,
    PVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned)
{
    BOOL bRet = FALSE;
    if (g_hGeneralDriverDevice == INVALID_HANDLE_VALUE)
    {
        logger.outDebug(L"DeviceIoControl skipped: driver handle invalid, ioctl=0x%08lX", dwIoControlCode);
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    DWORD localBytesReturned = 0;
    LPDWORD bytesReturned = lpBytesReturned != nullptr ? lpBytesReturned : &localBytesReturned;
    bRet = DeviceIoControl(
        g_hGeneralDriverDevice,
        dwIoControlCode,
        inputBuffer,
        inputBufferSize,
        lpOutBuffer,
        nOutBufferSize,
        bytesReturned,
        NULL);
    if (!bRet)
    {
        DWORD error = GetLastError();
        logger.outDebug(
            L"DeviceIoControl failed: ioctl=0x%08lX, input=%lu, output=%lu, bytes=%lu, error=%lu",
            dwIoControlCode,
            inputBufferSize,
            nOutBufferSize,
            *bytesReturned,
            error);
        SetLastError(error);
    }
    return bRet;
}

BOOL SendUserDataToDriver(DWORD dwIoControlCode,
    PVOID source,
    SIZE_T size,
    PVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned)
{
    BOOL bRet;
    std::string encodeData;
    USER_DATA userData = { 0 };
    if ((source == nullptr && size != 0) || size > USER_DATA_MAX_PLAINTEXT_SIZE)
    {
        return FALSE;
    }

    DWORD inputBufferSize = sizeof(USER_DATA);
    userData.uSize = static_cast<ULONG>(size);  //记录明文长度
    if (source)
    {
        encodeData = EncryptData((const char*)source, size, KEY);
        if (encodeData.empty() ||
            encodeData.size() + 1 > (static_cast<SIZE_T>(MAXDWORD) - sizeof(USER_DATA)))
        {
            return FALSE;
        }

        userData.Count = USER_DATA_INLINE_PACKET_MAGIC;
        userData.pUserData = sizeof(USER_DATA);
        inputBufferSize = static_cast<DWORD>(sizeof(USER_DATA) + encodeData.size() + 1);
    }

    std::vector<BYTE> inputBuffer(inputBufferSize);
    memcpy(inputBuffer.data(), &userData, sizeof(userData));
    if (!encodeData.empty())
    {
        memcpy(inputBuffer.data() + sizeof(USER_DATA), encodeData.c_str(), encodeData.size() + 1);
    }

    bRet = DispatchDataToDriver(dwIoControlCode,
        inputBuffer.data(),
        inputBufferSize,
        lpOutBuffer,
        nOutBufferSize,
        lpBytesReturned);
    return bRet;
}

// 旧项目保留代码：GetDriverData 直接把 ring3 指针塞进 USER_DATA 后交给驱动回写。
// 注释原因：重构后统一走 SendUserDataToDriver/DispatchDataToDriver，入参会先加密并按 inline packet 传输；旧写法依赖驱动直接信任用户态指针，边界和 WOW64 兼容风险更高。
// 放开好处：少一次输出缓冲封装，旧调用点如果仍按 USER_DATA.pUserData 回填会更直接。
// 放开坏处：破坏当前 IOCTL 输入规范，驱动侧 ResolveInputUserData 无法统一校验，错误指针会扩大内核异常面。
// 项目影响：继续注释不影响当前调试器启动、符号下发、进程信息查询；放开需要同步恢复旧驱动分发协议。
// 当前建议：继续注释，仅作为旧协议参考。
//ULONG GetDriverData(DWORD dwIoControlCode, PVOID pBuf)
//{
//    USER_DATA userData = { 0 };
//    userData.pUserData = (ULONG64)pBuf;
//    if (DispatchDataToDriver(dwIoControlCode, &userData))
//    {
//        return userData.uSize;
//    }
//    return 0;
//}

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
    if (g_hGeneralDriverDevice != INVALID_HANDLE_VALUE)
    {
        bRet = DeviceIoControl(g_hGeneralDriverDevice,
            dwIoControlCode,
            inputBuffer,
            inputBufferSize,
            lpOutBuffer,
            nOutBufferSize,
            lpBytesReturned,
            NULL);
        DWORD dwError = GetLastError();
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

//获取驱动层数据
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

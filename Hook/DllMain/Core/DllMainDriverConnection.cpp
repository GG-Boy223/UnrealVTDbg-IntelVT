#include "DllMainInternal.h"

namespace HookDllMainInternal
{
    bool VerifyDriverHandshake()
    {
        RING3_VERIFY request = { 0 };
        HOOK_DRIVER_HANDSHAKE_REPLY reply = { 0 };
        DWORD bytesReturned = 0;

        request.key = HOOK_DRIVER_HANDSHAKE_KEY;
        if (!SendUserDataToDriver(IOCTL_VERIFY_HOOK_CLIENT,
            &request,
            sizeof(request),
            &reply,
            sizeof(reply),
            &bytesReturned))
        {
            //logger.Log("驱动握手失败，DeviceIoControl error:%d", GetLastError());
            logger.Log("链接严重错误", GetLastError());

            return false;
        }

        if (bytesReturned < sizeof(reply))
        {
            //logger.Log("驱动握手失败，返回长度不足:%lu/%zu", bytesReturned, sizeof(reply));
            logger.Log("链接错误", GetLastError());
            return false;
        }

        if (reply.magic != HOOK_DRIVER_HANDSHAKE_MAGIC ||
            reply.version != HOOK_DRIVER_HANDSHAKE_VERSION ||
            reply.size != sizeof(HOOK_DRIVER_HANDSHAKE_REPLY) ||
            reply.status != HOOK_DRIVER_HANDSHAKE_STATUS_OK)
        {
            //logger.Log("驱动握手失败，magic:%llX version:%lu size:%lu status:%lu",
            //  reply.magic,
            //  reply.version,
            //  reply.size,
            //  reply.status);

            logger.Log("链接失败");
            return false;
        }

        //logger.Log("驱动握手成功，pid:%lu build:%lu flags:%lu", reply.pid, reply.build, reply.flags);
        logger.Log("链接成功");
        return true;
    }
}

//初始化设备
BOOL InitializeDevice()
{
    VMProtectionScope vmpScope;

    g_hGeneralDriverDevice = CreateDeviceHandle();
    if (g_hGeneralDriverDevice == INVALID_HANDLE_VALUE)
    {
        logger.Log("连接驱动失败 error: %d", GetLastError());
        return FALSE;
    }

    if (!HookDllMainInternal::VerifyDriverHandshake())
    {
        CloseHandle(g_hGeneralDriverDevice);
        g_hGeneralDriverDevice = INVALID_HANDLE_VALUE;
        return FALSE;
    }
    return TRUE;
}

//连接驱动
HANDLE CreateDeviceHandle()
{
    DWORD error = 0;
    return CreateFile(SYMBOLICLINK, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

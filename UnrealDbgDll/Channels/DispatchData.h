#pragma once

#ifndef _DISPATCH_DATA_H
#define _DISPATCH_DATA_H

//派遣数据到驱动
BOOL DispatchDataToDriver(DWORD dwIoControlCode,
    PVOID inputBuffer,
    DWORD inputBufferSize,
    PVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned);

BOOL SendUserDataToDriver(DWORD dwIoControlCode,
    PVOID source,
    SIZE_T size,
    PVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned);

//获取驱动层数据
ULONG GetDriverData(DWORD dwIoControlCode, PVOID pBuf);


#endif // !_DISPATCH_DATA_H

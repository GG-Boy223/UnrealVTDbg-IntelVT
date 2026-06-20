#pragma once

#ifndef _DISPATCHDATA_H
#define _DISPATCHDATA_H

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


#endif // !_DISPATCHDATA_H

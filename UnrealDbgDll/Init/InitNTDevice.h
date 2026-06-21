#pragma once

#ifndef _INIT_NTDEVICE_H
#define _INIT_NTDEVICE_H

extern HANDLE g_hGeneralDriverDevice;

//初始化驱动设备
BOOL InitializeDevice(const std::wstring DriveImagePath);

BOOL _Initialize(const TCHAR* sPath);
//结束时的扫尾工作
BOOL UnInitialize();

//BOOL UnInitialize_END();

#endif // !_INIT_NTDEVICE_H

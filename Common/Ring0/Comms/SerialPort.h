#pragma once


#ifndef _SERIAL_PORT_H
#define _SERIAL_PORT_H

#define PRINTF_LONG_SUPPORT
#define PRINTF_LONG_LONG_SUPPORT

//#define _SERIAL_DEBUG_LOG


// 初始化 VMware / 标准 UART 的 COM1（0x3F8）。
//
// 当前用途：
// 1. 在 VT_Driver 的 DriverEntry 最开始调用一次；
// 2. 用于验证“虚拟机 COM1 -> 宿主机文件”链路是否打通。
//
// 这里固定初始化为：
// - COM1 基址：0x3F8
// - 波特率：115200
// - 数据格式：8N1
void InitializeSerialPort();
void Log(const char* format, ...);


#ifdef _SERIAL_DEBUG_LOG
#define sLog(format, ...) \
	Log("[%s:%d]" format , __func__, __LINE__, __VA_ARGS__)
#else
#define sLog(format, ...)
#endif // _SERIAL_DEBUG_LOG

#endif // !_SERIAL_PORT_H

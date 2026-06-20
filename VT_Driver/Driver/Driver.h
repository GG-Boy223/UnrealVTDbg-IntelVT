#pragma once

#ifndef _DRIVER_H
#define _DRIVER_H

#include <ntifs.h>
#include <intrin.h>
#include <Shared/IOCTLs.h>
#include <Shared/SharedStruct.h>
#include <Ring0/ia32-doc/out/ia32.hpp>
#include <Ring0/SymbolicAccess/Utils/Log.h>
#include <Ring0/SymbolicAccess/ModuleExtender/ModuleExtenderFactory.h>
#include <Ring0/String/StringHandler.h>
#include <Ring0/Spinlock/spinlock.h>
#include <Ring0/Comms/SerialPort.h>
#include <stdarg.h>

#define SYMBOLIC  L"\\DosDevices\\VT_Device"

//#define _DEBUG_LOG


#ifdef _DEBUG_LOG

#define outDebug(format, ...) \
	DebugPrintf_3(format,__VA_ARGS__)

#else

#define outDebug(format, ...)

#endif // _DEBUG_LOG

void VtLoadWriteLog(_In_z_ _Printf_format_string_ const char* format, ...);

#define outFile(...) \
	VtLoadWriteLog(__VA_ARGS__)


NTSTATUS Dispatcher_Create_Close(_In_ PDEVICE_OBJECT device_object, _In_ PIRP irp);
NTSTATUS Dispatcher_IOCTL_Function(_In_ PDEVICE_OBJECT device_object, _In_ PIRP irp);

//bool InitOffset(PWINDOWS_STRUCT vmcallinfo);

// vmcallinfo 仍然是来宾中的 offset 结构地址；
// verifyKey 来自 VMCALL 的第二个参数（r8）。
bool InitOffset(PWINDOWS_STRUCT vmcallinfo, unsigned __int64 verifyKey);

PCLIENT_ID GuestCurrentThreadCid();

bool SetBreakpoint(PVT_BREAK_POINT vmcallinfo, unsigned __int64 Type);
bool RemoveBreakpoint(PVT_BREAK_POINT vmcallinfo);

void MyKeBugCheck(
    _In_ ULONG BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter1,
    _In_ ULONG_PTR BugCheckParameter2,
    _In_ ULONG_PTR BugCheckParameter3,
    _In_ ULONG_PTR BugCheckParameter4
);

//--------------------------------------------------

#ifdef _DEBUG_Key_Log
// ================================
// InitOffset 延后打印所需的全局状态
// ================================

// 1 表示有一条待打印的 InitOffset 结果；
// 0 表示当前没有待打印日志。
extern volatile LONG g_InitOffsetLogPending;

// 最近一次成功设置的 Cid 值。
extern size_t g_InitOffsetLastCid;

// 最近一次收到的 DirectoryTableBase 偏移。
extern size_t g_InitOffsetLastDirectoryTableBase;

// 日志线程句柄，用于驱动卸载时关闭句柄。
extern HANDLE g_InitOffsetLogThreadHandle;

// 线程退出标志：
// 0 = 继续运行
// 1 = 请求退出
extern volatile LONG g_StopInitOffsetLogThread;

// InitOffset 延后打印线程。
VOID InitOffsetLogThread(_In_ PVOID StartContext);

#endif




inline void DebugPrintf_3(const char* text, ...)
{
	va_list(args);
	va_start(args, text);

	vDbgPrintExWithPrefix("[VT_Driver] : ", 0, 0, text, args);

	va_end(args);
}

#endif // !_DRIVER_H

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

#define outFile(...) VtLoadWriteLog(__VA_ARGS__)


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
    _In_ ULONG BugCheckCode, // 蓝屏主错误码
    _In_ ULONG_PTR BugCheckParameter1, // 参数1：问题上下文相关信息
    _In_ ULONG_PTR BugCheckParameter2, // 参数2：问题上下文相关信息
    _In_ ULONG_PTR BugCheckParameter3, // 参数3：问题上下文相关信息
    _In_ ULONG_PTR BugCheckParameter4 // 参数4：问题上下文相关信息
);

//--------------------------------------------------

inline void DebugPrintf_3(const char* text, ...)
{
	va_list(args);
	va_start(args, text);

	vDbgPrintExWithPrefix("[VT_Driver] : ", 0, 0, text, args);

	va_end(args);
}

#endif // !_DRIVER_H

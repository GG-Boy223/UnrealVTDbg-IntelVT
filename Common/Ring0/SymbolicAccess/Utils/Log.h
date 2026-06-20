#pragma once
#include <string_view>
#include <stdarg.h>

//#define DEBUG_WinSys

enum __log_type
{
	LOG_TYPE_DEBUG,
	LOG_TYPE_ERROR,
	LOG_TYPE_DUMP,
	LOG_TYPE_INFO
};

namespace symbolic_access
{
	void PrintToDebugger(std::string_view Format, ...);
	void LogPrint(__log_type type, const char* fmt, ...);
	inline void DebugPrintf_3(const char* text, ...)
	{
#ifdef DEBUG_WinSys
		va_list(args);
		va_start(args, text);

		vDbgPrintExWithPrefix("[DbgkSysWin] : ", 0, 0, text, args);

		va_end(args);
#endif
	}

#ifdef ENABLE_LOGGING
#define PrintDbg(Format, ...) PrintToDebugger("[SymbolicAccess][%s:%d]: " Format, __func__, __LINE__, __VA_ARGS__)
#else
#define PrintDbg(Format, ...) 
#endif
}


#define outToFile(format, ...) \
	symbolic_access::DebugPrintf_3(format, __VA_ARGS__);//symbolic_access::LogPrint(LOG_TYPE_DEBUG," [%s:%d] " format , __func__, __LINE__, __VA_ARGS__)

#ifdef DEBUG_WinSys

#define LogError(format, ...) \
    symbolic_access::DebugPrintf_3(format, __VA_ARGS__);//symbolic_access::DebugPrintf_3(format, __VA_ARGS__);//symbolic_access::LogPrint(LOG_TYPE_ERROR," [%s:%d] " format , __func__, __LINE__, __VA_ARGS__)
#define LogDebug(format, ...) \
    symbolic_access::DebugPrintf_3(format, __VA_ARGS__);//symbolic_access::LogPrint(LOG_TYPE_DEBUG," [%s:%d] " format , __func__, __LINE__, __VA_ARGS__)
#define LogDump(format, ...) \
   symbolic_access::DebugPrintf_3(format, __VA_ARGS__);// symbolic_access::LogPrint(LOG_TYPE_DUMP," [%s:%d] " format , __func__, __LINE__, __VA_ARGS__)
#define LogInfo(format, ...) \
    symbolic_access::DebugPrintf_3(format, __VA_ARGS__);//symbolic_access::LogPrint(LOG_TYPE_INFO," [%s:%d] " format , __func__, __LINE__, __VA_ARGS__)

#define outLog(format, ...) \
	symbolic_access::DebugPrintf_3(format, __VA_ARGS__);//symbolic_access::LogPrint(LOG_TYPE_DEBUG," [%s:%d] " format , __func__, __LINE__, __VA_ARGS__)

#else

#define LogError(format, ...)
#define LogDebug(format, ...) 
#define LogDump(format, ...)
#define LogInfo(format, ...)
#define outLog(format, ...)

#endif // DEBUG

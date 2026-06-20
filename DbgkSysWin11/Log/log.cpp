#include "../Driver.h"
#include "log.h"

void LogPrint(__log_type type, const char* fmt, ...)
{
	// 旧项目保留的禁用路径：按日志类型拼接时间戳、消息正文，再通过 DbgPrintEx 输出。
	// 为什么注释：当前 LogPrint 默认空实现，避免内核热路径产生大量调试输出和格式化开销。
	// 放开的好处：调试驱动初始化、EPT hook、调试对象事件时可以直接看到带时间戳的日志。
	// 放开的坏处：DbgPrintEx 在高频路径会造成性能抖动，格式化缓冲区长度固定，过长消息会被截断。
	// 项目影响：打开后会改变内核日志噪声和时序，可能影响调试事件、VM-exit 相关问题复现。
	// 结论：继续注释；需要诊断时建议短期开启并限定调用点。
	//char* LogType = NULL;
	//LARGE_INTEGER SystemTime = {};
	//LARGE_INTEGER LocalTime = {};
	//TIME_FIELDS TimeFields = {};
	//char TimeBuffer[20] = {};
	//char MessageBuffer[412] = {};
	//char* OutputFormat = NULL;
	//char OutputBuffer[512] = {};
	//va_list Args = {};

	//switch (type)
	//{
	//case LOG_TYPE_DEBUG:
	//{
	//	LogType = "[DEBUG]";
	//	break;
	//}
	//case LOG_TYPE_DUMP:
	//{
	//	LogType = "[DUMP]";
	//	break;
	//}
	//case LOG_TYPE_ERROR:
	//{
	//	LogType = "[ERROR]";
	//	;			break;
	//}
	//case LOG_TYPE_INFO:
	//{
	//	LogType = "[INFORMATION]";
	//	break;
	//}
	//default:
	//{
	//	break;
	//}

	//}

	//KeQuerySystemTime(&SystemTime);
	//ExSystemTimeToLocalTime(&SystemTime, &LocalTime);
	//RtlTimeToTimeFields(&LocalTime, &TimeFields);

	//RtlStringCchPrintfA(
	//	TimeBuffer,
	//	sizeof(TimeBuffer),
	//	"[%02hd:%02hd:%02hd.%03hd]",
	//	TimeFields.Hour,
	//	TimeFields.Minute,
	//	TimeFields.Second,
	//	TimeFields.Milliseconds);

	//va_start(Args, fmt);
	//RtlStringCchVPrintfA(MessageBuffer, sizeof(MessageBuffer), fmt, Args);
	//va_end(Args);

	//OutputFormat = "%s  %s  %s\r\n";

	//RtlStringCchPrintfA(
	//	OutputBuffer,
	//	sizeof(OutputBuffer),
	//	OutputFormat,
	//	TimeBuffer,
	//	LogType,
	//	MessageBuffer);

	//DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "%s", OutputBuffer);
}

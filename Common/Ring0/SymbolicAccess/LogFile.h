#pragma once

#ifndef _LOG_FILE_H
#define _LOG_FILE_H

namespace LogFile
{
	typedef struct _THREAD_DATA {
		// 在这里定义你需要的线程参数
		// ...
	} THREAD_DATA, * PTHREAD_DATA;

	extern bool boLogInit;

	NTSTATUS CreateLogsDirectory(PWCHAR path);

	NTSTATUS WriteLogToXmlFileW(PWCHAR logMessage);

	NTSTATUS WriteLogToXmlFileA(PCHAR logMessage);

	NTSTATUS LogDriverMessageW(PWCHAR message);

	NTSTATUS LogDriverMessageA(PCHAR message);

	NTSTATUS SetLogFilePath(PWCHAR path);

	NTSTATUS ReadIniValue(_In_ PCWSTR filePath, _In_ PCWSTR sectionName, _In_ PCWSTR keyName, _Out_ PWSTR value, _In_ ULONG valueSize);

	PWSTR ReadIni(_In_ PCWSTR filePath, _In_ PCWSTR sectionName, _In_ PCWSTR keyName);

	NTSTATUS InitDriverLog();

	NTSTATUS CreateInternalThread(PKSTART_ROUTINE StartRoutine, PVOID StartContext, PETHREAD* Thread);

	NTSTATUS CreateKernelThread(PKSTART_ROUTINE StartRoutine, PTHREAD_DATA threadData, PETHREAD* Thread);

	VOID KernelSleep(UINT32 milliseconds);

	VOID RemovePath(WCHAR* fullPath);
}


#endif // !_LOG_FILE_H

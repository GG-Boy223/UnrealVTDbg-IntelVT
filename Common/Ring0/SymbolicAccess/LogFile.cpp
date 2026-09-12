#include <ntifs.h>
#include <ntstrsafe.h>
#include "Utils/Log.h"
#include "LogFile.h"

#define MAX_BUFFER_SIZE 256

namespace LogFile
{
    bool boLogInit;
    //FAST_MUTEX Mutex;          //互斥锁
    UNICODE_STRING logFilePath; // 日志文件路径

    NTSTATUS CreateLogsDirectory(PWCHAR path)
    {
        OBJECT_ATTRIBUTES objectAttributes;
        UNICODE_STRING directoryName;
        IO_STATUS_BLOCK ioStatus;
        HANDLE directoryHandle;
        NTSTATUS status;

        RtlInitUnicodeString(&directoryName, path);

        InitializeObjectAttributes(&objectAttributes, &directoryName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        status = ZwCreateFile(&directoryHandle, FILE_LIST_DIRECTORY | SYNCHRONIZE, &objectAttributes, &ioStatus, NULL, FILE_ATTRIBUTE_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN_IF, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        ZwClose(directoryHandle);
        return STATUS_SUCCESS;
    }

    NTSTATUS WriteLogToXmlFileW(PWCHAR logMessage)
    {
        HANDLE fileHandle;
        IO_STATUS_BLOCK ioStatus;
        OBJECT_ATTRIBUTES objectAttributes;
        NTSTATUS status;

        InitializeObjectAttributes(&objectAttributes, &logFilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
        status = ZwCreateFile(&fileHandle,
            FILE_APPEND_DATA,
            &objectAttributes,
            &ioStatus,
            NULL,
            FILE_ATTRIBUTE_NORMAL,
            NULL,
            FILE_OPEN_IF,
            FILE_SYNCHRONOUS_IO_NONALERT,
            NULL,
            0);
        if (!NT_SUCCESS(status))
        {
            outLog("ZwCreateFile日志文件创建失败!");
            return status;
        }

        status = ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatus, logMessage, (ULONG)wcslen(logMessage) * sizeof(WCHAR), NULL, NULL);
        if (!NT_SUCCESS(status))
        {
            outLog("ZwWriteFile日志文件写入失败!");
            ZwClose(fileHandle);
            return status;
        }

        ZwClose(fileHandle);
        return STATUS_SUCCESS;
    }

    NTSTATUS WriteLogToXmlFileA(PCHAR logMessage)
    {
        HANDLE fileHandle;
        IO_STATUS_BLOCK ioStatus;
        OBJECT_ATTRIBUTES objectAttributes;
        NTSTATUS status;

        InitializeObjectAttributes(&objectAttributes, &logFilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
        status = ZwCreateFile(&fileHandle,
            FILE_APPEND_DATA,
            &objectAttributes,
            &ioStatus,
            NULL,
            FILE_ATTRIBUTE_NORMAL,
            NULL,
            FILE_OPEN_IF,
            FILE_SYNCHRONOUS_IO_NONALERT,
            NULL,
            0);
        if (!NT_SUCCESS(status))
        {
            outLog("ZwCreateFile日志文件创建失败!");
            return status;
        }

        status = ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatus, logMessage, (ULONG)strlen(logMessage), NULL, NULL);
        if (!NT_SUCCESS(status))
        {
            outLog("ZwWriteFile日志文件写入失败!");
            ZwClose(fileHandle);
            return status;
        }

        ZwClose(fileHandle);
        return STATUS_SUCCESS;
    }

    NTSTATUS LogDriverMessageW(PWCHAR message)
    {
        NTSTATUS status;
        LARGE_INTEGER systemTime;
        TIME_FIELDS timeFields;
        WCHAR logMessage[256];

        // 获取当前系统时间
        KeQuerySystemTime(&systemTime);
        ExSystemTimeToLocalTime(&systemTime, &systemTime);
        RtlTimeToTimeFields(&systemTime, &timeFields);

        // 将包含日期的日志信息格式化为XML格式
        RtlStringCchPrintfW(logMessage, 
        	sizeof(logMessage), 
        	L"<LogEntry Date=\"%04u-%02u-%02u\" Time=\"%02u:%02u:%02u.%03u\">%s</LogEntry>\n", 
        	timeFields.Year, 
        	timeFields.Month, 
        	timeFields.Day, 
        	timeFields.Hour, 
        	timeFields.Minute, 
        	timeFields.Second, 
        	timeFields.Milliseconds, 
        	message);

        // 写入日志信息到XML文件
        status = WriteLogToXmlFileW(logMessage);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        return STATUS_SUCCESS;
    }

    NTSTATUS LogDriverMessageA(PCHAR message)
    {
        NTSTATUS status;
        LARGE_INTEGER systemTime;
        TIME_FIELDS timeFields;
        CHAR logMessage[256];

        // 获取当前系统时间
        KeQuerySystemTime(&systemTime);
        ExSystemTimeToLocalTime(&systemTime, &systemTime);
        RtlTimeToTimeFields(&systemTime, &timeFields);

        // 将包含日期的日志信息格式化为XML格式
        RtlStringCchPrintfA(logMessage,
            sizeof(logMessage),
            "<LogEntry Date=\"%04u-%02u-%02u\" Time=\"%02u:%02u:%02u.%03u\">%s</LogEntry>\n",
            timeFields.Year,
            timeFields.Month,
            timeFields.Day,
            timeFields.Hour,
            timeFields.Minute,
            timeFields.Second,
            timeFields.Milliseconds,
            message);

        // 写入日志信息到XML文件
        status = WriteLogToXmlFileA(logMessage);
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        return STATUS_SUCCESS;
    }

    NTSTATUS SetLogFilePath(PWCHAR path)
    {
        RtlInitUnicodeString(&logFilePath, path);
        return STATUS_SUCCESS;
    }

    NTSTATUS ConvertUnicodeToAnsi(PCWSTR unicodeString, PCHAR* ansiString)
    {
        UNICODE_STRING unicodeStr;
        ANSI_STRING ansiStr;

        RtlInitUnicodeString(&unicodeStr, unicodeString);
        NTSTATUS status = RtlUnicodeStringToAnsiString(&ansiStr, &unicodeStr, TRUE);

        if (NT_SUCCESS(status))
        {
            *ansiString = ansiStr.Buffer;
        }

        return status;
    }

    NTSTATUS ReadIniValue(_In_ PCWSTR filePath, _In_ PCWSTR sectionName, _In_ PCWSTR keyName, _Out_ PWSTR value, _In_ ULONG valueSize)
    {
        NTSTATUS status = STATUS_UNSUCCESSFUL;

        //// 打开INI文件
        //HANDLE fileHandle;
        //IO_STATUS_BLOCK ioStatusBlock;
        //UNICODE_STRING unicodeFilePath;
        //RtlInitUnicodeString(&unicodeFilePath, filePath);
        //OBJECT_ATTRIBUTES objectAttributes;
        //InitializeObjectAttributes(&objectAttributes, &unicodeFilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
        //status = ZwCreateFile(&fileHandle, GENERIC_READ, &objectAttributes, &ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
        //if (!NT_SUCCESS(status))
        //{
        //    KdPrint(("Failed to open file. Status: 0x%X\n", status));
        //    return status;
        //}

        //// 读取INI文件内容
        //CHAR buffer[512];
        //ULONG bytesRead;
        //status = ZwReadFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock, buffer, sizeof(buffer) - sizeof(CHAR), NULL, NULL);
        //if (!NT_SUCCESS(status))
        //{
        //    KdPrint(("Failed to read file. Status: 0x%X\n", status));
        //    ZwClose(fileHandle);
        //    return status;
        //}

        //// 关闭INI文件
        //ZwClose(fileHandle);

        //// 解析INI文件内容
        //PCHAR section = NULL;
        //PCHAR key = NULL;
        //PCHAR valueStart = NULL;
        //PCHAR valueEnd = NULL;
        //BOOLEAN inTargetSection = FALSE;
        //PCHAR token = strtok(buffer, "\r\n");
        //while (token != NULL)
        //{
        //    if (token[0] == '[' && token[strlen(token) - 1] == ']')
        //    {
        //        // 判断是否为目标节
        //        token[strlen(token) - 1] = '\0';

        //        PCHAR ansiString = NULL;
        //        status = ConvertUnicodeToAnsi(sectionName, &ansiString);
        //        if (!NT_SUCCESS(status))
        //        {
        //            return status;
        //        }

        //        if (strcmp(token + 1, ansiString) == 0)
        //        {
        //            inTargetSection = TRUE;
        //        }
        //        else
        //        {
        //            inTargetSection = FALSE;
        //        }

        //        // 释放资源
        //        if (ansiString != NULL)
        //        {
        //            RtlFreeAnsiString((PANSI_STRING)&ansiString);
        //        }
        //    }
        //    else if (inTargetSection)
        //    {
        //        // 判断是否为目标键
        //        PCHAR equalSign = strchr(token, '=');
        //        if (equalSign != NULL)
        //        {
        //            *equalSign = '\0';

        //            PCHAR ansiString = NULL;
        //            status = ConvertUnicodeToAnsi(keyName, &ansiString);
        //            if (!NT_SUCCESS(status))
        //            {
        //                return status;
        //            }


        //            if (strcmp(token, ansiString) == 0)
        //            {
        //                valueStart = equalSign + 1;
        //                valueEnd = token + strlen(token);
        //            }

        //            // 释放资源
        //            if (ansiString != NULL)
        //            {
        //                RtlFreeAnsiString((PANSI_STRING)&ansiString);
        //            }
        //        }
        //    }

        //    token = strtok(NULL, "\r\n");
        //}

        //// 复制键值到输出缓冲区
        //if (valueStart != NULL && valueEnd != NULL && valueSize >= valueEnd - valueStart + 1)
        //{
        //    RtlCopyMemory(value, valueStart, valueEnd - valueStart);
        //    value[valueEnd - valueStart] = '\0';
        //    status = STATUS_SUCCESS;
        //}
        //else
        //{
        //    status = STATUS_BUFFER_TOO_SMALL;
        //}

        return status;
    }

    PWSTR ReadIni(_In_ PCWSTR filePath, _In_ PCWSTR sectionName, _In_ PCWSTR keyName)
    {
        static WCHAR valueBuffer[256];  // 用于存储键值的缓冲区

        NTSTATUS status = ReadIniValue(filePath, sectionName, keyName, valueBuffer, sizeof(valueBuffer));

        if (!NT_SUCCESS(status))
        {
            // 读取键值失败
            // ...
            return nullptr;
        }

        return valueBuffer;
    }


    NTSTATUS InitDriverLog()
    {
        NTSTATUS status;         

        // 创建Logs目录
        status = LogFile::CreateLogsDirectory(L"\\??\\C:\\Logs");
        if (!NT_SUCCESS(status))
        {
            outLog("创建Logs目录失败!");
            return status;
        }

        //创建日志文件
        status = LogFile::SetLogFilePath(L"\\??\\C:\\Logs\\driver.xml");
        if (!NT_SUCCESS(status))
        {
            outLog("设置日志文件失败!");
            return status;
        }

        boLogInit = true;

        // 记录日志信息
        //status = Common::LogDriverMessage(L"记录日志信息 Driver loaded."); // 测试日志信息
        //if (!NT_SUCCESS(status))
        //{
        //    outLog(("记录日志信息失败！\n"));
        //    return status;
        //}
        return STATUS_SUCCESS;
    }

    NTSTATUS CreateInternalThread(PKSTART_ROUTINE StartRoutine, PVOID StartContext, PETHREAD* Thread)
    {
        NTSTATUS status;
        HANDLE hThread;
        OBJECT_ATTRIBUTES objectAttributes;
        InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

        // 创建线程
        status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, &objectAttributes, NULL, NULL, StartRoutine, StartContext);
        if (!NT_SUCCESS(status))
        {
            // 处理错误
            return status;
        }

        // 获取线程对象指针
        //status = ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, (PVOID*)Thread, NULL);
        //if (!NT_SUCCESS(status))
        //{
        //    // 处理错误
        //    ZwClose(hThread);
        //    return status;
        //}

        // 关闭线程句柄
        ZwClose(hThread);

        return STATUS_SUCCESS;
    }

    //NTSTATUS CreateKernelThread(PKSTART_ROUTINE StartRoutine, PTHREAD_DATA threadData, PETHREAD* Thread)
    //{
    //    // 创建线程
    //    return CreateInternalThread(StartRoutine, threadData, Thread);
    //}

    VOID KernelSleep(UINT32 milliseconds)
    {
        LARGE_INTEGER delay;
        delay.QuadPart = -((LONGLONG)milliseconds * 10 * 1000);  // 转换为100纳秒单位

        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }

    VOID RemovePath(WCHAR* fullPath)
    {
        WCHAR* lastSlash = wcsrchr(fullPath, L'\\');  // 在字符串中查找最后一个目录分隔符

        if (lastSlash != NULL)
        {
            WCHAR* fileName = lastSlash + 1;  // 跳过目录分隔符
            wcscpy_s(fullPath, wcslen(fileName) + 1, fileName);
        }
    }
}

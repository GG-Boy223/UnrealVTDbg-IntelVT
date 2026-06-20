#include <ntifs.h>
#include <ntstrsafe.h>
#include "Utils/Log.h"
#include "LogFile.h"

#define MAX_BUFFER_SIZE 256

namespace LogFile
{
    bool boLogInit;
    //FAST_MUTEX Mutex;          //������
    UNICODE_STRING logFilePath; // ��־�ļ�·��

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
            outLog("ZwCreateFile��־�ļ�ʧ��!");
            return status;
        }

        status = ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatus, logMessage, (ULONG)wcslen(logMessage) * sizeof(WCHAR), NULL, NULL);
        if (!NT_SUCCESS(status))
        {
            outLog("ZwWriteFile��־�ļ�ʧ��!");
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
            outLog("ZwCreateFile��־�ļ�ʧ��!");
            return status;
        }

        status = ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatus, logMessage, (ULONG)strlen(logMessage), NULL, NULL);
        if (!NT_SUCCESS(status))
        {
            outLog("ZwWriteFile��־�ļ�ʧ��!");
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

        // ��ȡ��ǰϵͳʱ��
        KeQuerySystemTime(&systemTime);
        ExSystemTimeToLocalTime(&systemTime, &systemTime);
        RtlTimeToTimeFields(&systemTime, &timeFields);

        // ��ʽ���������ڵ���־��ϢΪXML��ʽ
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

        // д����־��Ϣ��XML�ļ�
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

        // ��ȡ��ǰϵͳʱ��
        KeQuerySystemTime(&systemTime);
        ExSystemTimeToLocalTime(&systemTime, &systemTime);
        RtlTimeToTimeFields(&systemTime, &timeFields);

        // ��ʽ���������ڵ���־��ϢΪXML��ʽ
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

        // д����־��Ϣ��XML�ļ�
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

        //// ��INI�ļ�
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

        //// ��ȡINI�ļ�����
        //CHAR buffer[512];
        //ULONG bytesRead;
        //status = ZwReadFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock, buffer, sizeof(buffer) - sizeof(CHAR), NULL, NULL);
        //if (!NT_SUCCESS(status))
        //{
        //    KdPrint(("Failed to read file. Status: 0x%X\n", status));
        //    ZwClose(fileHandle);
        //    return status;
        //}

        //// �ر�INI�ļ�
        //ZwClose(fileHandle);

        //// ����INI�ļ�����
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
        //        // �ж��Ƿ����Ŀ���
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

        //        // �ͷ���Դ
        //        if (ansiString != NULL)
        //        {
        //            RtlFreeAnsiString((PANSI_STRING)&ansiString);
        //        }
        //    }
        //    else if (inTargetSection)
        //    {
        //        // �ж��Ƿ���Ŀ���
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

        //            // �ͷ���Դ
        //            if (ansiString != NULL)
        //            {
        //                RtlFreeAnsiString((PANSI_STRING)&ansiString);
        //            }
        //        }
        //    }

        //    token = strtok(NULL, "\r\n");
        //}

        //// ���Ƽ�ֵ�����������
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
        static WCHAR valueBuffer[256];  // ���ڴ洢��ֵ�Ļ�����

        NTSTATUS status = ReadIniValue(filePath, sectionName, keyName, valueBuffer, sizeof(valueBuffer));

        if (!NT_SUCCESS(status))
        {
            // �������������ӡ������־���׳��쳣��
            // ...
            return nullptr;
        }

        return valueBuffer;
    }


    NTSTATUS InitDriverLog()
    {
        NTSTATUS status;         

        // ����LogsĿ¼
        status = LogFile::CreateLogsDirectory(L"\\??\\C:\\Logs");
        if (!NT_SUCCESS(status))
        {
            outLog("����Ŀ¼ʧ��!");
            return status;
        }

        //������־�ļ�
        status = LogFile::SetLogFilePath(L"\\??\\C:\\Logs\\driver.xml");
        if (!NT_SUCCESS(status))
        {
            outLog("������־�ļ�ʧ��!");
            return status;
        }

        boLogInit = true;

        // ��¼��־��Ϣ
        //status = Common::LogDriverMessage(L"��¼��־��Ϣ Driver loaded."); // ������־��Ϣ
        //if (!NT_SUCCESS(status))
        //{
        //    outLog(("��¼��־��Ϣʧ�ܣ�\n"));
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

        // �����߳�
        status = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS, &objectAttributes, NULL, NULL, StartRoutine, StartContext);
        if (!NT_SUCCESS(status))
        {
            // ��������
            return status;
        }

        // ��ȡ�̶߳���ָ��
        //status = ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, (PVOID*)Thread, NULL);
        //if (!NT_SUCCESS(status))
        //{
        //    // ��������
        //    ZwClose(hThread);
        //    return status;
        //}

        // �ر��߳̾��
        ZwClose(hThread);

        return STATUS_SUCCESS;
    }

    //NTSTATUS CreateKernelThread(PKSTART_ROUTINE StartRoutine, PTHREAD_DATA threadData, PETHREAD* Thread)
    //{
    //    // �����߳�
    //    return CreateInternalThread(StartRoutine, threadData, Thread);
    //}

    VOID KernelSleep(UINT32 milliseconds)
    {
        LARGE_INTEGER delay;
        delay.QuadPart = -((LONGLONG)milliseconds * 10 * 1000);  // ת��Ϊ100���뵥λ

        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }

    VOID RemovePath(WCHAR* fullPath)
    {
        WCHAR* lastSlash = wcsrchr(fullPath, L'\\');  // ���ַ����в������һ��Ŀ¼�ָ��� '\'

        if (lastSlash != NULL)
        {
            WCHAR* fileName = lastSlash + 1;  // ����Ŀ¼�ָ���
            wcscpy_s(fullPath, wcslen(fileName) + 1, fileName);
        }
    }
}
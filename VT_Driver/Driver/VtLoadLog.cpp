#include "DriverInternal.h"

namespace
{
    constexpr size_t vt_load_message_max = 512;
    constexpr size_t vt_load_line_max = 768;
    constexpr wchar_t vt_load_log_path[] = L"\\??\\C:\\VTLoad.log";

    void AppendChar(char* buffer, size_t buffer_size, size_t& offset, char value)
    {
        if (buffer == nullptr || buffer_size == 0 || offset + 1 >= buffer_size)
        {
            return;
        }

        buffer[offset++] = value;
        buffer[offset] = '\0';
    }

    void AppendString(char* buffer, size_t buffer_size, size_t& offset, const char* value)
    {
        if (value == nullptr)
        {
            value = "(null)";
        }

        while (*value != '\0')
        {
            AppendChar(buffer, buffer_size, offset, *value++);
        }
    }

    void AppendUnsigned(char* buffer, size_t buffer_size, size_t& offset, unsigned __int64 value, unsigned int base, bool uppercase)
    {
        char temp[32] = {};
        size_t temp_offset = 0;
        const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

        if (base < 2 || base > 16)
        {
            return;
        }

        do
        {
            temp[temp_offset++] = digits[value % base];
            value /= base;
        } while (value != 0 && temp_offset < sizeof(temp));

        while (temp_offset != 0)
        {
            AppendChar(buffer, buffer_size, offset, temp[--temp_offset]);
        }
    }

    void AppendSigned(char* buffer, size_t buffer_size, size_t& offset, __int64 value)
    {
        if (value < 0)
        {
            AppendChar(buffer, buffer_size, offset, '-');
            AppendUnsigned(buffer, buffer_size, offset, static_cast<unsigned __int64>(-value), 10, false);
            return;
        }

        AppendUnsigned(buffer, buffer_size, offset, static_cast<unsigned __int64>(value), 10, false);
    }

    const char* SkipIntegerWidth(const char* format, bool& long_long_width)
    {
        long_long_width = false;
        if (format[0] == 'l' && format[1] == 'l')
        {
            long_long_width = true;
            return format + 2;
        }

        if (format[0] == 'I' && format[1] == '6' && format[2] == '4')
        {
            long_long_width = true;
            return format + 3;
        }

        return format;
    }

    void FormatVtLoadMessage(char* buffer, size_t buffer_size, const char* format, va_list args)
    {
        if (buffer == nullptr || buffer_size == 0)
        {
            return;
        }

        buffer[0] = '\0';
        size_t offset = 0;

        for (const char* cursor = format; cursor != nullptr && *cursor != '\0'; ++cursor)
        {
            if (*cursor != '%')
            {
                AppendChar(buffer, buffer_size, offset, *cursor);
                continue;
            }

            ++cursor;
            if (*cursor == '\0')
            {
                break;
            }

            if (*cursor == '%')
            {
                AppendChar(buffer, buffer_size, offset, '%');
                continue;
            }

            bool long_long_width = false;
            cursor = SkipIntegerWidth(cursor, long_long_width);

            switch (*cursor)
            {
            case 'u':
                AppendUnsigned(buffer, buffer_size, offset,
                    long_long_width ? va_arg(args, unsigned __int64) : va_arg(args, unsigned int),
                    10,
                    false); // 参数4：是否立即注入=false（按原逻辑交由 VM-entry 注入）
                break;
            case 'd':
            case 'i':
                AppendSigned(buffer, buffer_size, offset,
                    long_long_width ? va_arg(args, __int64) : va_arg(args, int));
                break;
            case 'x':
                AppendUnsigned(buffer, buffer_size, offset,
                    long_long_width ? va_arg(args, unsigned __int64) : va_arg(args, unsigned int),
                    16,
                    false); // 参数4：是否立即注入=false（按原逻辑交由 VM-entry 注入）
                break;
            case 'X':
                AppendUnsigned(buffer, buffer_size, offset,
                    long_long_width ? va_arg(args, unsigned __int64) : va_arg(args, unsigned int),
                    16,
                    true);
                break;
            case 'p':
                AppendString(buffer, buffer_size, offset, "0x");
                AppendUnsigned(buffer, buffer_size, offset,
                    reinterpret_cast<unsigned __int64>(va_arg(args, void*)),
                    16,
                    true);
                break;
            case 's':
                AppendString(buffer, buffer_size, offset, va_arg(args, const char*));
                break;
            case 'c':
                AppendChar(buffer, buffer_size, offset, static_cast<char>(va_arg(args, int)));
                break;
            default:
                AppendChar(buffer, buffer_size, offset, '%');
                AppendChar(buffer, buffer_size, offset, *cursor);
                break;
            }
        }
    }

    size_t StringLength(const char* value)
    {
        if (value == nullptr)
        {
            return 0;
        }

        size_t length = 0;
        while (value[length] != '\0')
        {
            ++length;
        }

        return length;
    }

    NTSTATUS AppendVtLoadLogLine(const char* line, ULONG line_length)
    {
        if (line == nullptr || line_length == 0)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        {
            return STATUS_UNSUCCESSFUL; // 返回失败状态
        }

        UNICODE_STRING file_name;
        RtlInitUnicodeString(&file_name, vt_load_log_path);

        OBJECT_ATTRIBUTES object_attributes;
        InitializeObjectAttributes(
            &object_attributes,
            &file_name,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            nullptr,
            nullptr);

        IO_STATUS_BLOCK io_status = {};
        HANDLE file_handle = nullptr;
        NTSTATUS status = ZwCreateFile(
            &file_handle,
            FILE_APPEND_DATA | SYNCHRONIZE,
            &object_attributes,
            &io_status,
            nullptr,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ,
            FILE_OPEN_IF,
            FILE_SYNCHRONOUS_IO_NONALERT,
            nullptr,
            0);

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        status = ZwWriteFile(
            file_handle,
            nullptr,
            nullptr,
            nullptr,
            &io_status,
            const_cast<char*>(line),
            line_length,
            nullptr,
            nullptr);

        ZwClose(file_handle);
        return status;
    }
}

void VtLoadWriteLog(_In_z_ _Printf_format_string_ const char* format, ...)
{
    if (format == nullptr)
    {
        return;
    }

    char message[vt_load_message_max] = {};
    va_list args;
    va_start(args, format);
    FormatVtLoadMessage(message, sizeof(message), format, args);
    va_end(args);

    LARGE_INTEGER system_time = {};
    LARGE_INTEGER local_time = {};
    TIME_FIELDS time_fields = {};
    KeQuerySystemTime(&system_time);
    ExSystemTimeToLocalTime(&system_time, &local_time);
    RtlTimeToTimeFields(&local_time, &time_fields);

    char line[vt_load_line_max] = {};
    size_t offset = 0;
    AppendChar(line, sizeof(line), offset, '[');
    AppendUnsigned(line, sizeof(line), offset, time_fields.Year, 10, false);
    AppendChar(line, sizeof(line), offset, '-');
    if (time_fields.Month < 10) AppendChar(line, sizeof(line), offset, '0');
    AppendUnsigned(line, sizeof(line), offset, time_fields.Month, 10, false);
    AppendChar(line, sizeof(line), offset, '-');
    if (time_fields.Day < 10) AppendChar(line, sizeof(line), offset, '0');
    AppendUnsigned(line, sizeof(line), offset, time_fields.Day, 10, false);
    AppendChar(line, sizeof(line), offset, ' ');
    if (time_fields.Hour < 10) AppendChar(line, sizeof(line), offset, '0');
    AppendUnsigned(line, sizeof(line), offset, time_fields.Hour, 10, false);
    AppendChar(line, sizeof(line), offset, ':');
    if (time_fields.Minute < 10) AppendChar(line, sizeof(line), offset, '0');
    AppendUnsigned(line, sizeof(line), offset, time_fields.Minute, 10, false);
    AppendChar(line, sizeof(line), offset, ':');
    if (time_fields.Second < 10) AppendChar(line, sizeof(line), offset, '0');
    AppendUnsigned(line, sizeof(line), offset, time_fields.Second, 10, false);
    AppendChar(line, sizeof(line), offset, '.');
    if (time_fields.Milliseconds < 100) AppendChar(line, sizeof(line), offset, '0');
    if (time_fields.Milliseconds < 10) AppendChar(line, sizeof(line), offset, '0');
    AppendUnsigned(line, sizeof(line), offset, time_fields.Milliseconds, 10, false);
    AppendString(line, sizeof(line), offset, "][处理器:");
    AppendUnsigned(line, sizeof(line), offset, KeGetCurrentProcessorIndex(), 10, false);
    AppendString(line, sizeof(line), offset, "] 【VT加载】 ");
    AppendString(line, sizeof(line), offset, message);
    AppendString(line, sizeof(line), offset, "\r\n");

    size_t line_length = StringLength(line);
    if (line_length == 0 || line_length > MAXULONG)
    {
        return;
    }

    AppendVtLoadLogLine(line, static_cast<ULONG>(line_length));
}

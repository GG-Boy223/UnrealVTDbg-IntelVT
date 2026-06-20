#include "SerialPortInternal.h"

#include "../Spinlock/spinlock.h"

volatile long sendstring_lock = 0;

void tfp_format(void* putp, putcf putf, const char* fmt, va_list va)
{
    SerialPortInternal::Format(putp, putf, fmt, va);
}

int tfp_vsnprintf(char* str, size_t size, const char* format, va_list ap)
{
    return SerialPortInternal::Vsnprintf(str, size, format, ap);
}

UCHAR inportb(_In_ USHORT Port)
{
    return SerialPortInternal::InPortByte(Port);
}

void outportb(_In_ USHORT Port, _In_ UCHAR Data)
{
    SerialPortInternal::OutPortByte(Port, Data);
}

void sendchar2(char c)
{
    SerialPortInternal::SendChar(c);
}

void Log(const char* format, ...)
{
    int sl = 0;

    va_list args;
    va_start(args, format);
    char message[1024] = { 0 };
    sl = tfp_vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    spinlock::lock(&sendstring_lock);

    if (sl > 0)
    {
        for (int i = 0; i < sl; i++)
        {
            sendchar2(message[i]);
        }
    }

    spinlock::unlock(&sendstring_lock);
}

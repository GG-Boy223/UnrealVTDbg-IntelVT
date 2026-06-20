#pragma once

#include <ntifs.h>
#include <stdarg.h>

#include "SerialPort.h"

typedef void (*putcf)(void*, char);

void tfp_format(void* putp, putcf putf, const char* fmt, va_list va);
int tfp_vsnprintf(char* str, size_t size, const char* format, va_list ap);
UCHAR inportb(_In_ USHORT Port);
void outportb(_In_ USHORT Port, _In_ UCHAR Data);
void sendchar2(char c);

namespace SerialPortInternal
{
    struct FormatParam
    {
        char lz : 1;
        char alt : 1;
        char uc : 1;
        char align_left : 1;
        unsigned int width;
        char sign;
        unsigned int base;
        char* bf;
    };

    struct VsnprintfPutcfData
    {
        size_t dest_capacity;
        char* dest;
        size_t num_chars;
    };

#ifdef PRINTF_LONG_LONG_SUPPORT
    void UnsignedLongLongToAscii(unsigned long long int num, FormatParam* p);
    void LongLongToAscii(long long int num, FormatParam* p);
#endif

#ifdef PRINTF_LONG_SUPPORT
    void UnsignedLongToAscii(unsigned long int num, FormatParam* p);
    void LongToAscii(long num, FormatParam* p);
#endif

    void UnsignedIntToAscii(unsigned int num, FormatParam* p);
    void IntToAscii(int num, FormatParam* p);
    char ReadUnsigned(char ch, const char** src, int base, unsigned int* nump);
    void PutCharsWithWidth(void* putp, putcf putf, FormatParam* p);
    void Format(void* putp, putcf putf, const char* fmt, va_list va);
    int Vsnprintf(char* str, size_t size, const char* format, va_list ap);
    UCHAR InPortByte(_In_ USHORT Port);
    void OutPortByte(_In_ USHORT Port, _In_ UCHAR Data);
    void SendChar(char c);
}

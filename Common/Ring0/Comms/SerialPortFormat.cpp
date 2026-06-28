#include "SerialPortInternal.h"

namespace SerialPortInternal
{
    void Format(void* putp, putcf putf, const char* fmt, va_list va)
    {
        FormatParam p;
#ifdef PRINTF_LONG_SUPPORT
        char bf[23];
// char bf[23];  /* long = 64b on some architectures */
#else
        char bf[12];
#endif
        char ch = 0;
        p.bf = bf;

        while ((ch = *(fmt++)))
        {
            if (ch != '%')
            {
                putf(putp, ch);
            }
            else
            {
#ifdef PRINTF_LONG_SUPPORT
                char lng = 0;
// char lng = 0;  /* 1 for long, 2 for long long */
#endif
                // /* Init parameter struct */
                p.lz = 0;
                p.alt = 0;
                p.width = 0;
                p.align_left = 0;
                p.sign = 0;

                while ((ch = *(fmt++)))
                {
                    switch (ch)
                    {
                    case '-':
                        p.align_left = 1;
                        continue;
                    case '0':
                        p.lz = 1;
                        continue;
                    case '#':
                        p.alt = 1;
                        continue;
                    default:
                        break;
                    }
                    break;
                }

                if (ch >= '0' && ch <= '9')
                {
                    ch = ReadUnsigned(ch, &fmt, 10, &(p.width));
                }

                if (ch == '.')
                {
                    p.lz = 1;
                    do
                    {
                        ch = *(fmt++);
                    } while ((ch >= '0') && (ch <= '9'));
                }

#ifdef PRINTF_SIZE_T_SUPPORT
# ifdef PRINTF_LONG_SUPPORT
                if (ch == 'z')
                {
                    ch = *(fmt++);
                    if (sizeof(size_t) == sizeof(unsigned long int))
                    {
                        lng = 1;
                    }
#  ifdef PRINTF_LONG_LONG_SUPPORT
                    else if (sizeof(size_t) == sizeof(unsigned long long int))
                    {
                        lng = 2;
                    }
#  endif
                }
                else
# endif
#endif

#ifdef PRINTF_LONG_SUPPORT
                    if (ch == 'l')
                    {
                        ch = *(fmt++);
                        lng = 1;
#ifdef PRINTF_LONG_LONG_SUPPORT
                        if (ch == 'l')
                        {
                            ch = *(fmt++);
                            lng = 2;
                        }
#endif
                    }
#endif
                switch (ch)
                {
                case 0: // 目标 CR0
                    goto abort;
                case 'u':
                    p.base = 10;
#ifdef PRINTF_LONG_SUPPORT
#ifdef PRINTF_LONG_LONG_SUPPORT
                    if (2 == lng)
                    {
                        UnsignedLongLongToAscii(va_arg(va, unsigned long long int), &p);
                    }
                    else
#endif
                        if (1 == lng)
                        {
                            UnsignedLongToAscii(va_arg(va, unsigned long int), &p);
                        }
                        else
#endif
                        {
                            UnsignedIntToAscii(va_arg(va, unsigned int), &p);
                        }
                    PutCharsWithWidth(putp, putf, &p);
                    break;
                case 'd':
                case 'i':
                    p.base = 10;
#ifdef PRINTF_LONG_SUPPORT
#ifdef PRINTF_LONG_LONG_SUPPORT
                    if (2 == lng)
                    {
                        LongLongToAscii(va_arg(va, long long int), &p);
                    }
                    else
#endif
                        if (1 == lng)
                        {
                            LongToAscii(va_arg(va, long int), &p);
                        }
                        else
#endif
                        {
                            IntToAscii(va_arg(va, int), &p);
                        }
                    PutCharsWithWidth(putp, putf, &p);
                    break;
#ifdef SIZEOF_POINTER
                case 'p':
                    p.alt = 1;
# if defined(SIZEOF_INT) && SIZEOF_POINTER <= SIZEOF_INT
                    lng = 0;
# elif defined(SIZEOF_LONG) && SIZEOF_POINTER <= SIZEOF_LONG
                    lng = 1;
# elif defined(SIZEOF_LONG_LONG) && SIZEOF_POINTER <= SIZEOF_LONG_LONG
                    lng = 2;
# endif
#endif
                case 'x':
                case 'X':
                    p.base = 16;
                    p.uc = (ch == 'X') ? 1 : 0;
#ifdef PRINTF_LONG_SUPPORT
#ifdef PRINTF_LONG_LONG_SUPPORT
                    if (2 == lng)
                    {
                        UnsignedLongLongToAscii(va_arg(va, unsigned long long int), &p);
                    }
                    else
#endif
                        if (1 == lng)
                        {
                            UnsignedLongToAscii(va_arg(va, unsigned long int), &p);
                        }
                        else
#endif
                        {
                            UnsignedIntToAscii(va_arg(va, unsigned int), &p);
                        }
                    PutCharsWithWidth(putp, putf, &p);
                    break;
                case 'o':
                    p.base = 8;
                    UnsignedIntToAscii(va_arg(va, unsigned int), &p);
                    PutCharsWithWidth(putp, putf, &p);
                    break;
                case 'c':
                    putf(putp, (char)(va_arg(va, int)));
                    break;
                case 's':
                    p.bf = va_arg(va, char*);
                    PutCharsWithWidth(putp, putf, &p);
                    p.bf = bf;
                    break;
                case '%':
                    putf(putp, ch);
                default:
                    break;
                }
            }
        }
abort:;
    }

    static void VsnprintfPutChar(void* p, char c)
    {
        VsnprintfPutcfData* data = (VsnprintfPutcfData*)p;
        if (data->num_chars < data->dest_capacity)
        {
            data->dest[data->num_chars] = c;
        }

        data->num_chars++;
    }

    int Vsnprintf(char* str, size_t size, const char* format, va_list ap)
    {
        VsnprintfPutcfData data;

        if (size < 1)
        {
            return 0;
        }

        data.dest = str;
        data.dest_capacity = size - 1;
        data.num_chars = 0;
        Format(&data, VsnprintfPutChar, format, ap);

        if (data.num_chars < data.dest_capacity)
        {
            data.dest[data.num_chars] = '\0';
        }
        else
        {
            data.dest[data.dest_capacity] = '\0';
        }

        return data.num_chars;
    }
}

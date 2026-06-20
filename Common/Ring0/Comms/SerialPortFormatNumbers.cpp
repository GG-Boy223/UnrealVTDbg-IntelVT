#include "SerialPortInternal.h"

namespace SerialPortInternal
{
#ifdef PRINTF_LONG_LONG_SUPPORT
    void UnsignedLongLongToAscii(unsigned long long int num, FormatParam* p)
    {
        int n = 0;
        unsigned long long int d = 1;
        char* bf = p->bf;

        while (num / d >= p->base)
        {
            d *= p->base;
        }

        while (d != 0)
        {
            int dgt = static_cast<int>(num / d);
            num %= d;
            d /= p->base;
            if (n || dgt > 0 || d == 0)
            {
                *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
                ++n;
            }
        }

        *bf = 0;
    }

    void LongLongToAscii(long long int num, FormatParam* p)
    {
        if (num < 0)
        {
            num = -num;
            p->sign = '-';
        }

        UnsignedLongLongToAscii(num, p);
    }
#endif

#ifdef PRINTF_LONG_SUPPORT
    void UnsignedLongToAscii(unsigned long int num, FormatParam* p)
    {
        int n = 0;
        unsigned long int d = 1;
        char* bf = p->bf;

        while (num / d >= p->base)
        {
            d *= p->base;
        }

        while (d != 0)
        {
            int dgt = num / d;
            num %= d;
            d /= p->base;
            if (n || dgt > 0 || d == 0)
            {
                *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
                ++n;
            }
        }

        *bf = 0;
    }

    void LongToAscii(long num, FormatParam* p)
    {
        if (num < 0)
        {
            num = -num;
            p->sign = '-';
        }

        UnsignedLongToAscii(num, p);
    }
#endif

    void UnsignedIntToAscii(unsigned int num, FormatParam* p)
    {
        int n = 0;
        unsigned int d = 1;
        char* bf = p->bf;

        while (num / d >= p->base)
        {
            d *= p->base;
        }

        while (d != 0)
        {
            int dgt = num / d;
            num %= d;
            d /= p->base;
            if (n || dgt > 0 || d == 0)
            {
                *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
                ++n;
            }
        }

        *bf = 0;
    }

    void IntToAscii(int num, FormatParam* p)
    {
        if (num < 0)
        {
            num = -num;
            p->sign = '-';
        }

        UnsignedIntToAscii(num, p);
    }

    static int AsciiToDigit(char ch)
    {
        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            return ch - 'a' + 10;
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            return ch - 'A' + 10;
        }
        else
        {
            return -1;
        }
    }

    char ReadUnsigned(char ch, const char** src, int base, unsigned int* nump)
    {
        const char* p = *src;
        unsigned int num = 0;
        int digit = 0;

        while ((digit = AsciiToDigit(ch)) >= 0)
        {
            if (digit > base)
            {
                break;
            }

            num = num * base + digit;
            ch = *p++;
        }

        *src = p;
        *nump = num;
        return ch;
    }

    void PutCharsWithWidth(void* putp, putcf putf, FormatParam* p)
    {
        char ch = 0;
        int n = p->width;
        char* bf = p->bf;

        while (*bf++ && n > 0)
        {
            n--;
        }

        if (p->sign)
        {
            n--;
        }

        if (p->alt && p->base == 16)
        {
            n -= 2;
        }
        else if (p->alt && p->base == 8)
        {
            n--;
        }

        if (!p->lz && !p->align_left)
        {
            while (n-- > 0)
            {
                putf(putp, ' ');
            }
        }

        if (p->sign)
        {
            putf(putp, p->sign);
        }

        if (p->alt && p->base == 16)
        {
            putf(putp, '0');
            putf(putp, (p->uc ? 'X' : 'x'));
        }
        else if (p->alt && p->base == 8)
        {
            putf(putp, '0');
        }

        if (p->lz)
        {
            while (n-- > 0)
            {
                putf(putp, '0');
            }
        }

        bf = p->bf;
        while ((ch = *bf++))
        {
            putf(putp, ch);
        }

        if (!p->lz && p->align_left)
        {
            while (n-- > 0)
            {
                putf(putp, ' ');
            }
        }
    }
}

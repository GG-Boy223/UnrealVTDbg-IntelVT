#include <ntifs.h>

#include "BlowfishInternal.h"

#pragma warning(push)
#pragma warning(disable: 4996)
PVOID BlowfishAllocate(SIZE_T size)
{
    return ExAllocatePoolWithTag(NonPagedPool, size, 'Blow');
}
#pragma warning(pop)

void BlowfishFree(PVOID ptr)
{
    ExFreePoolWithTag(ptr, 'Blow');
}

namespace
{
    void Char2Hex(unsigned char ch, char* szHex)
    {
        ULONG i;
        unsigned char byte[2];
        byte[0] = ch / 16;
        byte[1] = ch % 16;
        for (i = 0; i < 2; i++)
        {
            if (byte[i] >= 0 && byte[i] <= 9)
            {
                szHex[i] = '0' + byte[i];
            }
            else
            {
                szHex[i] = 'A' + byte[i] - 10;
            }
        }
        szHex[2] = 0;
    }

    void Hex2Char(char* szHex, unsigned char* rch)
    {
        ULONG i;
        for (i = 0; i < 2; i++)
        {
            if (*(szHex + i) >= '0' && *(szHex + i) <= '9')
            {
                *rch = (*rch << 4) + (*(szHex + i) - '0');
            }
            // *rch = (*rch << 4) + (*(szHex + i) - '0');
            else if (*(szHex + i) >= 'A' && *(szHex + i) <= 'F')
            {
                *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
            }
            else
            {
                break;
            }
        }
    }
}

// /*Function to convert string of unsigned chars to string of chars*/
VOID CharStr2HexStr(unsigned char* pucCharStr, ULONG iSize, unsigned char* pszHexStr)
{
    ULONG i;
    char szHex[3];
    pszHexStr[0] = 0;
    for (i = 0; i < iSize; i++)
    {
        Char2Hex(pucCharStr[i], szHex);
        strcat((char*)pszHexStr, szHex);
    }
}

// /*Function to convert string of chars to string of unsigned chars*/
VOID HexStr2CharStr(unsigned char* pszHexStr, ULONG iSize, unsigned char* pucCharStr)
{
    ULONG i;
    unsigned char ch;
    if (iSize % 2 != 0)
    {
        //是我们自己的调试器要创建线程，则直接返回不走线程回调
        return;
    }

    for (i = 0; i < iSize / 2; i++)
    {
        Hex2Char((char*)pszHexStr + 2 * i, &ch);
        pucCharStr[i] = ch;
    }
}

//字符流转字节流
void StrToHex(const char* source, unsigned char* dest, int length)
{
    short i;
    unsigned char highByte, lowByte;

    for (i = 0; i < length; i += 2)
    {
        highByte = (unsigned char)(toupper(source[i]));
        lowByte = (unsigned char)(toupper(source[i + 1]));

        if (highByte > 0x39)
        {
            highByte -= 0x37;
        }
        else
        {
            highByte -= 0x30;
        }

        if (lowByte > 0x39)
        {
            lowByte -= 0x37;
        }
        else
        {
            lowByte -= 0x30;
        }

        dest[i / 2] = (highByte << 4) | lowByte;
    }
}

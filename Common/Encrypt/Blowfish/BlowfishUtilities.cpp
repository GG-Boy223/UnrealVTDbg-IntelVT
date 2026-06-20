#include <cctype>
#include <cstdio>
#include <cstring>
#include <Windows.h>

#include "BlowfishInternal.h"

namespace
{
    void Char2Hex(unsigned char ch, char* szHex)
    {
        int i;
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
        int i;
        for (i = 0; i < 2; i++)
        {
            if (*(szHex + i) >= '0' && *(szHex + i) <= '9')
            {
                *rch = (*rch << 4) + (*(szHex + i) - '0');
            }
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

void CharStr2HexStr(unsigned char* pucCharStr, int iSize, unsigned char* pszHexStr)
{
    int i;
    char szHex[3];
    pszHexStr[0] = 0;
    for (i = 0; i < iSize; i++)
    {
        Char2Hex(pucCharStr[i], szHex);
        strcat((char*)pszHexStr, szHex);
    }
}

void HexStr2CharStr(unsigned char* pszHexStr, int iSize, unsigned char* pucCharStr)
{
    int i;
    unsigned char ch;
    if (iSize % 2 != 0)
    {
        return;
    }

    for (i = 0; i < iSize / 2; i++)
    {
        Hex2Char((char*)pszHexStr + 2 * i, &ch);
        pucCharStr[i] = ch;
    }
}

void HexToStr(const char* sSrc, char* sDest, int length)
{
    int i;
    char szTmp[3];

    for (i = 0; i < length; i++)
    {
        sprintf(szTmp, "%02X", (unsigned char)sSrc[i]);
        memcpy(&sDest[i * 2], szTmp, 2);
    }
    return;
}

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

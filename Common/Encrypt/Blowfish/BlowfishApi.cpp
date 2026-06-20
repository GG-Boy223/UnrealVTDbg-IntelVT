#include <cstdlib>
#include <cstring>
#include <string>
#include <Windows.h>

#include "../../VMProtect/vmp.h"
#include "BlowfishInternal.h"

int BlowFishInit(Blowfish* blowfish, unsigned char* ucKey, size_t keysize)
{
    unsigned int i, j, k, n;
    unsigned char aucLocalKey[MAX_KEY_SIZE];
    const unsigned char* p = aucLocalKey;
    unsigned int x = 0;
    int iCount = 0;
    SBlock block = { 0, 0 };

    if (keysize < 1)
    {
        return -1;
    }

    if (keysize < 0 || keysize > MAX_KEY_SIZE)
    {
        return -1;
    }

    memcpy(aucLocalKey, ucKey, keysize);
    memcpy(blowfish->m_auiP, scm_auiInitP, sizeof(blowfish->m_auiP));
    memcpy(blowfish->m_auiS, scm_auiInitS, sizeof(blowfish->m_auiS));

    for (i = 0; i < MAX_PBLOCK_SIZE; i++)
    {
        x = 0;
        for (n = 4; n--; )
        {
            x <<= 8;
            x |= *(p++);
            iCount++;
            if (iCount == keysize)
            {
                iCount = 0;
                p = aucLocalKey;
            }
        }
        blowfish->m_auiP[i] ^= x;
    }

    for (i = 0; i < MAX_PBLOCK_SIZE; )
    {
        EncryptBlock(blowfish, &block), blowfish->m_auiP[i++] = block.m_uil, blowfish->m_auiP[i++] = block.m_uir;
    }
    for (j = 0; j < MAX_SBLOCK_XSIZE; j++)
    {
        for (k = 0; k < MAX_SBLOCK_YSIZE; )
        {
            EncryptBlock(blowfish, &block), blowfish->m_auiS[j][k++] = block.m_uil, blowfish->m_auiS[j][k++] = block.m_uir;
        }
    }

    return 1;
}

int Encrypt(Blowfish* blowfish, const unsigned char* in, size_t siz_i, unsigned char* out, size_t siz_o, int iMode)
{
    unsigned char* byte_in;
    unsigned char* byte_out;
    int nLen, nRet;

    if (siz_i % 8 == 0)
    {
        nLen = static_cast<int>(siz_i);
    }
    else
    {
        nLen = static_cast<int>(siz_i + 8 - siz_i % 8);
    }

    if (siz_o < nLen * 2)
    {
        return -1;
    }

    byte_in = (unsigned char*)malloc(sizeof(unsigned char) * nLen + 1);
    byte_out = (unsigned char*)malloc(sizeof(unsigned char) * nLen + 1);
    memset(byte_in, 0x00, sizeof(unsigned char) * nLen + 1);
    memset(byte_out, 0x00, sizeof(unsigned char) * nLen + 1);
    memcpy(byte_in, in, siz_i);
    nRet = EncryptByte(blowfish, byte_in, nLen, byte_out, iMode);
    CharStr2HexStr(byte_out, nLen, out);
    free(byte_in);
    free(byte_out);
    return nRet;
}

int Decrypt(Blowfish* blowfish, const unsigned char* in, size_t siz_i, unsigned char* out, size_t siz_o, int iMode)
{
    unsigned char* byte_in;
    int nLen, nRet;

    if (siz_i % 2 != 0)
    {
        return -1;
    }

    nLen = static_cast<int>(siz_i / 2);
    if (siz_o < nLen)
    {
        return -1;
    }

    byte_in = (unsigned char*)malloc(sizeof(unsigned char) * nLen + 1);
    memset(byte_in, 0x00, sizeof(unsigned char) * nLen + 1);
    HexStr2CharStr((unsigned char*)in, static_cast<int>(siz_i), byte_in);
    nRet = DecryptByte(blowfish, byte_in, nLen, out, iMode);
    free(byte_in);
    return nRet;
}

std::string EncryptData(const char* pAddr, SIZE_T size, const char* key)
{
    VMProtectBeginVirtualization("VMP");
    Blowfish oBlowFish;
    unsigned char aucKey[17];
    BYTE* aucPlainText;
    BYTE* aucCipherText;
    SIZE_T aucCipherTextLen;
    aucKey[16] = 0;
    int nLen;

    strcpy((char*)aucKey, key);

    if (size % 8 == 0)
    {
        nLen = static_cast<int>(size);
    }
    else
    {
        nLen = static_cast<int>(size + 8 - size % 8);
    }

    aucPlainText = (BYTE*)pAddr;
    aucCipherTextLen = nLen * 2 + 1;
    aucCipherText = new BYTE[aucCipherTextLen];
    memset(aucCipherText, 0, aucCipherTextLen);
    BlowFishInit(&oBlowFish, aucKey, 16);
    Encrypt(&oBlowFish, aucPlainText, nLen, aucCipherText, aucCipherTextLen, ECB);

    std::string strCipher((char*)aucCipherText);

    delete[] aucCipherText;
    VMProtectEnd();
    return strCipher;
}

std::string DecryptData(const char* pInAddr, const char* key)
{
    VMProtectBeginVirtualization("VMP");
    Blowfish oBlowFish;
    unsigned char aucKey[17];
    UCHAR* aucPlainText;
    size_t aucPlainTextLen = 0;
    size_t aucCipherTextLen = 0;

    strcpy((char*)aucKey, key);
    aucCipherTextLen = strlen(pInAddr);
    aucPlainTextLen = aucCipherTextLen / 2 + 1;
    aucPlainText = new BYTE[aucPlainTextLen];
    memset(aucPlainText, 0, aucPlainTextLen);

    BlowFishInit(&oBlowFish, aucKey, 16);
    Decrypt(&oBlowFish, (const unsigned char*)pInAddr, aucCipherTextLen, aucPlainText, aucPlainTextLen, ECB);

    std::string strPlain((char*)aucPlainText);

    delete[] aucPlainText;
    return strPlain;
    VMProtectEnd();
}

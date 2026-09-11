#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <Windows.h>

#include "../../VMProtect/vmp.h"
#include "BlowfishInternal.h"

//输入key变换s盒 p盒
// /****************************************************************************************/
// /*Constructor - Initialize the P and S boxes for a given Key*/
int BlowFishInit(Blowfish* blowfish, unsigned char* ucKey, size_t keysize)
{
    unsigned int i, j, k, n;
    unsigned char aucLocalKey[MAX_KEY_SIZE];
    const unsigned char* p = aucLocalKey;
    unsigned int x = 0;
    int iCount = 0;
    SBlock block = { 0, 0 }; //all-zero block

    if (keysize < 1)
    {
        return -1;
    }

    if (keysize < 0 || keysize > MAX_KEY_SIZE)
    {
        return -1;
    }

    memcpy(aucLocalKey, ucKey, keysize);
    //Blowfish的反射初始化。使用PI从P和S盒生成子密钥
    memcpy(blowfish->m_auiP, scm_auiInitP, sizeof(blowfish->m_auiP));
    memcpy(blowfish->m_auiS, scm_auiInitS, sizeof(blowfish->m_auiS));

    // Repeatedly cycle through the key bits until the entire P array has been XORed with key bits*/
    //加载具有密钥字节的P盒在密钥位中重复循环，直到整个P阵列与密钥位异或
    // /*Load P boxes with key bytes
    for (i = 0; i < MAX_PBLOCK_SIZE; i++)
    {
        x = 0;
        for (n = 4; n--; )
        {
            // /*int iVal = (int)(*p);*/
            x <<= 8;
            x |= *(p++);
            iCount++;
            if (iCount == keysize)
            {
                //已使用所有字节，因此回收字节
                // /*All bytes used, so recycle bytes*/
                iCount = 0;
                p = aucLocalKey;
            }
        }
        blowfish->m_auiP[i] ^= x;
    }

    // /*Reflect P and S boxes through the evolving Blowfish*/
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

    return 1; // Wired Ethernet second.
}

// Returns false if n is multiple of 8*/
// /*Encrypt from Input Buffer to Output Buffer
int Encrypt(Blowfish* blowfish, const unsigned char* in, size_t siz_i, unsigned char* out, size_t siz_o, int iMode)
{
    unsigned char* byte_in;
    unsigned char* byte_out;
    int nLen, nRet;

    if (siz_i > static_cast<size_t>((std::numeric_limits<int>::max)() - 7))
    {
        return -1;
    }

    if (siz_i % 8 == 0)
    {
        nLen = static_cast<int>(siz_i);
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        nLen = static_cast<int>(siz_i + 8 - siz_i % 8);
    }

    if (siz_o < static_cast<size_t>(nLen) * 2)
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

// Returns false if n is multiple of 8*/
// /*Decrypt from Input Buffer to Output Buffer
int Decrypt(Blowfish* blowfish, const unsigned char* in, size_t siz_i, unsigned char* out, size_t siz_o, int iMode)
{
    unsigned char* byte_in;
    int nLen, nRet;

    if (siz_i % 2 != 0)
    {
        return -1;
    }

    if (siz_i / 2 > static_cast<size_t>((std::numeric_limits<int>::max)()))
    {
        return -1;
    }

    nLen = static_cast<int>(siz_i / 2);
    if (siz_o < static_cast<size_t>(nLen))
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

//加密函数
std::string EncryptData(const char* pAddr, SIZE_T size, const char* key)
{
    VMProtectBeginVirtualization("VMP");
    Blowfish oBlowFish;
    unsigned char aucKey[17]; //密钥
    BYTE* aucPlainText; //明文缓冲区
    BYTE* aucCipherText; //密文缓冲区  注意: 必须是明文缓冲区的2倍
    //SIZE_T aucPlainTextLen;
    SIZE_T aucCipherTextLen;
    aucKey[16] = 0;
    int nLen;

    //一组明文对应一组key
    strcpy((char*)aucKey, key);

    if (size % 8 == 0)
    {
        nLen = static_cast<int>(size);
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        nLen = static_cast<int>(size + 8 - size % 8);
    }

    aucPlainText = (BYTE*)pAddr;
    aucCipherTextLen = nLen * 2 + 1; //因为加密后密文要比明文长2倍
    aucCipherText = new BYTE[aucCipherTextLen];
    memset(aucCipherText, 0, aucCipherTextLen); //初始化密文缓冲区
    BlowFishInit(&oBlowFish, aucKey, 16);
    Encrypt(&oBlowFish, aucPlainText, nLen, aucCipherText, aucCipherTextLen, ECB);

    std::string strCipher((char*)aucCipherText);

    delete[] aucCipherText;
    VMProtectEnd();
    return strCipher;
}

//解密函数
std::string DecryptData(const char* pInAddr, const char* key)
{
    VMProtectBeginVirtualization("VMP");
    Blowfish oBlowFish;
    unsigned char aucKey[17]; //密钥
    UCHAR* aucPlainText; //明文
    size_t aucPlainTextLen = 0;
    size_t aucCipherTextLen = 0;

    //一组明文对应一组key
    strcpy((char*)aucKey, key);
    aucCipherTextLen = strlen(pInAddr);
    aucPlainTextLen = aucCipherTextLen / 2 + 1; //因为加密后密文要比明文长2倍
    aucPlainText = new BYTE[aucPlainTextLen]; //分配明文缓冲
    memset(aucPlainText, 0, aucPlainTextLen);

    BlowFishInit(&oBlowFish, aucKey, 16);
    Decrypt(&oBlowFish, (const unsigned char*)pInAddr, aucCipherTextLen, aucPlainText, aucPlainTextLen, ECB);

    std::string strPlain((char*)aucPlainText);

    delete[] aucPlainText;
    return strPlain;
    VMProtectEnd();
}

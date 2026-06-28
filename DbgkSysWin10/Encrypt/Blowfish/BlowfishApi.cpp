#include <ntifs.h>
#include <ntstrsafe.h>

#include "BlowfishInternal.h"

//输入key变换s盒 p盒
// /****************************************************************************************/
// /*Constructor - Initialize the P and S boxes for a given Key*/
ULONG BlowFishInit(Blowfish* blowfish, unsigned char* ucKey, size_t keysize)
{
    unsigned int i, j, k, n;
    unsigned char aucLocalKey[MAX_KEY_SIZE];
    const unsigned char* p = aucLocalKey;
    unsigned int x = 0;
    ULONG iCount = 0;
    SBlock block = { 0, 0 }; //all-zero block

    if (keysize < 1)
    {
        return -1;
    }

    if (keysize > MAX_KEY_SIZE)
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

    return 1;
}

// Returns false if n is multiple of 8*/
// /*Encrypt from Input Buffer to Output Buffer
ULONG Encrypt(Blowfish* blowfish, const unsigned char* in, size_t siz_i, unsigned char* out, size_t siz_o, ULONG iMode)
{
    unsigned char* byte_in;
    unsigned char* byte_out;
    ULONG nLen, nRet;

    if (siz_i % 8 == 0)
    {
        nLen = siz_i;
    }
    else
    {
        nLen = siz_i + 8 - siz_i % 8;
    }

    if (siz_o < nLen * 2)
    {
        return -1;
    }

    byte_in = (unsigned char*)BlowfishAllocate(sizeof(unsigned char) * nLen + 1);
    byte_out = (unsigned char*)BlowfishAllocate(sizeof(unsigned char) * nLen + 1);
    memset(byte_in, 0x00, sizeof(unsigned char) * nLen + 1);
    memset(byte_out, 0x00, sizeof(unsigned char) * nLen + 1);
    memcpy(byte_in, in, siz_i);
    nRet = EncryptByte(blowfish, byte_in, nLen, byte_out, iMode);
    CharStr2HexStr(byte_out, nLen, out);
    BlowfishFree(byte_in);
    BlowfishFree(byte_out);
    return nRet;
}

// Returns false if n is multiple of 8*/
// /*Decrypt from Input Buffer to Output Buffer
ULONG Decrypt(Blowfish* blowfish, const unsigned char* in, size_t siz_i, unsigned char* out, size_t siz_o, ULONG iMode)
{
    unsigned char* byte_in;
    ULONG nLen, nRet;

    if (siz_i % 2 != 0)
    {
        return -1;
    }

    nLen = siz_i / 2;
    if (siz_o < nLen)
    {
        return -1;
    }

    byte_in = (unsigned char*)BlowfishAllocate(sizeof(unsigned char) * nLen + 1);
    memset(byte_in, 0x00, sizeof(unsigned char) * nLen + 1);
    HexStr2CharStr((unsigned char*)in, siz_i, byte_in);
    nRet = DecryptByte(blowfish, byte_in, nLen, out, iMode);
    BlowfishFree(byte_in);
    return nRet;
}

VOID DecryptData(PVOID pInAddr, PVOID pOutAddr)
{
    Blowfish oBlowFish;
    unsigned char aucKey[17]; //密钥
    UCHAR* aucPlainText; //明文
    size_t aucPlainTextLen = 0;
    size_t aucCipherTextLen = 0;
    const char* key = KEY;

    //一组明文对应一组key
    RtlStringCbCopyA((NTSTRSAFE_PSTR)aucKey, sizeof(aucKey), key);
    RtlStringCbLengthA((STRSAFE_PCNZCH)pInAddr, NTSTRSAFE_MAX_CCH * sizeof(char), &aucCipherTextLen);
    aucPlainTextLen = aucCipherTextLen / 2 + 1;
    aucPlainText = (UCHAR*)BlowfishAllocate(aucPlainTextLen);
    memset(aucPlainText, 0, aucPlainTextLen);

    BlowFishInit(&oBlowFish, aucKey, 16);
    Decrypt(&oBlowFish, (const unsigned char*)pInAddr, aucCipherTextLen, aucPlainText, aucPlainTextLen, ECB);
    memcpy(pOutAddr, aucPlainText, (aucPlainTextLen - 1));
    BlowfishFree(aucPlainText);
}

VOID TestBlowfish()
{
    //TESTING Blowfish
    //ecb test data (taken from the DES validation tests)
    char szData[34][2][17] = {
        {"0000000000000000", "0000000000000000"},
        {"FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF"},
        {"3000000000000000", "1000000000000001"},
        {"1111111111111111", "1111111111111111"},
        {"0123456789ABCDEF", "1111111111111111"},
        {"1111111111111111", "0123456789ABCDEF"},
        {"0000000000000000", "0000000000000000"},
        {"FEDCBA9876543210", "0123456789ABCDEF"},
        {"7CA110454A1A6E57", "01A1D6D039776742"},
        {"0131D9619DC1376E", "5CD54CA83DEF57DA"},
        {"07A1133E4A0B2686", "0248D43806F67172"},
        {"3849674C2602319E", "51454B582DDF440A"},
        {"04B915BA43FEB5B6", "42FD443059577FA2"},
        {"0113B970FD34F2CE", "059B5E0851CF143A"},
        {"0170F175468FB5E6", "0756D8E0774761D2"},
        {"43297FAD38E373FE", "762514B829BF486A"},
        {"07A7137045DA2A16", "3BDD119049372802"},
        {"04689104C2FD3B2F", "26955F6835AF609A"},
        {"37D06BB516CB7546", "164D5E404F275232"},
        {"1F08260D1AC2465E", "6B056E18759F5CCA"},
        {"584023641ABA6176", "004BD6EF09176062"},
        {"025816164629B007", "480D39006EE762F2"},
        {"49793EBC79B3258F", "437540C8698F3CFA"},
        {"4FB05E1515AB73A7", "072D43A077075292"},
        {"49E95D6D4CA229BF", "02FE55778117F12A"},
        {"018310DC409B26D6", "1D9D5C5018F728C2"},
        {"1C587F1C13924FEF", "305532286D6F295A"},
        {"0101010101010101", "0123456789ABCDEF"},
        {"1F1F1F1F0E0E0E0E", "0123456789ABCDEF"},
        {"E0FEE0FEF1FEF1FE", "0123456789ABCDEF"},
        {"0000000000000000", "FFFFFFFFFFFFFFFF"},
        {"FFFFFFFFFFFFFFFF", "0000000000000000"},
        {"0123456789ABCDEF", "0000000000000000"},
        {"FEDCBA9876543210", "FFFFFFFFFFFFFFFF"}
    };
    unsigned char aucKey[17]; //密钥
    unsigned char aucPlainText[64]; //明文
    unsigned char aucCipherText[64]; //密文
    size_t aucPlainTextLen = 0;
    size_t aucCipherTextLen = 0;

    for (ULONG i = 0; i < 34; i++)
    {
        Blowfish oBlowFish;
        aucKey[16] = 0;
        memset(aucPlainText, 0x00, sizeof(aucPlainText)); //初始化明文缓冲区
        memset(aucCipherText, 0x00, sizeof(aucCipherText)); //初始化密文缓冲区

        //一组明文对应一组key
        RtlStringCbCopyA((NTSTRSAFE_PSTR)aucKey, sizeof(aucKey), szData[i][0]);
        RtlStringCbCopyA((NTSTRSAFE_PSTR)aucPlainText, sizeof(szData[i][1]), szData[i][1]);
        BlowFishInit(&oBlowFish, aucKey, 16);
        memset(aucCipherText, 0x00, sizeof(aucCipherText)); //初始化密文缓冲区
        RtlStringCbLengthA((STRSAFE_PCNZCH)aucPlainText, NTSTRSAFE_MAX_CCH * sizeof(char), &aucPlainTextLen);
        Encrypt(&oBlowFish, aucPlainText, aucPlainTextLen, aucCipherText, sizeof(aucCipherText), ECB);

        memset(aucPlainText, 0x00, sizeof(aucPlainText)); //初始化明文缓冲区
        RtlStringCbLengthA((STRSAFE_PCNZCH)aucCipherText, NTSTRSAFE_MAX_CCH * sizeof(char), &aucCipherTextLen);
        Decrypt(&oBlowFish, aucCipherText, aucCipherTextLen, aucPlainText, sizeof(aucPlainText), ECB);
        KdPrint(("[%s][%s]\n", aucCipherText, aucPlainText));
    }
}

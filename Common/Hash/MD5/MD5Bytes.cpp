#include <iostream>
#include <string>
#include <vector>
#include <Windows.h>
#include <wincrypt.h>
#include "MD5.h"

//字节流哈希摘要
std::string calculateMD5(const std::vector<unsigned char>& data)
{
    HCRYPTPROV hProv = NULL;
    HCRYPTHASH hHash = NULL;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        std::cerr << "CryptAcquireContext failed." << std::endl;
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
    {
        std::cerr << "CryptCreateHash failed." << std::endl;
        CryptReleaseContext(hProv, 0);
        return "";
    }

    for (size_t offset = 0; offset < data.size();)
    {
        const size_t remaining = data.size() - offset;
        const DWORD chunkSize = remaining > MAXDWORD ? MAXDWORD : static_cast<DWORD>(remaining);
        if (!CryptHashData(hHash, data.data() + offset, chunkSize, 0))
        {
            std::cerr << "CryptHashData failed." << std::endl;
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }
        offset += chunkSize;
    }

    DWORD cbHashSize = 0;
    DWORD dwCount = sizeof(DWORD);
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&cbHashSize), &dwCount, 0))
    {
        std::cerr << "CryptGetHashParam failed." << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::vector<unsigned char> hashData(cbHashSize);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hashData.data(), &cbHashSize, 0))
    {
        std::cerr << "CryptGetHashParam failed." << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::string md5Hash;
    for (const auto& byte : hashData)
    {
        char hex[3];
        sprintf_s(hex, "%02X", byte);
        md5Hash += hex;
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return md5Hash;
}

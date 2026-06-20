#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <Windows.h>
#include <wincrypt.h>
#include "MD5.h"

//对字符串进行哈希摘要
std::string calculateMD5(const TCHAR* inputParam)
{
    HCRYPTPROV hProv = NULL;
    HCRYPTHASH hHash = NULL;

    if (inputParam == NULL)
    {
        return "";
    }

    std::wstring input(inputParam);

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        std::cout << "CryptAcquireContext failed" << std::endl;
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
    {
        std::cout << "CryptCreateHash failed" << std::endl;
        CryptReleaseContext(hProv, 0);
        return "";
    }

    const BYTE* inputData = reinterpret_cast<const BYTE*>(input.c_str());
    DWORD inputSize = static_cast<DWORD>(input.length() * sizeof(wchar_t));
    if (!CryptHashData(hHash, inputData, inputSize, 0))
    {
        std::cout << "CryptHashData failed" << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    DWORD dwHashSize = 0;
    DWORD dwHashLen = sizeof(DWORD);
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&dwHashSize), &dwHashLen, 0))
    {
        std::cout << "CryptGetHashParam failed" << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::vector<BYTE> hash(dwHashSize);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &dwHashSize, 0))
    {
        std::cout << "CryptGetHashParam failed" << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::stringstream md5Stream;
    md5Stream << std::uppercase << std::hex << std::setfill('0');
    for (BYTE byte : hash)
    {
        md5Stream << std::setw(2) << static_cast<int>(byte);
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return md5Stream.str();
}

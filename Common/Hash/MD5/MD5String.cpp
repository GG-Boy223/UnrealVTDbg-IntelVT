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
        // 转换失败，可以根据实际情况进行错误处理
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
        CryptReleaseContext(hProv, 0); // 释放加密提供程序上下文
        return "";
    }

    DWORD dwHashSize = 0;
    DWORD dwHashLen = sizeof(DWORD);
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&dwHashSize), &dwHashLen, 0))
    {
        std::cout << "CryptGetHashParam failed" << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0); // 释放加密提供程序上下文
        return "";
    }

    std::vector<BYTE> hash(dwHashSize);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &dwHashSize, 0))
    {
        std::cout << "CryptGetHashParam failed" << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0); // 释放加密提供程序上下文
        return "";
    }

    // 创建字符串流用于构建十六进制字符串
    std::stringstream md5Stream;
    md5Stream << std::uppercase << std::hex << std::setfill('0');
    for (BYTE byte : hash)
    {
        md5Stream << std::setw(2) << static_cast<int>(byte);
    }

    // 清理资源：销毁哈希对象
    CryptDestroyHash(hHash);
    // 清理资源：释放加密提供程序上下文
    CryptReleaseContext(hProv, 0);

    // 返回计算得到的MD5哈希值（十六进制字符串）
    return md5Stream.str();
}

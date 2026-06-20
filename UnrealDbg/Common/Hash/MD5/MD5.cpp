#include <iostream>
#include <fstream>
#include <iomanip>
#include <Windows.h>
#include <wincrypt.h>
#include <sstream>
#include <vector>
#include "MD5.h"

//计算文件md5
std::string calculateMD5(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);

    if (!file) {
        std::cerr << "Error opening file: " << filePath.c_str() << std::endl;
        return "";
    }

    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        std::cerr << "CryptAcquireContext failed." << std::endl;
        return "";
    }

    HCRYPTHASH hHash = 0;
    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        std::cerr << "CryptCreateHash failed." << std::endl;
        CryptReleaseContext(hProv, 0);
        return "";
    }

    char buffer[4096];
    while (file) {
        file.read(buffer, sizeof(buffer));
        DWORD bytesRead = static_cast<DWORD>(file.gcount());
        if (!CryptHashData(hHash, reinterpret_cast<const BYTE*>(buffer), bytesRead, 0)) {
            std::cerr << "CryptHashData failed." << std::endl;
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }
    }

    //获取密码哈希的哈希值长度
    DWORD hashSize = 0;
    DWORD hashSizeLen = sizeof(DWORD);
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hashSize), &hashSizeLen, 0)) {
        std::cerr << "CryptGetHashParam failed." << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    //获取密码哈希的哈希值
    std::vector<BYTE> hashBuffer(hashSize);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hashBuffer.data(), &hashSize, 0)) {
        std::cerr << "CryptGetHashParam failed." << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::stringstream md5Stream;
    //转十六进制 不足两个字符补0填充 例如：00 0E
    md5Stream << std::hex << std::setfill('0');
    for (BYTE byte : hashBuffer) {
        md5Stream << std::setw(2) << static_cast<int>(byte);
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return md5Stream.str();
}

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

    if (!CryptHashData(hHash, data.data(), data.size(), 0))
    {
        std::cerr << "CryptHashData failed." << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
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

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        std::cout << "CryptAcquireContext failed" << std::endl;
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        std::cout << "CryptCreateHash failed" << std::endl;
        CryptReleaseContext(hProv, 0);
        return "";
    }

    const BYTE* inputData = reinterpret_cast<const BYTE*>(input.c_str());
    DWORD inputSize = static_cast<DWORD>(input.length() * sizeof(wchar_t));
    if (!CryptHashData(hHash, inputData, inputSize, 0)) {
        std::cout << "CryptHashData failed" << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    DWORD dwHashSize = 0;
    DWORD dwHashLen = sizeof(DWORD);
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&dwHashSize), &dwHashLen, 0)) {
        std::cout << "CryptGetHashParam failed" << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::vector<BYTE> hash(dwHashSize);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &dwHashSize, 0)) {
        std::cout << "CryptGetHashParam failed" << std::endl;
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::stringstream md5Stream;
    md5Stream << std::uppercase << std::hex << std::setfill('0');
    for (BYTE byte : hash) {
        md5Stream << std::setw(2) << static_cast<int>(byte);
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return md5Stream.str();
}

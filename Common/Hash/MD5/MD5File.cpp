#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <Windows.h>
#include <wincrypt.h>
#include "MD5.h"

//计算文件md5
// 计算文件的MD5哈希值
std::string calculateMD5(const std::string& filePath)
{
    // 以二进制模式打开文件
    std::ifstream file(filePath, std::ios::binary);

    // 检查文件是否成功打开
    if (!file)
    {
        std::cerr << "Error opening file: " << filePath.c_str() << std::endl;
        return ""; // 文件打开失败，返回空字符串
    }

    // 初始化加密提供程序句柄
    HCRYPTPROV hProv = 0;
    // 获取加密提供程序上下文，用于后续的加密操作
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        std::cerr << "CryptAcquireContext failed." << std::endl;
        return ""; // 获取上下文失败，返回空字符串
    }

    // 初始化哈希句柄
    HCRYPTHASH hHash = 0;
    // 创建MD5哈希对象
    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
    {
        std::cerr << "CryptCreateHash failed." << std::endl;
        CryptReleaseContext(hProv, 0); // 释放加密提供程序上下文
        return ""; // 创建哈希对象失败，返回空字符串
    }

    // 定义缓冲区用于读取文件数据
    char buffer[4096];
    // 循环读取文件内容并计算哈希
    while (file)
    {
        // 从文件中读取数据到缓冲区
        file.read(buffer, sizeof(buffer));
        // 获取实际读取的字节数
        DWORD bytesRead = static_cast<DWORD>(file.gcount());
        // 将读取的数据添加到哈希计算中
        if (!CryptHashData(hHash, reinterpret_cast<const BYTE*>(buffer), bytesRead, 0))
        {
            std::cerr << "CryptHashData failed." << std::endl;
            CryptDestroyHash(hHash); // 销毁哈希对象
            CryptReleaseContext(hProv, 0); // 释放加密提供程序上下文
            return ""; // 哈希数据处理失败，返回空字符串
        }
    }

    // 获取哈希值的长度
    DWORD hashSize = 0;
    DWORD hashSizeLen = sizeof(DWORD);
    // 获取哈希值大小参数
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hashSize), &hashSizeLen, 0))
    {
        std::cerr << "CryptGetHashParam failed." << std::endl;
        CryptDestroyHash(hHash); // 销毁哈希对象
        CryptReleaseContext(hProv, 0); // 释放加密提供程序上下文
        return ""; // 获取哈希大小失败，返回空字符串
    }

    // 创建缓冲区存储哈希值
    std::vector<BYTE> hashBuffer(hashSize);
    // 获取实际的哈希值
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hashBuffer.data(), &hashSize, 0))
    {
        std::cerr << "CryptGetHashParam failed." << std::endl;
        CryptDestroyHash(hHash); // 销毁哈希对象
        CryptReleaseContext(hProv, 0); // 释放加密提供程序上下文
        return ""; // 获取哈希值失败，返回空字符串
    }

    // 创建字符串流用于构建十六进制字符串
    std::stringstream md5Stream;
    // 设置为十六进制输出模式，并用0填充
    md5Stream << std::hex << std::setfill('0');
    // 将每个字节转换为两位十六进制字符串
    for (BYTE byte : hashBuffer)
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

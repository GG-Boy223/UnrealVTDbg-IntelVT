#include "CryptoFile.h"

#include <Windows.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "advapi32.lib")

namespace
{
    class CryptProviderHandle
    {
    public:
        CryptProviderHandle() = default;
        CryptProviderHandle(const CryptProviderHandle&) = delete;
        CryptProviderHandle& operator=(const CryptProviderHandle&) = delete;

        ~CryptProviderHandle()
        {
            if (handle_ != NULL)
            {
                CryptReleaseContext(handle_, 0);
            }
        }

        HCRYPTPROV* Receive()
        {
            return &handle_;
        }

        HCRYPTPROV Get() const
        {
            return handle_;
        }

    private:
        HCRYPTPROV handle_ = NULL;
    };

    class CryptHashHandle
    {
    public:
        CryptHashHandle() = default;
        CryptHashHandle(const CryptHashHandle&) = delete;
        CryptHashHandle& operator=(const CryptHashHandle&) = delete;

        ~CryptHashHandle()
        {
            if (handle_ != NULL)
            {
                CryptDestroyHash(handle_);
            }
        }

        HCRYPTHASH* Receive()
        {
            return &handle_;
        }

        HCRYPTHASH Get() const
        {
            return handle_;
        }

    private:
        HCRYPTHASH handle_ = NULL;
    };

    class CryptKeyHandle
    {
    public:
        CryptKeyHandle() = default;
        CryptKeyHandle(const CryptKeyHandle&) = delete;
        CryptKeyHandle& operator=(const CryptKeyHandle&) = delete;

        ~CryptKeyHandle()
        {
            if (handle_ != NULL)
            {
                CryptDestroyKey(handle_);
            }
        }

        HCRYPTKEY* Receive()
        {
            return &handle_;
        }

        HCRYPTKEY Get() const
        {
            return handle_;
        }

    private:
        HCRYPTKEY handle_ = NULL;
    };

    DWORD CheckedSizeToDword(size_t value)
    {
        if (value > static_cast<size_t>((std::numeric_limits<DWORD>::max)()))
        {
            throw std::overflow_error("buffer size exceeds DWORD range");
        }

        return static_cast<DWORD>(value);
    }

    DWORD CheckedStreamSizeToDword(std::streamoff value)
    {
        if (value < 0)
        {
            throw std::runtime_error("invalid stream size");
        }

        return CheckedSizeToDword(static_cast<size_t>(value));
    }
}

void EncryptDataToFile_internal(const std::string& data, const std::string& filename, const std::string& userKey)
{
    const DWORD userKeyLength = CheckedSizeToDword(userKey.length());
    BYTE* pbData = (BYTE*)data.c_str();
    DWORD dwPlainTextLen = CheckedSizeToDword(data.length());
    DWORD dwBufLen = dwPlainTextLen;
    CryptProviderHandle hProv;
    CryptKeyHandle hKey;
    CryptHashHandle hHash;

    if (!CryptAcquireContext(hProv.Receive(), NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
    {
        std::cerr << "CryptAcquireContext failed: " << GetLastError() << std::endl;
        return;
    }

    if (!CryptCreateHash(hProv.Get(), CALG_SHA_256, 0, 0, hHash.Receive()))
    {
        std::cerr << "CryptCreateHash failed: " << GetLastError() << std::endl;
        return;
    }

    if (!CryptHashData(hHash.Get(), (BYTE*)userKey.c_str(), userKeyLength, 0))
    {
        std::cerr << "CryptHashData failed: " << GetLastError() << std::endl;
        return;
    }

    if (!CryptDeriveKey(hProv.Get(), CALG_AES_256, hHash.Get(), 0, hKey.Receive()))
    {
        std::cerr << "CryptDeriveKey failed: " << GetLastError() << std::endl;
        return;
    }

    if (!CryptEncrypt(hKey.Get(), 0, TRUE, 0, NULL, &dwBufLen, 0))
    {
        std::cerr << "CryptEncrypt (size) failed: " << GetLastError() << std::endl;
        return;
    }

    if (dwBufLen == (std::numeric_limits<DWORD>::max)())
    {
        std::cerr << "Encrypted buffer is too large" << std::endl;
        return;
    }

    const DWORD dwCipherCapacity = dwBufLen + 1;
    std::vector<BYTE> pbCipherText(dwCipherCapacity);
    memcpy(pbCipherText.data(), pbData, dwPlainTextLen);

    DWORD tmp = dwPlainTextLen;
    if (!CryptEncrypt(hKey.Get(), 0, TRUE, 0, pbCipherText.data(), &tmp, dwCipherCapacity))
    {
        std::cerr << "CryptEncrypt failed: " << GetLastError() << std::endl;
        return;
    }

    std::ofstream outFile(filename, std::ios::binary);
    outFile.write((char*)pbCipherText.data(), static_cast<std::streamsize>(dwBufLen));
    outFile.close();
}

void DecryptDataFromFile_internal(const std::string& filename, const std::string& userKey, std::string& decryptedData)
{
    const DWORD userKeyLength = CheckedSizeToDword(userKey.length());
    DWORD dwCipherTextLen = 0;
    CryptProviderHandle hProv;
    CryptKeyHandle hKey;
    CryptHashHandle hHash;

    if (!CryptAcquireContext(hProv.Receive(), NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
    {
        std::cerr << "CryptAcquireContext failed: " << GetLastError() << std::endl;
        return;
    }

    if (!CryptCreateHash(hProv.Get(), CALG_SHA_256, 0, 0, hHash.Receive()))
    {
        std::cerr << "CryptCreateHash failed: " << GetLastError() << std::endl;
        return;
    }

    if (!CryptHashData(hHash.Get(), (BYTE*)userKey.c_str(), userKeyLength, 0))
    {
        std::cerr << "CryptHashData failed: " << GetLastError() << std::endl;
        return;
    }

    if (!CryptDeriveKey(hProv.Get(), CALG_AES_256, hHash.Get(), 0, hKey.Receive()))
    {
        std::cerr << "CryptDeriveKey failed: " << GetLastError() << std::endl;
        return;
    }

    std::ifstream inFile(filename, std::ios::binary | std::ios::ate);
    if (!inFile.is_open())
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    dwCipherTextLen = CheckedStreamSizeToDword(inFile.tellg());
    inFile.seekg(0, std::ios::beg);
    std::vector<BYTE> pbCipherText(static_cast<size_t>(dwCipherTextLen) + 1);
    inFile.read((char*)pbCipherText.data(), static_cast<std::streamsize>(dwCipherTextLen));
    inFile.close();

    DWORD dwPlainTextLen = dwCipherTextLen;
    if (!CryptDecrypt(hKey.Get(), 0, TRUE, 0, pbCipherText.data(), &dwPlainTextLen))
    {
        std::cerr << "CryptDecrypt failed: " << GetLastError() << std::endl;
        return;
    }

    decryptedData.assign((char*)pbCipherText.data(), dwPlainTextLen);
}

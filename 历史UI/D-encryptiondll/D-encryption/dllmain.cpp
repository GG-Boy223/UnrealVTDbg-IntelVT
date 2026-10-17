#include "dllmain.h"
#include "Crypto/CryptoFile.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    int CheckedSizeToInt(size_t value)
    {
        if (value > static_cast<size_t>((std::numeric_limits<int>::max)()))
        {
            throw std::overflow_error("buffer size exceeds int range");
        }

        return static_cast<int>(value);
    }
}

void EncryptDataToFile(TCHAR* data, TCHAR* filename, TCHAR* userKey)
{
    try
    {
        if (data && filename && userKey)
        {
            std::string _data = Common::wideStringToString(data);
            std::string _filename = Common::wideStringToString(filename);
            std::string _userKey = Common::wideStringToString(userKey);
            EncryptDataToFile_internal(_data, _filename, _userKey);
        }
    }
    catch (const std::exception& e)
    {
        Common::ReportSeriousError("%s[%d]  %s", __func__, __LINE__, e.what());
    }
}

int DecryptDataFromFile(TCHAR* filename, TCHAR* userKey, TCHAR* decryptedData)
{
    int decryptedDataLen = 0;
    try
    {
        if (filename && userKey)
        {
            std::string _filename = Common::wideStringToString2(filename);
            std::string _userKey = Common::wideStringToString(userKey);
            std::string _decryptedData;
            DecryptDataFromFile_internal(_filename, _userKey, _decryptedData);

            std::wstring ws_decryptedData = Common::stringToWideString(_decryptedData);
            decryptedDataLen = CheckedSizeToInt((ws_decryptedData.length() + 1) * sizeof(WCHAR));

            if (decryptedData)
            {
                wcscpy(decryptedData, ws_decryptedData.c_str());
            }
        }
    }
    catch (const std::exception& e)
    {
        Common::ReportSeriousError("%s[%d]  %s", __func__, __LINE__, e.what());
    }
    return decryptedDataLen;
}

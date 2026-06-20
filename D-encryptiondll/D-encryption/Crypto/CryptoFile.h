#pragma once

#include <string>

void EncryptDataToFile_internal(const std::string& data, const std::string& filename, const std::string& userKey);
void DecryptDataFromFile_internal(const std::string& filename, const std::string& userKey, std::string& decryptedData);

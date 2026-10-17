#pragma once

#include "../WinMain.h"
#include "../Symbols.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <memory>

extern std::wstring g_szPdbPath;
extern CStatusCallback callback;

bool CreateDirectoryIfNotExists(const TCHAR* dir);
bool VerifyExistingPdb(const GUID& guid);
std::wstring GetFileName(const std::wstring& full);
std::string BuildSymbolServerBaseUrl(const std::wstring& configPath);

#include <Windows.h>
#include <string>
#include <fstream>
#include <vector>
#include <tchar.h>
#include "FileSystem.h"

namespace FileSystem
{
    //创建ini文件
    void CreateIniFile(const std::wstring& filename)
    {
        std::wofstream file(filename);
        file.close();
    }

    //删除ini文件
    bool DeleteIniFile(const std::wstring& filename)
    {
        if (_wremove(filename.c_str()) == 0) {
            return true; // 成功删除文件
        }
        else {
            return false; // 删除文件失败
        }
    }

    //遍历指定目录下的文件
    std::vector<std::wstring> TraverseDirectory(const std::wstring& directoryPath)
    {
        std::vector<std::wstring> files;

        if (!directoryPath.empty())
        {
            std::wstring searchPath = directoryPath + L"\\*.*";
            WIN32_FIND_DATAW findData;
            HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

            if (hFind != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        //std::wstring filePath = directoryPath + L"\\" + findData.cFileName;
                        std::wstring filePath = findData.cFileName;
                        files.push_back(filePath);
                    }
                } while (FindNextFileW(hFind, &findData));

                FindClose(hFind);
            }
        }

        return files;
    }

    std::wstring ReadIniValue(const std::wstring& filename, const std::wstring& section, const std::wstring& key)
    {
        wchar_t buffer[256];
        GetPrivateProfileString(section.c_str(), key.c_str(), L"", buffer, 256, filename.c_str());
        return std::wstring(buffer);
    }

    void WriteIniValue(const std::wstring& filename, const std::wstring& section, const std::wstring& key, const std::wstring& value)
    {
        WritePrivateProfileString(section.c_str(), key.c_str(), value.c_str(), filename.c_str());
    }

    std::wstring GetModuleDirectory(HMODULE hModule)
    {
        wchar_t buffer[MAX_PATH];
        DWORD length = GetModuleFileName(hModule, buffer, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            std::wstring path(buffer, length);
            size_t lastSlash = path.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos)
            {
                std::wstring directory = path.substr(0, lastSlash + 1);
                return directory;
            }
        }
        return L"";
    }

    std::wstring GetModuleDirectory2(std::wstring path)
    {
        if (!path.empty())
        {
            size_t lastSlash = path.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos)
            {
                std::wstring directory = path.substr(0, lastSlash + 1);
                return directory;
            }
        }
        return L"";
    }

    //获取自身模块名称
    std::string GetSelfModuleName()
    {
        HMODULE hModule = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&GetSelfModuleName), &hModule);
        if (hModule)
        {
            char buffer[MAX_PATH];
            GetModuleFileNameA(hModule, buffer, MAX_PATH);
            std::string fullPath(buffer);
            size_t pos = fullPath.find_last_of("\\/");
            if (pos != std::string::npos)
            {
                std::string moduleName = fullPath.substr(pos + 1);
                if (moduleName.empty())
                {
                    ::MessageBox(NULL, _T("定位模块失败!"), _T("错误:"), MB_ICONWARNING);
                }
                else
                {
                    return moduleName;
                }
            }
        }
        return "";
    }
}
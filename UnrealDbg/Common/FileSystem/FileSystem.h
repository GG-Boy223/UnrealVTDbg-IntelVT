#pragma once

#ifndef _FILE_SYSTEM_H
#define _FILE_SYSTEM_H

namespace FileSystem
{
    //创建ini文件
    void CreateIniFile(const std::wstring& filename);
    //删除ini文件
    bool DeleteIniFile(const std::wstring& filename);
    //遍历指定目录下的文件
    std::vector<std::wstring> TraverseDirectory(const std::wstring& directoryPath);
    std::wstring ReadIniValue(const std::wstring& filename, const std::wstring& section, const std::wstring& key);
    void WriteIniValue(const std::wstring& filename, const std::wstring& section, const std::wstring& key, const std::wstring& value);
    std::wstring GetModuleDirectory(HMODULE hModule);
    std::wstring GetModuleDirectory2(std::wstring path);
    //获取自身模块名称
    std::string GetSelfModuleName();
}

#endif // !_FILE_SYSTEM_H

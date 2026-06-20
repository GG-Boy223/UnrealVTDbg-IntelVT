#pragma once

#ifndef _MD5_H
#define _MD5_H

//计算文件md5
std::string calculateMD5(const std::string& filePath);
//字节流哈希摘要
std::string calculateMD5(const std::vector<unsigned char>& data);
//对字符串进行哈希摘要
std::string calculateMD5(const TCHAR* inputParam);

#endif // !_MD5_H

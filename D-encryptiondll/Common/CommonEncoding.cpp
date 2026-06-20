#include <Windows.h>
#include <string>
#include <codecvt>
#include <tuple>
#include <vector>
#include <limits>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include "Common.h"

namespace Common
{
	//string转wstring
	std::wstring stringToWideString(const std::string& narrowStr)
	{
		// 获取宽字符字符串的长度（包括空终止符）
		int wideStrLength = MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, nullptr, 0);

		// 分配内存来存储宽字符字符串
		wchar_t* wideStr = new wchar_t[wideStrLength];

		// 将窄字符转换为宽字符
		MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, wideStr, wideStrLength);

		// 创建 std::wstring 对象
		std::wstring result(wideStr);

		// 释放内存
		delete[] wideStr;

		return result;
	}

	//wstring转string
	//注意: 在Windows下将utf16转utf8的std::string是无法正常显示的
	std::string wideStringToString(const std::wstring& wideStr)
	{
		int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string str(bufferSize - 1, 0);
		WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &str[0], bufferSize - 1, nullptr, nullptr);
		return str;
	}

	//wstring转本地string
	//注意: 本地ansi可以显示中文，但请不要再网络内容传输中使用它，因为不同计算机本地代码页不相同.
	std::string wideStringToString2(const std::wstring& wideStr)
	{
		int bufferSize = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string str(bufferSize - 1, 0);
		WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &str[0], bufferSize - 1, nullptr, nullptr);
		return str;
	}

	//wchar_t*转string
	std::string wcharToString(const wchar_t* str)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		return converter.to_bytes(str);
	}

	//wchar_t*转wstring
	std::wstring wcharToWideString(const wchar_t* wcharStr)
	{
		// 使用构造函数将 wchar_t* 转换为 std::wstring
		std::wstring wideStr(wcharStr);

		return wideStr;
	}

	//char*转wchar_t*
	std::wstring ConvertCharToWchar(const char* charStr)
	{
		const size_t charStrLengthSize = strlen(charStr) + 1; // char 字符串的长度（包括 null 终止符）
		if (charStrLengthSize > static_cast<size_t>((std::numeric_limits<int>::max)()))
		{
			throw std::overflow_error("char string is too long to convert");
		}
		const int charStrLength = static_cast<int>(charStrLengthSize);

		// 计算 wchar_t 字符串所需的缓冲区大小
		const int wcharStrSize = MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLength, nullptr, 0);

		// 分配 wchar_t 缓冲区
		wchar_t* wcharStr = new wchar_t[wcharStrSize];

		// 执行转换
		MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLength, wcharStr, wcharStrSize);

		// 将 wchar_t 字符串封装到 std::wstring 类型
		std::wstring result(wcharStr);

		// 释放内存
		delete[] wcharStr;

		return result;
	}

	//gbk转utf8
	std::string GbkToUTF8(const std::string& gbkString)
	{
		int bufferSize = MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, nullptr, 0);
		std::wstring wideString(bufferSize - 1, L'\0');
		MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, &wideString[0], bufferSize - 1);

		bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string utf8String(bufferSize - 1, '\0');
		WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], bufferSize - 1, nullptr, nullptr);

		return utf8String;
	}

	// 将 utf8 编码的字符串转换为 GBK 编码
	std::string utf8ToGbk(const std::string& utf8String)
	{
		int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
		if (bufferSize == 0)
		{
			// 转换失败，可以根据实际情况进行错误处理
			return "";
		}

		std::wstring wideString(bufferSize, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &wideString[0], bufferSize);

		bufferSize = WideCharToMultiByte(CP_ACP, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (bufferSize == 0)
		{
			// 转换失败，可以根据实际情况进行错误处理
			return "";
		}

		std::string gbkString(bufferSize, '\0');
		WideCharToMultiByte(CP_ACP, 0, wideString.c_str(), -1, &gbkString[0], bufferSize, nullptr, nullptr);

		return gbkString;
	}

	// 将 utf8 编码的字符串转换为 Unicode 编码
	std::wstring utf8ToUnicode(const std::string& utf8String)
	{
		int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
		std::wstring unicodeString(bufferSize, 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &unicodeString[0], bufferSize);
		return unicodeString;
	}

	//本地代码页转std::wstring
	std::wstring ConvertLocalCodePageToWideString(const std::string& str)
	{
		int wideStrLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
		if (wideStrLen == 0)
		{
			// 转换失败，可以根据实际情况处理错误
			return L"";
		}

		std::wstring wideStr(wideStrLen, L'\0');
		if (MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wideStr[0], wideStrLen) == 0)
		{
			// 转换失败，可以根据实际情况处理错误
			return L"";
		}

		// 去掉末尾的空字符
		wideStr.resize(wideStrLen - 1);

		return wideStr;
	}

	//本地代码页转std::string
	std::string LocalCodePageToUtf8(const std::string& localString)
	{
		int wideCharLength = MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, nullptr, 0);
		if (wideCharLength == 0)
		{
			// 转换失败
			return "";
		}

		std::wstring wideString(wideCharLength, L'\0');
		if (MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, &wideString[0], wideCharLength) == 0)
		{
			// 转换失败
			return "";
		}

		int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (utf8Length == 0)
		{
			// 转换失败
			return "";
		}

		std::string utf8String(utf8Length, '\0');
		if (WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) == 0)
		{
			// 转换失败
			return "";
		}

		return utf8String;
	}

	//Unicode转Utf8
	std::string UnicodeToUtf8(const std::wstring& unicodeString)
	{
		int utf8Length = WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (utf8Length == 0)
		{
			// 转换失败
			return "";
		}

		std::string utf8String(utf8Length, '\0');
		if (WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) == 0)
		{
			// 转换失败
			return "";
		}

		return utf8String;
	}
}

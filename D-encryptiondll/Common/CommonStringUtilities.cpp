#include <Windows.h>
#include <string>
#include <random>
#include <tuple>
#include <vector>
#include <cstdint>
#include "Common.h"

namespace Common
{
	//生成16位随机字符串
	std::string generateRandomString()
	{
		const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		const int length = 16;
		const int maxIndex = static_cast<int>(characters.length() - 1);

		std::random_device rd;
		std::mt19937 generator(rd());
		std::uniform_int_distribution<int> distribution(0, maxIndex);

		std::string randomString;

		for (int i = 0; i < length; ++i)
		{
			randomString += characters[distribution(generator)];
		}

		return randomString;
	}

	//字符串截取
	std::string truncateString(const std::string& input, int length)
	{
		if (length >= input.length())
		{
			return input;
		}
		else
		{
			return input.substr(0, length);
		}
	}

	//截取字符串 和剩余字符串
	std::tuple<std::string, std::string> truncateString2(const std::string& input, int length)
	{
		if (length >= input.length())
		{
			return std::make_tuple(input, "");
		}
		else
		{
			return std::make_tuple(input.substr(0, length), input.substr(length));
		}
	}

	//将string转小写
	std::string ToLowerWindows(const std::string& str)
	{
		std::string lowerStr(str);
		CharLowerBuffA(&lowerStr[0], static_cast<DWORD>(lowerStr.size()));

		return lowerStr;
	}

	//将wstring转小写
	std::wstring ToLowerWindows(const std::wstring& str)
	{
		std::wstring lowerStr(str);
		CharLowerBuffW(&lowerStr[0], static_cast<DWORD>(lowerStr.size()));

		return lowerStr;
	}
}

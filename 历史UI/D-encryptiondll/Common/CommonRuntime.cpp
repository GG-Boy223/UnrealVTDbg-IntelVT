#include <Windows.h>
#include <array>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>
#include <cstdint>
#include <intrin.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "Common.h"

namespace Common
{
	extern HANDLE hMutex;
	extern std::mutex mutex;

	//单例模式
	//防止程序多开
	BOOL SingletonPattern(const wchar_t* mutexName)
	{
		BOOL boRet = FALSE;

		// 创建互斥体
		hMutex = CreateMutexW(nullptr, TRUE, mutexName);

		// 检查互斥体是否已存在
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			// 关闭互斥体句柄并退出程序
			CloseHandle(hMutex);
		}
		else
		{
			boRet = TRUE;;
		}
		return boRet;
	}

	//退出单例
	void SingletonProgramEnd()
	{
		// 关闭互斥体句柄
		if (hMutex)
		{
			CloseHandle(hMutex);
		}
	}

	//int转wstring
	std::wstring IntToWString(int value)
	{
		return std::to_wstring(value);
	}

	//wstring转int
	int WStringToInt(const std::wstring& str)
	{
		return std::stoi(str);
	}

	//确认CPU型号
	void ConfirmCPUVendor()
	{
		std::array<int, 4> cpui;

		// Calling __cpuid with 0x0 as the function_id argument
		// gets the number of the highest valid function ID.
		__cpuid(cpui.data(), 0);

		// Capture vendor string
		char vendor[0x20];
		memset(vendor, 0, sizeof(vendor));
		*reinterpret_cast<int*>(vendor) = cpui[ebx];
		*reinterpret_cast<int*>(vendor + 4) = cpui[edx];
		*reinterpret_cast<int*>(vendor + 8) = cpui[ecx];
		std::string vendor_ = vendor;
		if (vendor_ == "GenuineIntel")
		{
			isIntel = true;
		}
		else if (vendor_ == "AuthenticAMD")
		{
			isAMD = true;
		}
	}

	void ReportSeriousError(const char* format, ...)
	{
		// 线程同步：使用互斥锁保护临界区
		std::lock_guard<std::mutex> lock(mutex);

		va_list args;
		va_start(args, format);
		char message[1024] = { 0 };
		vsnprintf(message, sizeof(message), format, args);
		va_end(args);

		std::ostringstream oss;
		oss << message;

		std::string logMessage = oss.str();
		if (!logMessage.empty())
		{
			MessageBoxA(NULL, logMessage.c_str(), "严重错误:", MB_ICONERROR | MB_SYSTEMMODAL);
		}
	}

	bool fileExists(const std::wstring& path)
	{
		HANDLE hFile = CreateFile(
			path.c_str(),
			GENERIC_READ,
			0, // 不共享
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		if (hFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFile);
			return true; // 文件存在
		}
		else
		{
			return false; // 文件不存在
		}
	}
}

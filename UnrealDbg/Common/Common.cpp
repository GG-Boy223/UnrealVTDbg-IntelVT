#include <iostream>
#include <Windows.h>
#include <string>
#include <codecvt>
#include <random>
#include <tuple>
#include <TlHelp32.h>
#include <vector>
#include <psapi.h>
#include <intrin.h>
#include <array>
#include <mutex>
#include <fstream>
#include <sstream>
#include "Common.h"

namespace Common
{

	HANDLE hMutex;// 防多开
	bool isIntel = false;
	bool isAMD = false;
	std::mutex mutex; // 互斥锁

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
	//注意: 在Windows下将utf16转utf8的std::string是无法正常显示中文的
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
		const int charStrLength = strlen(charStr) + 1; // char 字符串的长度（包括 null 终止符）

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

	//gbk转utf8
	//std::string GbkToUTF8(const std::string& gbkString)
	//{
	//	int bufferSize = MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, nullptr, 0);
	//	std::wstring wideString(bufferSize, L'\0');
	//	MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, &wideString[0], bufferSize);

	//	bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
	//	std::string utf8String(bufferSize, '\0');
	//	WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], bufferSize, nullptr, nullptr);

	//	return utf8String;
	//}

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
		if (wideCharLength == 0) {
			// 转换失败
			return "";
		}

		std::wstring wideString(wideCharLength, L'\0');
		if (MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, &wideString[0], wideCharLength) == 0) {
			// 转换失败
			return "";
		}

		int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (utf8Length == 0) {
			// 转换失败
			return "";
		}

		std::string utf8String(utf8Length, '\0');
		if (WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) == 0) {
			// 转换失败
			return "";
		}

		return utf8String;
	}

	//Unicode转Utf8
	std::string UnicodeToUtf8(const std::wstring& unicodeString)
	{
		int utf8Length = WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (utf8Length == 0) {
			// 转换失败
			return "";
		}

		std::string utf8String(utf8Length, '\0');
		if (WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) == 0) {
			// 转换失败
			return "";
		}

		return utf8String;
	}

	//生成16位随机字符串
	std::string generateRandomString()
	{
		const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		const int length = 16;

		std::random_device rd;
		std::mt19937 generator(rd());
		std::uniform_int_distribution<int> distribution(0, characters.length() - 1);

		std::string randomString;

		for (int i = 0; i < length; ++i) {
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

	//枚举进程
	std::vector<ProcessInfo> EnumerateProcesses()
	{
		std::vector<ProcessInfo> processes;

		// 获取系统中所有进程的快照
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE)
		{
			// 返回空容器
			return processes;
		}

		PROCESSENTRY32W processEntry = { sizeof(PROCESSENTRY32W) };

		// 枚举进程快照中的进程信息
		if (Process32First(hSnapshot, &processEntry))
		{
			do
			{
				ProcessInfo process;
				process.processId = processEntry.th32ProcessID;
				process.processName = processEntry.szExeFile;

				// 打开进程
				HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processEntry.th32ProcessID);
				if (hProcess != nullptr)
				{
					TCHAR modulePath[MAX_PATH] = { 0 };
					if (GetModuleFileNameEx(hProcess, NULL, modulePath, MAX_PATH))
					{
						process.FullPath = modulePath;
					}
					CloseHandle(hProcess);					
				}
				processes.push_back(process);
			} while (Process32Next(hSnapshot, &processEntry));
		}

		// 关闭进程快照句柄
		CloseHandle(hSnapshot);

		return processes;
	}

	//检查目标进程是否正在运行
	BOOL IsProcessRunning(const std::wstring& processName)
	{
		BOOL boRet = FALSE;
		PROCESSENTRY32W entry;
		entry.dwSize = sizeof(PROCESSENTRY32W);

		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot != INVALID_HANDLE_VALUE)
		{
			if (Process32FirstW(hSnapshot, &entry))
			{
				do
				{
					std::wstring currentProcessName = Common::ToLowerWindows(entry.szExeFile);
					if (currentProcessName.find(Common::ToLowerWindows(processName)) != std::wstring::npos)  //查找子串
					{
						boRet = TRUE;
						break;
					}
				} while (Process32NextW(hSnapshot, &entry));
			}
			CloseHandle(hSnapshot);
		}
		return boRet;
	}

	//查找窗口信息
	BOOL FindWindowInfo(LPCWSTR lpClassName, LPCWSTR titleName)
	{
		if (FindWindow(lpClassName, titleName))
		{
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	//终止进程
	bool TerminateWindowsProcess(DWORD processId)
	{
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
		if (hProcess == NULL)
		{
			// 处理打开进程失败的情况
			return false;
		}

		// 终止进程
		bool result = TerminateProcess(hProcess, 0);

		// 关闭进程句柄
		CloseHandle(hProcess);

		return result;
	}


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

	BOOL xxx_Process(DWORD dwProcessID, BOOL fSuspend)
	{
		BOOL bRet = FALSE;
		//Get the list of threads in the system.
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, dwProcessID);

		if (hSnapshot != INVALID_HANDLE_VALUE)
		{
			//Walk the list of threads.
			THREADENTRY32 te = { sizeof(te) };
			BOOL fOk = Thread32First(hSnapshot, &te);

			for (; fOk; fOk = Thread32Next(hSnapshot, &te))
			{
				//Is this thread in the desired process?
				if (te.th32OwnerProcessID == dwProcessID)
				{
					//Attempt to convert the thread ID into a handle.
					HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);

					if ((hThread != NULL) && (GetCurrentThreadId() != te.th32ThreadID))
					{
						//Suspend or resume the thread.
						if (fSuspend)
						{
							if (SuspendThread(hThread) != -1)
								bRet = TRUE;
						}
						else
						{
							if (ResumeThread(hThread) != -1)
								bRet = TRUE;
						}
					}
					CloseHandle(hThread);
				}
			}
			CloseHandle(hSnapshot);
		}
		return bRet;
	}

	//暂停进程
	BOOL SuspendProcess(DWORD dwProcessID)
	{
		return xxx_Process(dwProcessID, TRUE);
	}

	//恢复进程
	BOOL ResumeProcess(DWORD dwProcessID)
	{
		return xxx_Process(dwProcessID, FALSE);
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

		if (hFile != INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
			return true; // 文件存在
		}
		else {
			return false; // 文件不存在
		}
	}

	//获取系统版本信息
	BOOL GetNtVersionNumbers(DWORD& dwMajorVer, DWORD& dwMinorVer, DWORD& dwBuildNumber)
	{
		BOOL bRet = FALSE;
		HMODULE hModNtdll = GetModuleHandle(L"ntdll.dll");
		if (hModNtdll)
		{
			typedef VOID(NTAPI* PFN_RTLGETNTVERSIONNUMBERS)(OUT PULONG pMajorVersion,
					OUT PULONG pMinorVersion,
					OUT PULONG pBuildNumber);
			PFN_RTLGETNTVERSIONNUMBERS pfnRtlGetNtVersionNumbers;
			pfnRtlGetNtVersionNumbers = (PFN_RTLGETNTVERSIONNUMBERS)GetProcAddress(hModNtdll, "RtlGetNtVersionNumbers");
			if (pfnRtlGetNtVersionNumbers)
			{
				pfnRtlGetNtVersionNumbers(&dwMajorVer, &dwMinorVer, &dwBuildNumber);
				dwBuildNumber &= 0x0ffff;
				bRet = TRUE;
			}
		}

		return bRet;
	}

}
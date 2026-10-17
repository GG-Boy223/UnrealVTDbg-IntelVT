#pragma once

#ifndef _COMMON_H
#define _COMMON_H

#ifdef _WIN64

#define CONTEXT_EXTENDED_REGISTERS 0

#endif // _WIN64


enum IllegalEnum
{
	Illegal_detectedHackTools = 1998,
};

enum cpuid_reg
{
	eax,
	ebx,
	ecx,
	edx
};

//Vol3B[18.2 DEBUG REGISTERS]
union Dr6 {
	uint64_t flags;
	struct
	{
		uint64_t B0 : 1; //bit0
		uint64_t B1 : 1; //bit1
		uint64_t B2 : 1; //bit2
		uint64_t B3 : 1; //bit3
		uint64_t Reserved_1 : 7; //bit10:4
		uint64_t BLD : 1; //bit11
		uint64_t Reserved_2 : 1; //bit12
		uint64_t BD : 1; //bit13
		uint64_t BS : 1; //bit14 (single step)
		uint64_t BT : 1; //bit15
		uint64_t RTM : 1; //bit16
	};
};

union EFLAGS
{
	uint32_t flags;
	struct
	{
		uint32_t CF : 1; //bit0
		uint32_t Reserved_1 : 1; //bit1
		uint32_t PF : 1; //bit2
		uint32_t Reserved_2 : 1; //bit3
		uint32_t AF : 1; //bit4
		uint32_t Reserved_3 : 1; //bit5
		uint32_t ZF : 1; //bit6
		uint32_t SF : 1; //bit7
		uint32_t TF : 1; //bit8
		uint32_t IF : 1; //bit9
	};
};

namespace Common
{
	struct ProcessInfo
	{
		DWORD processId;
		std::wstring processName;
		std::wstring FullPath;
	};

	extern bool isIntel;
	extern bool isAMD;

	//string转wstring
	std::wstring stringToWideString(const std::string& narrowStr);
	//wstring转string
	std::string wideStringToString(const std::wstring& wideStr);
	//wstring转本地string
	std::string wideStringToString2(const std::wstring& wideStr);
	//wchar_t*转string
	std::string wcharToString(const wchar_t* str);
	//wchar_t*转wstring
	std::wstring wcharToWideString(const wchar_t* wcharStr);
	//char*תwchar_t*
	std::wstring ConvertCharToWchar(const char* charStr);
	//gbk转utf8
	std::string GbkToUTF8(const std::string& gbkString);
	//将utf8编码的字符串转换为GBK编码
	std::string utf8ToGbk(const std::string& utf8String);
	//将utf8编码的字符串转换为Unicode编码
	std::wstring utf8ToUnicode(const std::string& utf8String);
	//本地代码页转std::wstring
	std::wstring ConvertLocalCodePageToWideString(const std::string& str);
	//本地代码页转std::string
	std::string LocalCodePageToUtf8(const std::string& localString);
	//Unicode转utf8
	std::string UnicodeToUtf8(const std::wstring& unicodeString);
	//生成16位随机字符串
	std::string generateRandomString();
	//字符串截取
	std::string truncateString(const std::string& input, int length);
	//截取字符串 和剩余字符串
	std::tuple<std::string, std::string> truncateString2(const std::string& input, int length);
	//将string转小写
	std::string ToLowerWindows(const std::string& str);
	//将wstring转小写
	std::wstring ToLowerWindows(const std::wstring& str);
	//枚举进程
	std::vector<ProcessInfo> EnumerateProcesses();
	//检查目标进程是否正在运行
	BOOL IsProcessRunning(const std::wstring& processName);
	//查找窗口信息
	BOOL FindWindowInfo(LPCWSTR lpClassName, LPCWSTR titleName);
	//终止进程
	bool TerminateWindowsProcess(DWORD processId);
	//单例模式
	//防止程序多开
	BOOL SingletonPattern(const wchar_t* mutexName);
	//退出单例
	void SingletonProgramEnd();
	//int转wstring
	std::wstring IntToWString(int value);
	//wstring转int
	int WStringToInt(const std::wstring& str);
	//确认CPU型号
	void ConfirmCPUVendor();
	//暂停进程
	BOOL SuspendProcess(DWORD dwProcessID);
	//恢复进程
	BOOL ResumeProcess(DWORD dwProcessID);
	//错误报告
	void ReportSeriousError(const char* format, ...);
	//检查文件是否存在
	bool fileExists(const std::wstring& path);
	//获取系统版本信息
	BOOL GetNtVersionNumbers(DWORD& dwMajorVer, DWORD& dwMinorVer, DWORD& dwBuildNumber);
}


#endif // !_COMMON_H

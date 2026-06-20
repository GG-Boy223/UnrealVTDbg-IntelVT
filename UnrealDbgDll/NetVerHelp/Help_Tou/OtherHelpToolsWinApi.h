#pragma once
#ifndef OtherHelpWinApi_H_
#define OtherHelpWinApi_H_

#include <windows.h>

#include <list>
#include <locale.h>
#include <string>

using namespace std;

namespace OtherHelp
{
	class OtherHelpTools
	{
	public:
		static std::wstring StoWs(const std::string& s);
		static void TerminateApp();
		static string PVOIDToString(LPVOID lp);
		static std::string WString2String(const std::wstring& ws);
		static std::wstring String2WString(const std::string& s);
	};
}

#endif

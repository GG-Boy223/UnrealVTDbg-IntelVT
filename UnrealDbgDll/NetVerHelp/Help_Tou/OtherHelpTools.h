#pragma once
#ifndef OtherHelp_H_
#define OtherHelp_H_
#include <afxinet.h>
#include <string>
#include <list>
#include<locale.h>
using namespace std;
namespace OtherHelp
{
	/*
	类型转换
	*/
	// /*
	// 类型转换
	// */
	class OtherHelpTools
	{
	public:
		/// <summary>
		/// //string转wstring
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		static std::wstring StoWs(const std::string& s);


		/// <summary>
		/// 强行结束程序
		/// </summary>
		static void TerminateApp();

		/// <summary>
		/// // LPVOID转CString
		/// </summary>
		/// <param name="lp"></param>
		/// <returns></returns>
		static string PVOIDToString(LPVOID lp);

		/// <summary>
		/// //wstring=>string
		/// </summary>
		/// <param name="ws"></param>
		/// <returns></returns>
		static std::string WString2String(const std::wstring& ws);

		/// <summary>
		/// //string => wstring
		/// </summary>
		/// <param name="s"></param>
		/// <returns></returns>
		static std::wstring String2WString(const std::string& s);
	};
}
#endif
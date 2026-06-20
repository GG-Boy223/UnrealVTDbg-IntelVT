#pragma once
#ifndef CHttpRequestToolWinInet_H_
#define CHttpRequestToolWinInet_H_

#include <windows.h>
#include <wininet.h>

#include <list>
#include <string>

#include "../../Crypter/xorstr.hpp"

using namespace std;

namespace Help
{
	class CHttpRequestTool
	{
	public:
		CHttpRequestTool(string strAgent = (const char*)_xor_("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/74.0.3729.108 Safari/537.36"));
		virtual ~CHttpRequestTool(void);

	public:
		string HttpGet(string url, int timeOut);
		string HttpPost(string url, string postData, list<string> head, int timeOut);

	private:
		static string ConvertAcpToUtf8(const string& strText);
		static string ConvertUtf8ToAcp(const string& strText);

		string ExecuteGetRequest(string strUrl, int timeOut);
		string ExecutePostRequest(string strUrl, string postData, list<string> head, int timeOut);
		void Clear();

	private:
		HINTERNET m_session;
		HINTERNET m_connection;
		HINTERNET m_request;
	};
}

#endif

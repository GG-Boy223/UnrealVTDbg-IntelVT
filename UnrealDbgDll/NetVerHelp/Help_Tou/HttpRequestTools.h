#pragma once
#ifndef CHttpRequestTool_H_
#define CHttpRequestTool_H_
#include <afxinet.h>
#include <string>
#include <list>
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
		/**
		 * @brief				-- Http Get 请求
		 * @param url		-- input,请求的url地址
		 * @param timeOut	-- 接收服务器数据的超时时间
		 * @return				--- 请求的结果
		 */
		string HttpGet(string url, int timeOut);

		/**
		 * @brief				-- Http Post 请求
		 * @param url		-- input,请求的url地址
		 * @param postData	-- Post的内容
		 * @param head	-- 头部
		 * @param timeOut	-- 接收服务器数据的超时时间
		 * @return				--- 请求的结果
		 */
		string HttpPost(string url, string postData, list<string> head, int timeOut);


	private:
		/**
		 * @brief				-- 将url转码为utf_8
		 * @param strText		-- CString reference
		 */
		void ConvertUnicodeToUtf8(CString& strText);

		/**
		 * @brief				-- 将获取到的网页msg转码为Unicode
		 * @param strText		-- string reference
		 */
		void ConvertUtf8ToUnicode(string& strText);

		/**
		 * @brief				-- Http Get 请求
		 * @param strUrl		-- input,请求的url地址
		 * @param timeOut	-- 接收服务器数据的超时时间
		 * @return				-- 请求的结果
		 */
		string ExecuteGetRequest(CString strUrl, int timeOut);

		/**
		 * @brief				-- Http Post 请求
		 * @param strUrl		-- input,请求的url地址
		 * @param m_strPostData	-- Post的内容
		 * @param head	-- 头部
		 * @param timeOut	-- 接收服务器数据的超时时间
		 * @return				--- 请求的结果
		 */
		string ExecutePostRequest(CString strUrl, CString m_strPostData, list<string> head, int timeOut);

		/**
		 * @brief				-- recycle CHttpConnection* and CHttpFile*
		 */
		void Clear();

	private:
		CInternetSession*		m_pSession;
		CHttpConnection*		m_pConnection;
		CHttpFile*				m_pFile;
	};
}
#endif
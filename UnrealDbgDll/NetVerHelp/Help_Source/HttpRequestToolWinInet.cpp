#include "HttpRequestToolWinInetInternal.h"

#include <cstring>

namespace Help
{
	CHttpRequestTool::CHttpRequestTool(string strAgent) :
		m_session(NULL),
		m_connection(NULL),
		m_request(NULL)
	{
		m_session = InternetOpenA(strAgent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	}

	CHttpRequestTool::~CHttpRequestTool(void)
	{
		Clear();

		if (m_session != NULL)
		{
			InternetCloseHandle(m_session);
			m_session = NULL;
		}
	}

	string CHttpRequestTool::HttpGet(string url, int timeOut)
	{
		return ExecuteGetRequest(url, timeOut);
	}

	string CHttpRequestTool::HttpPost(string url, string postData, list<string> head, int timeOut)
	{
		return ExecutePostRequest(url, postData, head, timeOut);
	}

	void CHttpRequestTool::Clear()
	{
		if (m_request != NULL)
		{
			InternetCloseHandle(m_request);
			m_request = NULL;
		}

		if (m_connection != NULL)
		{
			InternetCloseHandle(m_connection);
			m_connection = NULL;
		}
	}

	string CHttpRequestTool::ExecuteGetRequest(string strUrl, int timeOut)
	{
		string strResponse = (const char*)_xor_("");

		if (m_session == NULL)
		{
			return strResponse;
		}

		Clear();
		http_request_tool_wininet::ApplyTimeouts(m_session, timeOut);

		const string utf8Url = ConvertAcpToUtf8(strUrl);

		m_request = InternetOpenUrlA(m_session, utf8Url.c_str(), NULL, 0, http_request_tool_wininet::GetRequestFlags, 1);
		if (m_request != NULL)
		{
			strResponse = http_request_tool_wininet::ReadInternetResponse(m_request);
			strResponse = ConvertUtf8ToAcp(strResponse);
		}

		Clear();
		return strResponse;
	}

	string CHttpRequestTool::ExecutePostRequest(string strUrl, string postData, list<string> head, int timeOut)
	{
		string strResponse = (const char*)_xor_("");

		if (m_session == NULL)
		{
			return strResponse;
		}

		http_request_tool_wininet::ParsedHttpUrl parsed = {};
		if (!http_request_tool_wininet::ParseHttpUrl(strUrl, parsed))
		{
			return strResponse;
		}

		Clear();
		http_request_tool_wininet::ApplyTimeouts(m_session, timeOut);

		const DWORD connectFlags = parsed.secure ? http_request_tool_wininet::SecureConnectFlags : http_request_tool_wininet::NormalConnectFlags;
		m_connection = InternetConnectA(
			m_session,
			parsed.host.c_str(),
			parsed.port,
			NULL,
			NULL,
			INTERNET_SERVICE_HTTP,
			connectFlags,
			1);

		if (m_connection == NULL)
		{
			Clear();
			return strResponse;
		}

		const DWORD requestFlags = parsed.secure ? http_request_tool_wininet::SecureRequestFlags : http_request_tool_wininet::NormalRequestFlags;
		m_request = HttpOpenRequestA(
			m_connection,
			"POST",
			parsed.object.c_str(),
			NULL,
			NULL,
			NULL,
			requestFlags,
			1);

		if (m_request == NULL)
		{
			Clear();
			return strResponse;
		}

		for (const string& item : head)
		{
			HttpAddRequestHeadersA(m_request, item.c_str(), static_cast<DWORD>(item.length()), HTTP_ADDREQ_FLAG_ADD_IF_NEW);
		}

		const string utf8PostData = ConvertAcpToUtf8(postData);
		const char* requestData = utf8PostData.empty() ? "" : utf8PostData.c_str();
		const DWORD requestDataLength = static_cast<DWORD>(strlen(requestData));

		if (HttpSendRequestA(m_request, NULL, 0, const_cast<char*>(requestData), requestDataLength))
		{
			DWORD statusCode = 0;
			DWORD statusCodeLength = sizeof(statusCode);

			if (HttpQueryInfoA(m_request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeLength, NULL) && statusCode == HTTP_STATUS_OK)
			{
				strResponse = http_request_tool_wininet::ReadInternetResponse(m_request);
				strResponse = ConvertUtf8ToAcp(strResponse);
			}
		}

		Clear();
		return strResponse;
	}

}

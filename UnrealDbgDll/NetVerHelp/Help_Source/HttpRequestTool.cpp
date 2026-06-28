
//#define _CRT_SECURE_NO_WARNINGS
#include "../Help_Tou/HttpRequestTools.h"

namespace Help
{
#define BUFFER_SIZE   (4 * 1024)			// 接收数据buffer size
#define NORMAL_CONNECT   INTERNET_FLAG_KEEP_CONNECTION
#define SECURE_CONNECT   NORMAL_CONNECT | INTERNET_FLAG_SECURE
#define NORMAL_REQUEST   INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE
#define SECURE_REQUEST   NORMAL_REQUEST | INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID

	CHttpRequestTool::CHttpRequestTool(string strAgent) :
		m_pConnection(NULL),
		m_pFile(NULL)
	{
		CString agent(strAgent.c_str());
		m_pSession = new CInternetSession(agent);
	}

	CHttpRequestTool::~CHttpRequestTool()
	{
		Clear();

		if (NULL != m_pSession)
		{
			m_pSession->Close();
			delete m_pSession;
			m_pSession = NULL;
		}
	}

	string CHttpRequestTool::HttpGet(string url, int timeOut)
	{
		CString strUrl(url.c_str());
		return ExecuteGetRequest(strUrl, timeOut);
	}

	string CHttpRequestTool::HttpPost(string url, string postData, list<string> head, int timeOut)
	{
		CString strUrl(url.c_str());
		CString m_strPostData(postData.c_str());
		return ExecutePostRequest(strUrl, m_strPostData, head, timeOut);
	}

	void CHttpRequestTool::Clear()
	{
		if (NULL != m_pFile)
		{
			m_pFile->Close(); // recycle
			delete m_pFile;
			m_pFile = NULL;
		}

		if (NULL != m_pConnection)
		{
			m_pConnection->Close();
			delete m_pConnection;
			m_pConnection = NULL;
		}
	}

	string CHttpRequestTool::ExecuteGetRequest(CString strUrl, int timeOut)
	{
		string strResponse = (const char*)_xor_("");
		try
		{
			// 1. 首先得设置个超时标准吧
			m_pSession->SetOption(INTERNET_OPTION_CONNECT_RETRIES, 1000 * 10);// 连接超时
			m_pSession->SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 1000 * 10);// 连接超时

			m_pSession->SetOption(INTERNET_OPTION_SEND_TIMEOUT, 1000 * 10);           //发送超时
			m_pSession->SetOption(INTERNET_OPTION_DATA_SEND_TIMEOUT, 1000 * 10);     //发送超时

			if (timeOut <= 1000 * 30)
			{
				timeOut = 1000 * 30;
			}
			m_pSession->SetOption(INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, timeOut);  // 接收超时
			m_pSession->SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, timeOut);       // 接收超时



			// 2. request to the HTTP server
			ConvertUnicodeToUtf8(strUrl);						// before OpenUrl, convert strUrl coding
			m_pFile = (CHttpFile*)m_pSession->OpenURL(strUrl);
			if (m_pFile)
			{
				char szChars[BUFFER_SIZE + 1] = { 0 };			// buffer
				UINT nReaded = 0;								// The number of bytes transferred to the buffer
				while ((nReaded = m_pFile->Read((void *)szChars, BUFFER_SIZE)) > 0)
				{
					szChars[nReaded] = '\0';
					strResponse += szChars;
				}

				ConvertUtf8ToUnicode(strResponse);				// response context convert coding

				m_pFile->Close();								// recycle
				delete m_pFile;
				m_pFile = NULL;
			}
		}
		catch (CInternetException* e)
		{
			if (m_pFile)
			{
				m_pFile->Close(); // recycle
				delete m_pFile;
				m_pFile = NULL;
			}
		}
		return strResponse;
	}

	string CHttpRequestTool::ExecutePostRequest(CString strUrl, CString	m_strPostData, list<string> head, int timeOut)
	{
		string strResponse = (const char*)_xor_("");

		// 1. 解析URL字符串，并返回服务类型
		DWORD dwServiceType;					// 表名Internet的服务类型，如AFX_INET_SERVICE_HTTP
		CString strServer;						// 解析出来的主机字符串
		CString strObject;						// 解析出来的URL内容字符串
		INTERNET_PORT nPort;					// 端口号（由Server或Object段指定的）

		if (!AfxParseURL(strUrl, dwServiceType, strServer, strObject, nPort))
		{
			// the URL is empty or does not contain a known Internet service type
			return strResponse;
		}
		if (AFX_INET_SERVICE_HTTP != dwServiceType && AFX_INET_SERVICE_HTTPS != dwServiceType)
		{
			// the type of Internet service is illegal
			return strResponse;
		}

		try
		{
			// 2. 设置超时标准,超时时间单位：毫秒
			m_pSession->SetOption(INTERNET_OPTION_CONNECT_RETRIES, 1000 * 10);// 连接超时
			m_pSession->SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 1000 * 10);// 连接超时

			m_pSession->SetOption(INTERNET_OPTION_SEND_TIMEOUT, 1000 * 10);           //发送超时
			m_pSession->SetOption(INTERNET_OPTION_DATA_SEND_TIMEOUT, 1000 * 10);     //发送超时

			if (timeOut <= 1000 * 30)
			{
				timeOut = 1000 * 30;
			}
			m_pSession->SetOption(INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, timeOut);  // 接收超时
			m_pSession->SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, timeOut);       // 接收超时




			// 3. 用来管理HTTP连接，并返回一个CHttpConnection对象
			m_pConnection = m_pSession->GetHttpConnection(strServer, dwServiceType ==
				AFX_INET_SERVICE_HTTP ? NORMAL_CONNECT : SECURE_CONNECT, nPort);


			// 4. 用来打开HTTP连接，并返回一个指向CHttpFile对象的指针_T("POST")
			m_pFile = m_pConnection->OpenRequest(CHttpConnection::HTTP_VERB_POST, strObject, NULL, 1, NULL, NULL,
				(dwServiceType == AFX_INET_SERVICE_HTTP ? NORMAL_REQUEST : SECURE_REQUEST));

			// 5. 给请求添加Headers
			list<string>::iterator it;
			for (it = head.begin(); it != head.end(); it++)
			{
				string temp = *it;
				CString headTemp(temp.c_str());
				m_pFile->AddRequestHeaders(headTemp);
			}


			// 6. 向HTTP服务器发送请求,需要将参数转码utf_8
			if (m_strPostData.IsEmpty())
			{
				m_pFile->SendRequest(NULL, 0, (LPVOID)"", 0);
			}
			else
			{
				int nLen = WideCharToMultiByte(CP_UTF8, 0, m_strPostData, -1, NULL, 0, NULL, NULL);
				char *pBuf = new char[nLen + 1];
				if (NULL != pBuf)
				{
					WideCharToMultiByte(CP_UTF8, 0, m_strPostData, -1, pBuf, nLen, NULL, NULL);
					m_pFile->SendRequest(NULL, 0, (LPVOID)pBuf, strlen(pBuf));

					delete[] pBuf;
					pBuf = NULL;
				}
			}

			// 7. 获取HTTP请求的状态码
			DWORD dwStatusCode = 0L;
			m_pFile->QueryInfoStatusCode(dwStatusCode);
			if (HTTP_STATUS_OK == dwStatusCode)
			{
				char szChars[BUFFER_SIZE + 1] = { 0 };			// buffer
				UINT nReaded = 0;								// The number of bytes transferred to the buffer
				while ((nReaded = m_pFile->Read((void *)szChars, BUFFER_SIZE)) > 0)
				{
					strResponse += szChars;
					::ZeroMemory(szChars, BUFFER_SIZE + 1);
				}

				ConvertUtf8ToUnicode(strResponse); // response context convert coding
			}

		}
		catch (CInternetException *e)
		{
			DWORD dwErrorCode = e->m_dwError;
			DWORD_PTR dwContext = e->m_dwContext;
			e->Delete();

			// write to log
			/*CString strLog;
			strLog.Format(_T("InternetException m_dwContext:%u m_dwError:%u"), dwContext, dwErrorCode);*/
		}

		Clear();
		return strResponse;
	}

	void CHttpRequestTool::ConvertUnicodeToUtf8(CString& strText)
	{
		int nLen = WideCharToMultiByte(CP_UTF8, 0, strText, -1, NULL, 0, NULL, NULL);
		char *pBuf = new char[nLen + 1];
		if (NULL != pBuf)
		{
			WideCharToMultiByte(CP_UTF8, 0, strText, -1, pBuf, nLen, NULL, NULL);
			pBuf[nLen] = '\0';
			strText = pBuf;

			delete[] pBuf;
			pBuf = NULL;
		}
	}

	void CHttpRequestTool::ConvertUtf8ToUnicode(string& strText)
	{
		int unicodeLen = MultiByteToWideChar(CP_UTF8, 0, strText.c_str(), -1, NULL, 0);
		WCHAR *pUnicode = new WCHAR[unicodeLen + 1];
		if (NULL != pUnicode)
		{
			MultiByteToWideChar(CP_UTF8, 0, strText.c_str(), -1, pUnicode, unicodeLen);
			pUnicode[unicodeLen] = '\0';

			USES_CONVERSION;
			strText = T2A(pUnicode);

			delete[] pUnicode;
			pUnicode = NULL;
		}
	}
}
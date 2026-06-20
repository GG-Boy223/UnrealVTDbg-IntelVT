#include "../dllmain.h"
#include "Log.h"

void _outDebug(const TCHAR* sText)
{
	if (sText)
	{
		OutputDebugString(sText);
		OutputDebugString(_T("\n"));
	}
}

int outDebug(const TCHAR* _Format, ...)
{
	int iRet;
	va_list list;
	TCHAR SzBuf[1024] = { 0 };
	va_start(list, _Format);
	iRet = vswprintf(SzBuf, _countof(SzBuf), _Format, list);
	_outDebug(SzBuf);
	va_end(list);
	return iRet;
}

void ReportSeriousError(LPCSTR lpText)
{
	if (!lpText)
	{
		lpText = "unknown hook error";
	}

	logger.Log("严重错误: %s", lpText);
	OutputDebugStringA("[Hook] ");
	OutputDebugStringA(lpText);
	OutputDebugStringA("\n");
}

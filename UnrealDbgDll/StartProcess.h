#pragma once

#ifndef _START_PROCESS_H

//启动信息
typedef struct _STARTUP_INFO
{
	TCHAR szExe[256];
	TCHAR sPath[256];
}STARTUP_INFO, * PSTARTUP_INFO;

BOOL StartProcess(TCHAR* szExe, TCHAR* sPath);

//根据光标位置获取进程pid
DWORD GetProcessId_ByCursor();

//下发调试器信息给驱动
BOOL SendDebuggerDataToDriver(DWORD dwProcessId);


#endif // !_START_PROCESS_H

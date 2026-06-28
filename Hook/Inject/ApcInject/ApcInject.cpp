#include "../../dllmain.h"
#include "../../Globals.h"
#include "../../Channels/DispatchData.h"
#include "../../NativeApi/NtdllResolver.h"
#include "ApcInject.h"

void ApcCallRemoteFunc(HANDLE hProcess)
{
	if (!_ApcCallRemoteFunc(hProcess))
	{
		logger.Log("APC注入线程失败");
	}
	// *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
	else
	{
		logger.Log("APC注入成功");
	}
}

//通过驱动注入apc线程到目标进程
BOOL _ApcCallRemoteFunc(HANDLE hProcess)
{
	BOOL bRet;
	DWORD BytesReturned = 0;
	RING3_REMOTE_THREAD Info = { 0 };
	Info.hProcess = (ULONG64)hProcess;
	Info.pUserFunc = ResolveDbgUserBreakPointAddress();
	if (Info.pUserFunc == 0)
	{
		logger.Log("DbgUserBreakPoint空指针");
		return FALSE;
	}
	bRet = SendUserDataToDriver(IOCTL_CREATE_REMOTE_THREAD, &Info, sizeof(RING3_REMOTE_THREAD), NULL, 0, &BytesReturned);
	return bRet;
}

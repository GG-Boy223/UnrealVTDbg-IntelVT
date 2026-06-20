#include <Windows.h>
#include "detours.h"
#include "Hook.h"


#ifdef _WIN64
#pragma comment(lib,"../Common/Detours/x64/detours.lib")
#else
#pragma comment(lib,"../Common/Detours/x86/detours.lib")
#endif // _WIN64

//安装hook
void HookOn(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread)
{
	//修改目标内存页保护属性
	DetourTransactionBegin();
	//暂停目标线程
	DetourUpdateThread(hThread);
	//开始hook
	DetourAttach(pfun, proxy_fun);
	//提交执行
	DetourTransactionCommit();
}

//卸载hook
void HookOff(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread)
{
	//修改目标内存页保护属性
	DetourTransactionBegin();
	//暂停目标线程
	DetourUpdateThread(hThread);
	//卸载hook
	DetourDetach(pfun, proxy_fun);
	//提交执行
	DetourTransactionCommit();
}
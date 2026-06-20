#include "../../Driver.h"
#include "../../ntos/inc/extypes.h"
#include "../../ntos/inc/ketypes.h"
#include "../../ntos/inc/ntosdef.h"
#include "../../ntos/inc/amd64.h"
#include "../../ntos/inc/mi.h"
#include "../../ntos/inc/pstypes.h"
#include "../../ntos/inc/obtypes.h"
#include "../../ntos/inc/mmtypes.h"
#include "../../ntos/inc/ntdbg.h"
#include "../../ntos/inc/peb_teb.h"
#include "../../List/MyList.h"
#include "../../ntos/inc/ntlpcapi.h"
#include "../../ntos/inc/psp.h"
#include "../../Globals.h"
#include "../../DbgkApi/DbgkApi.h"
#include "../../Log/log.h"
#include "../../Hooks/EptHook/EptHook.h"
#include "../../Encrypt/Blowfish/Blowfish.h"
#include "../../Hvm/vmcall_reason.h"
#include "../../Hvm/hypervisor_gateway.h"
#include "InitSymbolic.h"
#include "InitWin32kfull.h"


BOOLEAN InitWin32kfullSymbolsTable()
{
    //当前线程必须是gui线程才能访问win32k的内存
    symbolic_access::ModuleExtenderFactory extenderFactory{};
    const auto& moduleExtender = extenderFactory.Create(L"win32kfull.sys");
    if (!moduleExtender.has_value())
    {
        outLog("win32kfull.sys 符号初始化失败..");
        return FALSE;
    }

    Sys_NtUserFindWindowEx = (PFN_NTUSERFINDWINDOWEX)moduleExtender->GetPointer<PFN_NTUSERFINDWINDOWEX>("NtUserFindWindowEx");
    Sys_NtUserWindowFromPoint = (PFN_NTUSERWINDOWFROMPOINT)moduleExtender->GetPointer<PFN_NTUSERWINDOWFROMPOINT>("NtUserWindowFromPoint");

    return TRUE;
}

bool ShouldAllowAccess(HWND hwnd)
{
    if (DbgkIsDriverRuntimeUnloading())
    {
        return true;
    }

    if (hwnd)
    {
        // 根据窗口句柄获得窗口对象
        PWND pwnd = Original_ValidateHwnd(hwnd);
        if (pwnd)
        {
            // 判断当前调用者是否是调试器进程
            if (!IsDebugger(PsGetCurrentProcess()))
            {
                // 根据线程对象获得进程对象
                PEPROCESS Process = PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);

                // 判断要访问的目标窗口是否是我们调试器的窗口
                if (IsDebugger(Process))
                {
                    WCHAR SubStr[256] = { 0 };
                    UNICODE_STRING ImageFileName, PassImage;
                    NTSTATUS Status = GetProcessName(PsGetCurrentProcess(), &SubStr[0]);
                    if (NT_SUCCESS(Status))
                    {
                        RtlInitUnicodeString(&ImageFileName, SubStr);
                        for (ULONG i = 0; i < sizeof(PassProcessList) / sizeof(PassProcessList[0]); i++)
                        {
                            RtlInitUnicodeString(&PassImage, PassProcessList[i]);
                            if (RtlEqualUnicodeString(&ImageFileName, &PassImage, TRUE))
                            {
                                // 发现是白名单进程就放行
                                return true;
                            }
                        }
                        // 如果访问的是我们调试器的窗口，则拒绝访问。
                        //PrintProcessName((_EPROCESS*)PsGetCurrentProcess());
                        return false;
                    }
                }
            }
        }
    }
    return true; // 默认放行
}

//API FindWindowA/W, FindWindowExA/W
HWND NewNtUserFindWindowEx(IN HWND hwndParent,
    IN HWND hwndChild,
    IN PUNICODE_STRING pstrClassName,
    IN PUNICODE_STRING pstrWindowName)
{
    DbgkRuntimeCallGuard runtimeGuard;
    HWND hwnd = Original_NtUserFindWindowEx(hwndParent, hwndChild, pstrClassName, pstrWindowName);

    if (!ShouldAllowAccess(hwnd))
    {
        hwnd = NULL;
    }

    return hwnd;
}

//API WindowFromPoint
HWND NewNtUserWindowFromPoint(POINT Point)
{
    DbgkRuntimeCallGuard runtimeGuard;
    HWND hwnd = Original_NtUserWindowFromPoint(Point);

    if (!ShouldAllowAccess(hwnd))
    {
        hwnd = NULL;
    }

    return hwnd;
}
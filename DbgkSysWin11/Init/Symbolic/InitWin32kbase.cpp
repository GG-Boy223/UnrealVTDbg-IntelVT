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
#include "InitSymbolic.h"
#include "InitWin32kbase.h"
#include "../../Encrypt/Blowfish/Blowfish.h"
#include "../../Hvm/vmcall_reason.h"
#include "../../Hvm/hypervisor_gateway.h"

BOOLEAN InitWin32kbaseSymbolsTable()
{
    //当前线程必须是gui线程才能访问win32k的内存
    symbolic_access::ModuleExtenderFactory extenderFactory{};
    const auto& moduleExtender = extenderFactory.Create(L"win32kbase.sys");
    if (!moduleExtender.has_value())
    {
        outLog("win32kbase.sys 符号初始化失败..");
        return FALSE;
    }

    Sys_ValidateHwnd = (PFN_VALIDATEHWND)moduleExtender->GetPointer<PFN_VALIDATEHWND>("ValidateHwnd");

    return TRUE;
}


PWND NewValidateHwnd(_In_ HWND hwnd)
{
    DbgkRuntimeCallGuard runtimeGuard;
    //根据窗口句柄获得窗口对象
    PWND pwnd = Original_ValidateHwnd(hwnd);
    if (DbgkIsDriverRuntimeUnloading())
    {
        return pwnd;
    }

    if (pwnd)
    {
        //判断当前调用者是否是调试器进程
        if (!IsDebugger(PsGetCurrentProcess()))
        {
            //根据线程对象获得进程对象
            PEPROCESS Process = PsGetThreadProcess((PETHREAD)pwnd->head.pti->pEThread);

            //判断要访问的目标窗口是否是我们调试器的窗口
            if (IsDebugger(Process))
            {
                //获取当前调用进程
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
                            //发现是白名单进程就放行
                            goto pass;
                        }
                    }

                    //如果访问的是我们调试器的窗口，则拒绝访问。
                    pwnd = NULL;

                    //PrintProcessName((_EPROCESS*)PsGetCurrentProcess());
                }
            }
        }
    }
pass:
    return pwnd;
}
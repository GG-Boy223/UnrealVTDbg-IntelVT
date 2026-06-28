#include "DebugEventInternal.h"

BOOL
WINAPI
NewWaitForDebugEvent(
    __in LPDEBUG_EVENT lpDebugEvent,
    __in DWORD dwMilliseconds
)
{
    BOOL bRet = FALSE;
    BOOL bFlag;
    DWORD dwOldProtect;
    DWORD error = 0;

    if (lpDebugEvent)
    {
        bRet = Sys_WaitForDebugEvent(lpDebugEvent, dwMilliseconds);
        if (bRet)
        {
            if (lpDebugEvent->dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
            {
                CleanupDebugSessionState("EXIT_PROCESS_DEBUG_EVENT");
                return bRet;
            }

            //处理非附加调试的情况
            if (g_process_info.isCreate)
            {
                //Debug event
                switch (lpDebugEvent->dwDebugEventCode)
                {
                case LOAD_DLL_DEBUG_EVENT:
                {
                    if (!g_SetDbgBreakPoint.boBaseThreadInitThunk)
                    {
                        assert(BaseThreadInitThunk);
                        if (BaseThreadInitThunk)
                        {
                            //设置int3中断
                            UCHAR chBuffer[3] = { 0x90,0xCC,0xEB };
                            PVOID BreakPointAddr = (PVOID)((ULONG_PTR)BaseThreadInitThunk + 4);
                            bFlag = VirtualProtectEx(g_process_info.ProcessHandle, BreakPointAddr, sizeof(chBuffer), PAGE_EXECUTE_READWRITE, &dwOldProtect);
                            if (bFlag)
                            {
                                bFlag = WriteProcessMemory(g_process_info.ProcessHandle, BreakPointAddr, &chBuffer, sizeof(chBuffer), NULL);
                                if (!bFlag)
                                {
                                    error = GetLastError();
                                    outDebug((TCHAR*)_T("[LOAD_DLL_DEBUG_EVENT] 设置断点失败！(error:%d)"), error);
                                }
                                // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
                                else
                                {
                                    outDebug((TCHAR*)_T("非附加调试__设置int3中断成功！"));
                                    //outDebug((TCHAR*)_T("设置int3中断成功！"));
                                    g_SetDbgBreakPoint.boBaseThreadInitThunk = TRUE;
                                }
                                VirtualProtectEx(g_process_info.ProcessHandle, BreakPointAddr, sizeof(chBuffer), dwOldProtect, &dwOldProtect);
                            }
                            // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
                            else
                            {
                                error = GetLastError();
                                outDebug((TCHAR*)_T("[LOAD_DLL_DEBUG_EVENT] 修改内存属性失败！(error:%d)"), error);
                            }
                        }
                        // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
                        else
                        {
                            error = GetLastError();
                            outDebug((TCHAR*)_T("[LOAD_DLL_DEBUG_EVENT] BaseThreadInitThunk空指针！(error:%d)"), error);
                        }
                    }
                    break;
                }
                case EXCEPTION_DEBUG_EVENT:
                {
                    if ((lpDebugEvent->u.Exception.ExceptionRecord.ExceptionAddress == (PVOID)((ULONG_PTR)BaseThreadInitThunk + 5)) &&
                        (lpDebugEvent->u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT))
                    {
                        g_first_breakpoint = TRUE;
                    }
                    break;
                }
                }
            }
            // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
            else
            {
                //处理附加的情况
                if (lpDebugEvent->dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
                {
                    const DWORD exceptionCode = lpDebugEvent->u.Exception.ExceptionRecord.ExceptionCode;
                    if (DebugEventInternal::IsSingleStepExceptionCode(exceptionCode))
                    {
                        DebugEventInternal::CaptureSingleStepDebugState(lpDebugEvent->dwThreadId);
                    }
                }
            }
        }
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        outDebug((TCHAR*)_T("WaitForDebugEvent参数无效！"));
    }
    return bRet;
}

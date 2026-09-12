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
                    if (!g_SetDbgBreakPoint.boBaseThreadInitThunk &&
                        !g_SetDbgBreakPoint.BaseThreadInitThunkPatched)
                    {
                        assert(BaseThreadInitThunk);
                        if (BaseThreadInitThunk)
                        {
                            //设置int3中断
                            UCHAR chPatchedBytes[3] = { 0x90, 0xCC, 0xEB };
                            PVOID BreakPointAddr = (PVOID)((ULONG_PTR)BaseThreadInitThunk + 4);
                            bFlag = VirtualProtectEx(g_process_info.ProcessHandle, BreakPointAddr, sizeof(chPatchedBytes), PAGE_EXECUTE_READWRITE, &dwOldProtect);
                            if (bFlag)
                            {
                                bFlag = ReadProcessMemory(
                                    g_process_info.ProcessHandle,
                                    BreakPointAddr,
                                    g_SetDbgBreakPoint.BaseThreadInitThunkOriginalBytes,
                                    sizeof(g_SetDbgBreakPoint.BaseThreadInitThunkOriginalBytes),
                                    NULL);
                                if (!bFlag)
                                {
                                    error = GetLastError();
                                    outDebug((TCHAR*)_T("[LOAD_DLL_DEBUG_EVENT] 读取断点原始字节失败！(error:%d)"), error);
                                }
                                // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
                                else
                                {
                                    bFlag = WriteProcessMemory(
                                        g_process_info.ProcessHandle,
                                        BreakPointAddr,
                                        &chPatchedBytes,
                                        sizeof(chPatchedBytes),
                                        NULL);
                                    if (!bFlag)
                                    {
                                        error = GetLastError();
                                        outDebug((TCHAR*)_T("设置断点失败!(error:%d)"), error);
                                    }
                                    else
                                    {
                                        outDebug((TCHAR*)_T("非附加调试__设置int3中断成功!"));
                                        g_SetDbgBreakPoint.BaseThreadInitThunkPatched = TRUE;
                                    }
                                }
                                VirtualProtectEx(g_process_info.ProcessHandle, BreakPointAddr, sizeof(chPatchedBytes), dwOldProtect, &dwOldProtect);
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

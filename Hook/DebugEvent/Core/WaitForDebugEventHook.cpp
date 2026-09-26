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

            // 单步/硬件断点命中状态捕获必须对「创建调试」与「附加调试」两条路径都执行。
            // 依据：Hook/DebugBreak/Core/ThreadContextHooks.cpp 的 NewGetThreadContext 在合成
            // 返回给调试器的 DR6 时，依赖这里写入的 DEBUG_HIT_RECORD 来区分三种情形 ——
            //   条件 1 + 寄存器状态位 = 硬件断点命中（DR6.B0-B3）
            //   条件 2                  = TF 单步完成（DR6.BS）
            // 原实现只在附加路径（else 分支）调用，导致创建路径下纯单步拿不到 BS 位，
            // 调试器读到的 DR6 在两条路径下不一致（x64dbg 的 File -> Open 走
            // NewNtCreateUserProcess -> g_process_info.isCreate = TRUE，见 ProcessApiHooks.cpp:25）。
            if (lpDebugEvent->dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
                DebugEventInternal::IsSingleStepExceptionCode(
                    lpDebugEvent->u.Exception.ExceptionRecord.ExceptionCode))
            {
                DebugEventInternal::CaptureSingleStepDebugState(lpDebugEvent->dwThreadId);
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
            // 附加路径的调试事件已在上方统一处理单步状态捕获，这里不再重复。
        }
    }
    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
    else
    {
        outDebug((TCHAR*)_T("WaitForDebugEvent参数无效！"));
    }
    return bRet;
}

#include "DebugEventInternal.h"

BOOL
WINAPI
NewContinueDebugEvent(
    _In_ DWORD dwProcessId,
    _In_ DWORD dwThreadId,
    _In_ DWORD dwContinueStatus
)
{
    BOOL bFlag;
    DWORD dwOldProtect;
    DWORD error = 0;

    //处理非附加调试的情况
    if (g_process_info.isCreate)
    {
        if (dwContinueStatus == DBG_CONTINUE)
        {
            if (g_first_breakpoint)
            {
                g_first_breakpoint = FALSE;
                assert(BaseThreadInitThunk);
                if (BaseThreadInitThunk)
                {
                    UCHAR chOldBytes[3] = { 0 };
                    PVOID BreakPointAddr = (PVOID)((ULONG_PTR)BaseThreadInitThunk + 4);
                    bFlag = VirtualProtectEx(g_process_info.ProcessHandle, BreakPointAddr, sizeof(chOldBytes), PAGE_EXECUTE_READWRITE, &dwOldProtect);
                    if (bFlag)
                    {
                        bFlag = ReadProcessMemory(g_process_info.ProcessHandle, BreakPointAddr, &chOldBytes, sizeof(chOldBytes), NULL);
                        if (bFlag)
                        {
                            if (chOldBytes[1] == 0xCC)
                            {
                                UCHAR chBuffer[3] = { 0x85,0xC9,0x75 };
                                bFlag = WriteProcessMemory(g_process_info.ProcessHandle, BreakPointAddr, &chBuffer, sizeof(chBuffer), NULL);
                                if (!bFlag)
                                {
                                    error = GetLastError();
                                    outDebug((TCHAR*)_T("[EXCEPTION_DEBUG_EVENT] 移除断点失败！(error:%d)"), error);
                                }
                            }
                            VirtualProtectEx(g_process_info.ProcessHandle, BreakPointAddr, sizeof(chOldBytes), dwOldProtect, &dwOldProtect);
                        }
                        // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
                        else
                        {
                            error = GetLastError();
                            outDebug((TCHAR*)_T("[EXCEPTION_DEBUG_EVENT] 检查内存时失败！(error:%d)"), error);
                        }
                    }
                    // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
                    else
                    {
                        error = GetLastError();
                        outDebug((TCHAR*)_T("[EXCEPTION_DEBUG_EVENT] 修改内存属性失败！(error:%d)"), error);
                    }
                }
                // *rch = (*rch << 4) + (*(szHex + i) - 'A' + 10);
                else
                {
                    error = GetLastError();
                    outDebug((TCHAR*)_T("[EXCEPTION_DEBUG_EVENT] BaseThreadInitThunk空指针！(error:%d)"), error);
                }
            }
        }
    }
    // CE/x64dbg 的手动 F5 软件断点必须保持到用户再次 F5/删除。
    // 这里不能在 Continue 时主动删除隐藏 INT3；真正删除只由写回原字节路径处理。
    const BOOL continueResult = Sys_ContinueDebugEvent(dwProcessId, dwThreadId, dwContinueStatus);
    if (continueResult)
    {
        DebugEventInternal::ClearDebugHitRecord(dwThreadId);
    }
    return continueResult;
}

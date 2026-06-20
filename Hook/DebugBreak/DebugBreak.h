#pragma once

#ifndef _DEBUG_BREAK_H
#define _DEBUG_BREAK_H

//Vol3B[18.2.4 Debug Control Register(DR7)]
#define WATCH_EXECUTION_ONLY   0
#define WATCH_WRITE            1
#define WATCH_READWRITE        3

#define BYTE_1  0
#define BYTE_2  1
#define BYTE_8  2
#define BYTE_4  3

#define BREAKPOINT_COUNT  4

BOOL
WINAPI
NewSetThreadContext(
    _In_ HANDLE hThread,
    _In_ CONTEXT* lpContext
);

BOOL WINAPI NewGetThreadContext(
    _In_ HANDLE    hThread,
    _Inout_ LPCONTEXT lpContext
);

//添加断点
bool AddBreakpoint(PVOID setAddress, unsigned __int64 command, int length, int debugRegister, int debugRw, int debugLen);
bool RemoveBreakpoint();

//我们应该将地址直接传给vt
//并在vt里修改ept pte页表属性
bool SetBreakpoint(PVOID lpBaseAddress, unsigned __int64 command, int length, int debugRegister, int debugRw, int debugLen);


#endif // !_DEBUG_BREAK_H

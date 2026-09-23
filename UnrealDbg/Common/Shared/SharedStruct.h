/**
 * 注意此头文件是ring3 ring0共享的，不能使用自适应的数据类型，例如size_t等数据类型作为变量的数据类型
 * 如果这样做会导致在编译ring3 x86程序时，size_t自适应类型长度会是对应x86平台的4字节，而在64位操作系统上
 * 驱动程序是64位的，size_t自适应类型长度则是8字节。这会导致两边的结构体大小不匹配
 */

#pragma once

#ifndef _SHARED_STRUCT_H
#define _SHARED_STRUCT_H

#define SYMBOLICLINK ((TCHAR*)_T("\\\\.\\UnrealDbg"))

#define PATH_SIZE 260
#define PTR64(x) ULONG64

#define PROTECT_FILE           1
#define PROTECT_WINDOW         2

typedef struct _MY_CLIENT_ID
{
    PTR64(HANDLE) UniqueProcess;
    PTR64(HANDLE) UniqueThread;
} MY_CLIENT_ID, * PMY_CLIENT_ID;

typedef struct _USER_DATA
{
    ULONG Count;  //用户队列的最大计数
	ULONG uSize;  //数据大小
	PTR64(PVOID) pUserData;  //结构体指针
}USER_DATA, * PUSER_DATA;

typedef struct _USER_FUNCTION_RECORD
{
    WCHAR programName[100];      //进程名
    WCHAR functionName[100];     //函数名
    unsigned __int64 params[16];        //参数列表    
    unsigned __int64 retAddr;           //返回地址
    ULONG uPid;                  //进程id
    ULONG uTid;                  //线程id
}USER_FUNCTION_RECORD, * PUSER_FUNCTION_RECORD;

typedef struct _PROTECT_TABLE_ENTRY {
    unsigned long dwProcessId;
    unsigned long dwThreadId;
}PROTECT_TABLE_ENTRY, * PPROTECT_TABLE_ENTRY;

#pragma pack(push, 1)
typedef struct _CHECK_VT_TABLE_ENTRY {
    bool boCheckVt;
    unsigned long number;  //记录是哪个函数检测到的
}CHECK_VT_TABLE_ENTRY, * PCHECK_VT_TABLE_ENTRY;
#pragma pack(pop)

typedef struct _SYMBOLICLINK_TABLE_ENTRY {
    WCHAR Symboliclink[64];
}SYMBOLICLINK_TABLE_ENTRY, * PSYMBOLICLINK_TABLE_ENTRY;

typedef struct _SYMBOL_TABLE
{
    unsigned __int64 lpDbgUiRemoteBreakin;
    unsigned __int64 lpDbgBreakPoint;
}SYMBOL_TABLE, * PSYMBOL_TABLE;

typedef struct _RING3_DEBUGGER_TABLE_ENTRY
{
    unsigned long dwPid;          //调试器的进程id
    PTR64(TCHAR*) fileData;  //文件信息
    PTR64(TCHAR*) fileData2;  //文件信息
}RING3_DEBUGGER_TABLE_ENTRY, * PRING3_DEBUGGER_TABLE_ENTRY;

typedef struct _RING3_TL_GAME_TABLE_ENTRY
{
    unsigned long dwPid;          //game进程id
}RING3_TL_GAME_TABLE_ENTRY, * PRING3_TL_GAME_TABLE_ENTRY;

typedef struct _RING3_REMOTE_THREAD
{
    PTR64(HANDLE) pid;  //要注入线程的目的进程pid
    PTR64(HANDLE) hProcess;  //目标进程句柄
    PTR64(PVOID) pUserFunc;  //要执行的函数
}RING3_REMOTE_THREAD, * PRING3_REMOTE_THREAD;

//调试器状态
typedef struct _DEBUGGER_STATE
{
    PTR64(HANDLE) ProcessHandle;  //要调试的目标进程
    PTR64(HANDLE) DebugObjectHandle;  //调试对象句柄
}DEBUGGER_STATE, * PDEBUGGER_STATE;

//断点描述符表
typedef struct _BREAKPOINT_RECORD {
    unsigned __int64 command;  //此成员必须放在结构体的第一位
    ULONG64 cr3; //目标进程的cr3
    PTR64(PVOID) Address; //断点地址
    int length;
    int watchid;  //监视id
    int CPUCount;  //逻辑处理器数量
    unsigned __int64 LoopUserMode;  //用户模式的int3
    unsigned long pid; //进程pid
    int debugRegister; //来源 DR0-DR3 槽位，供 ring3 回填调试上下文
    int debugRw; //DR7 RWn 编码
    int debugLen; //DR7 LENn 编码
}BREAKPOINT_RECORD, * PBREAKPOINT_RECORD;

typedef struct _VMCALLINFO {
    unsigned __int64 command;
}VMCALLINFO,*PVMCALLINFO;

//虚拟化断点
typedef struct _VT_BREAK_POINT {
    unsigned __int64 command;  //此成员必须放在结构体的第一位
    int Size;  //断点长度
    unsigned __int64 VirtualAddress;  //断点地址
    unsigned __int64 PhysicalAddress;
    unsigned __int64 cr3; //目标进程的cr3    
    unsigned __int64 LoopUserMode;  //用户模式的int3
    int CPUCount;  //逻辑处理器数量
    int watchid;  //监视id
    int errorCode; //错误码
    unsigned char OriginalBytes; //原字节
    unsigned char buffer[16];
    unsigned long pid; //进程pid
    int debugRegister; //来源 DR0-DR3 槽位，供驱动命中后回填 DR6
}VT_BREAK_POINT, * PVT_BREAK_POINT;

typedef struct _BREAKPOINT_DETECTED {
    unsigned __int64 command;
    MY_CLIENT_ID Cid;
    unsigned __int64 breakpoint_detected; //记录触发的地址
    int debugRegister; //命中的 DR0-DR3 槽位，-1 表示无效
}BREAKPOINT_DETECTED,*PBREAKPOINT_DETECTED;

typedef struct _RING3_PROCESS_INFO {
    unsigned __int64 cr3; //目标进程的cr3 
    PTR64(HANDLE) ProcessHandle;
}RING3_PROCESS_INFO,*PRING3_PROCESS_INFO;

typedef struct _RING3_PROTECT_OBJECT
{
    unsigned long dataType;          //数据类型
    PTR64(TCHAR*) fileData;  //文件信息
}RING3_PROTECT_OBJECT, * PRING3_PROTECT_OBJECT;

typedef struct _WINDOWS_STRUCT {
    unsigned __int64 command;
    unsigned __int64 ethread_offset_Cid;
}WINDOWS_STRUCT,*PWINDOWS_STRUCT;

typedef struct _RING3_VERIFY
{
    unsigned __int64 key;
}RING3_VERIFY, * PRING3_VERIFY;


#endif // !_SHARED_STRUCT_H


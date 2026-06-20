#include "dllmain.h"
#include "HookCallSet/functionSet.h"
#include "Globals.h"

DWORD g_dwNumberOfProcessors;  //逻辑处理器数量
LONG g_debug_condition_detected;  //记录是TF单步执行，还是drx断点
LONG g_debug_register_status;  //记录本次#DB触发的DR6低4位
LONG g_active_hardware_breakpoint_count;  //当前由VT接管的硬件断点数量
DWORD g_target_pid;  //目标进程的pid
ULONG64 g_target_cr3; //目标进程的cr3
vectorExt<BREAKPOINT_RECORD> BreakpointList;
vectorExt<VT_BREAK_POINT> INT3BreakpointList;
vectorExt<DEBUG_HIT_RECORD> DebugHitList;
HANDLE g_hGeneralDriverDevice = INVALID_HANDLE_VALUE;
BOOL g_first_breakpoint = FALSE;  //是否是第一次断点
PROCESS_INFO g_process_info = { 0 };
SET_DBG_BREAKPOINT g_SetDbgBreakPoint = { 0 };

PFN_LDRINITIALIZETHUNK LdrInitializeThunk;
PVOID BaseThreadInitThunk;
PVOID KiUserApcDispatcher;

//注意事项: 当我们hook 系统的函数转到我们自定义的函数里时
//我们自定义的函数所采用的调用约定必须设置为__stdcall
//因为如果将hook项目编译为32位dll时是需要严格的函数调用约定的
//否则可能会导致栈不平衡
PFN_DEBUGACTIVEPROCESS Sys_DebugActiveProcess;
PFN_DEBUGACTIVEPROCESSSTOP Sys_DebugActiveProcessStop;
PFN_TERMINATEPROCESS Sys_TerminateProcess;
PFN_NTDEBUGACTIVEPROCESS Sys_NtDebugActiveProcess;
PFN_NTREMOVEPROCESSDEBUG Sys_NtRemoveProcessDebug;
PFN_NTTERMINATEPROCESS Sys_NtTerminateProcess;
PFN_DBGUIISSUEREMOTEBREAKIN Sys_DbgUiIssueRemoteBreakin;
PFN_NTCREATEUSERPROCESS Sys_NtCreateUserProcess;
PFN_WAITFORDEBUGEVENT Sys_WaitForDebugEvent;
PFN_CONTINUEDEBUGEVENT Sys_ContinueDebugEvent;
PFN_OUTPUTDEBUGSTRINGA Sys_OutputDebugStringA;
PFN_OUTPUTDEBUGSTRINGW Sys_OutputDebugStringW;
PFN_DBGUIDEBUGACTIVEPROCESS Sys_DbgUiDebugActiveProcess;
PFN_SETTHREADCONTEXT Sys_SetThreadContext;
PFN_GETTHREADCONTEXT Sys_GetThreadContext;
PFN_VIRTUALPROTECTEX Sys_VirtualProtectEx;
PFN_WRITEPROCESSMEMORY Sys_WriteProcessMemory;
PFN_READPROCESSMEMORY Sys_ReadProcessMemory;
PFN_NTDEBUGCONTINUE Sys_NtDebugContinue;
PFN_NTWRITEVIRTUALMEMORY Sys_NtWriteVirtualMemory;
PFN_NTREADVIRTUALMEMORY Sys_NtReadVirtualMemory;

#pragma once
#include "../../Driver.h"
#include "../../Globals.h"  // 包含 PDEBUG_OBJECT, PDEBUG_PROCESS, PDEBUGGER_TABLE_ENTRY 等类型定义

// 调试对象初始化/反初始化
NTSTATUS DbgkInitialize();
VOID DbgkUnInitialize();

// 调试进程列表管理
BOOLEAN InsertDebuggerList(PDEBUGGER_TABLE_ENTRY Debugger);
VOID DeleteDebugProcess(PDEBUG_OBJECT DebugObject);

// 调试目标进程操作
BOOLEAN IsDebugTargetProcess(IN _EPROCESS* Process, OUT PDEBUG_PROCESS* DebugProcess);

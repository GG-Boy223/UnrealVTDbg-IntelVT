#include <ntifs.h>
#include <ntstrsafe.h>
#include "Types/ApcInjectNativeSupportTypes.h"

// OS Dependant data
DYNAMIC_DATA dynData;

// 实现按职责在 Core/ 目录下拆分：
// - ApcQueue.cpp / ThreadLookup.cpp：APC 注入、APC 回调、线程查找
// - ShellcodeBuilder.cpp：x64 Shellcode 生成
// - UserModuleLookup.cpp / ApiSetResolver.cpp / ImagePathResolver.cpp / SxsResolver.cpp：模块路径解析、API Set、SxS
// - ExportResolver.cpp / RemoteRoutineCall.cpp：导出查找和远程例程调用
// - StringUtilities.cpp / KernelResolver.cpp / ThreadExecution.cpp：字符串工具、内核基址、SSDT、模式搜索、工作执行
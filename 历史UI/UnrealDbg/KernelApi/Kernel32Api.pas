unit Kernel32Api;                       // 单元名称：Kernel32Api

interface                               // 接口部分开始

uses Winapi.Windows;                    // 引入 Windows 单元，提供基本类型与常数

const                                   // 常数定义区
  THREAD_SUSPEND_RESUME = $2;           // 线程访问权限：挂起/恢复线程
  LIST_MODULES_32BIT = $1;              // 枚举模块标志：仅 32 位模块
  LIST_MODULES_64BIT = $2;              // 枚举模块标志：仅 64 位模块
  LIST_MODULES_ALL = $3;                // 枚举模块标志：所有模块（32+64）
  LIST_MODULES_DEFAULT = $0;            // 枚举模块标志：默认（按进程位数）

type                                    // 类型定义区
  // 获取进程完整映像路径（ANSI 版）的函数指针类型
  TQueryFullProcessImageNameA = function(hProcess: THandle; dwFlags: DWORD; lpExeName: LPSTR; var lpdwSize: DWORD): BOOL; stdcall;
  // 打开指定线程的函数指针类型
  TOpenThread = function(dwDesiredAccess: DWORD; bInheritHandle: BOOL; dwThreadId: DWORD): THandle; stdcall;
  // 枚举进程模块（增强版）的函数指针类型
  TEnumProcessModulesEx = function(hProcess: THandle; lphModule: PHMODULE; cb: DWORD; lpcbNeeded: PDWORD; dwFilterFlag: DWORD): BOOL; stdcall;

var                                     // 变量声明区
  WindowsKernel: THandle;               // 保存 Kernel32.dll 模块句柄
  QueryFullProcessImageNameA: TQueryFullProcessImageNameA; // 指向 QueryFullProcessImageNameA 的指针
  OpenThread: TOpenThread;              // 指向 OpenThread 的指针
  EnumProcessModulesEx: TEnumProcessModulesEx; // 指向 K32EnumProcessModulesEx 的指针

implementation                          // 实现部分开始

initialization                          // 单元初始化代码
begin
  WindowsKernel := LoadLibrary('Kernel32.dll'); // 动态加载 Kernel32.dll
  // 获取函数地址并赋值给对应指针
  QueryFullProcessImageNameA := GetProcAddress(WindowsKernel, 'QueryFullProcessImageNameA');
  OpenThread := GetProcAddress(WindowsKernel, 'OpenThread');
  EnumProcessModulesEx := GetProcAddress(WindowsKernel, 'K32EnumProcessModulesEx');
end;

end.                                    // 单元结束

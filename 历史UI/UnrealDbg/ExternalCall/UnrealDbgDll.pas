unit UnrealDbgDll;   // 单元名称：UnrealDbgDll——封装与核心驱动 DLL 的静态加载接口
interface            // 接口部分开始
uses
  Winapi.Windows , System.SysUtils;    // 引入 Windows API 常量、类型与函数
const
  PROTECT_FILE = 0;  // 保护模式：按文件路径保护（预留）
  PROTECT_WIN  = 1;  // 保护模式：按窗口句柄保护（预留）
type
  // 以下是指向 DLL 导出函数的函数指针类型。
  TUnreal_Initialize = function(key: ULONG64): BOOL; stdcall;   // 初始化驱动/设备句柄，key 为 64 位授权码
  TUnreal_StartProcess = function(szExe: PChar; sPath: PChar): BOOL; stdcall; // 启动目标进程并注入调试环境
  TUnreal_GetFileVersion = procedure(FileName: PChar; VerInfo: Pointer); stdcall; // 获取 PE 文件版本串到缓冲区
  TUnreal_TL_BlockGameResumeThread = function(dwPid: DWORD): BOOL; stdcall; // 阻止指定进程内部 ResumeThread 恢复线程（TL 对抗用）
var
  //变量名 : 类型
  LoadSymbol: THandle = 0;   // 保存 DLL 模块句柄，后续 FreeLibrary 用（本例未释放，程序退出由 OS 回收）
  Unreal_Initialize: TUnreal_Initialize = nil;   // 函数指针：初始化
  Unreal_StartProcess: TUnreal_StartProcess = nil; // 函数指针：启动进程
  Unreal_GetFileVersion: TUnreal_GetFileVersion = nil; // 函数指针：取文件版本
  Unreal_TL_BlockGameResumeThread: TUnreal_TL_BlockGameResumeThread = nil; // 函数指针：阻止 ResumeThread

function UnrealDbgDllReady: Boolean;
function UnrealDbgDllLastError: string;

implementation   // 实现部分开始
uses
  Grobal;       // 引入全局变量单元（内含 ExitProcess 封装）

var
  g_UnrealDbgDllLastError: string = '';

function LoadUnrealDbgDll: Boolean;
var
  DllPath: string;
  MissingExports: string;
begin
  if LoadSymbol <> 0 then
  begin
    Result := Assigned(Unreal_Initialize) and Assigned(Unreal_StartProcess) and
      Assigned(Unreal_GetFileVersion) and Assigned(Unreal_TL_BlockGameResumeThread);
    Exit;
  end;

  DllPath := IncludeTrailingPathDelimiter(ExtractFilePath(ParamStr(0))) + 'UnrealDbgDll.dll';
  LoadSymbol := LoadLibrary(PChar(DllPath));
  if LoadSymbol = 0 then
  begin
    g_UnrealDbgDllLastError := Format('LoadLibrary failed: %s, GetLastError=%d',
      [DllPath, GetLastError]);
    Result := False;
    Exit;
  end;

  Unreal_Initialize := GetProcAddress(LoadSymbol, 'Initialize_F3');
  Unreal_StartProcess := GetProcAddress(LoadSymbol, 'StartProcess');
  Unreal_GetFileVersion := GetProcAddress(LoadSymbol, 'GetFileVersion');
  Unreal_TL_BlockGameResumeThread := GetProcAddress(LoadSymbol, 'TL_BlockGameResumeThread');

  MissingExports := '';
  if not Assigned(Unreal_Initialize) then
    MissingExports := MissingExports + ' Initialize_F3';
  if not Assigned(Unreal_StartProcess) then
    MissingExports := MissingExports + ' StartProcess';
  if not Assigned(Unreal_GetFileVersion) then
    MissingExports := MissingExports + ' GetFileVersion';
  if not Assigned(Unreal_TL_BlockGameResumeThread) then
    MissingExports := MissingExports + ' TL_BlockGameResumeThread';

  Result := MissingExports = '';
  if not Result then
  begin
    g_UnrealDbgDllLastError := 'UnrealDbgDll export missing:' + MissingExports;
    FreeLibrary(LoadSymbol);
    LoadSymbol := 0;
    Exit;
  end;

  g_UnrealDbgDllLastError := '';
end;

function UnrealDbgDllReady: Boolean;
begin
  Result := LoadUnrealDbgDll;
end;

function UnrealDbgDllLastError: string;
begin
  Result := g_UnrealDbgDllLastError;
end;

initialization   // 单元首次加载时自动执行
begin
  LoadUnrealDbgDll;
end;
end.   // 单元结束

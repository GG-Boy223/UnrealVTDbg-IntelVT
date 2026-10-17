unit LockThread;

interface

uses Winapi.Windows, Vcl.ComCtrls, Winapi.Messages, System.Classes, System.SysUtils, ExtTQueue,
  PsAPI;


type
  TMsgRcd = record
    nAction: Integer;
  end;
  pTMsgRcd = ^TMsgRcd;

  TLockThread = class(TThread)
    m_MsgQueue: TExtQueue;
    m_Open: Boolean;
  private
    FLock: TRTLCriticalSection;
  public
    {*********************************线程类方法*********************************}
    procedure Lock;
    procedure UnLock;
    constructor Create(bFlag: Boolean);
    destructor Destroy; override;
    procedure Execute; override;
    procedure SendMsg(MsgRcd: TMsgRcd);
    function GetMsg(pMsgRcd: pTMsgRcd): Boolean;
    procedure DispatchMsg(MsgRcd: TMsgRcd);
    procedure Stop;
    function IsOpen: Boolean;
  public
    m_handled_GetTickCount: BOOL;
  end;

  function GetProcessModuleBase(ProcessID: DWORD; ModuleName: string): DWORD_PTR;

var
  g_LockThread: TLockThread;

implementation

uses Log, Main, Kernel32Api, HandlerTLDetection, Grobal, GlobalVar;

function GetProcessModuleBase(ProcessID: DWORD; ModuleName: string): DWORD_PTR;
var
  hProcess: THandle;
  Module: HMODULE;
  modInfo: MODULEINFO;
  modules: array of HMODULE;
  need: DWORD;
  tmpName: array of Char;
  sName: string;
begin
  Result := 0;
  hProcess := OpenProcess(PROCESS_ALL_ACCESS, False, ProcessID);
  if hProcess <> 0 then begin
    try
      EnumProcessModulesEx(hProcess,nil,0,@need, LIST_MODULES_ALL);
      SetLength(modules, need div SizeOf(HMODULE));
      FillChar(modules[0],Length(modules) * SizeOf(HMODULE),#0);
      EnumProcessModulesEx(hProcess, @modules[0], need, @need, LIST_MODULES_ALL);
      SetLength(tmpName,1024);
      for Module in modules do begin
        FillChar(tmpName[0],Length(tmpName) * SizeOf(Char),#0);
        GetModuleFileNameEx(hProcess, Module, @tmpName[0], 1024);
        sName := string(PChar(@tmpName[0]));
        if SameText(ExtractFileName(sName), ModuleName) then begin
          GetModuleInformation(hProcess, Module, @modInfo, SizeOf(MODULEINFO));
          Result := DWORD_PTR(modInfo.lpBaseOfDll);
//          MessageBox(0,PChar(Result.ToHexString),'模块基地址:', MB_SYSTEMMODAL);
          Break;
        end;
      end;
    finally
      SetLength(modules,0);
      SetLength(tmpName,0);
      CloseHandle(hProcess);
    end;
  end;
end;


constructor TLockThread.Create(bFlag: Boolean);
begin
  try
    inherited Create(bFlag);  //False创建线程后立即执行
    InitializeCriticalSection(FLock);
    m_MsgQueue := TExtQueue.Create;
    m_handled_GetTickCount := False;
  except on e:Exception do
    sLog.outError('[TLockThread.Create]===>' + e.Message);
  end;
end;

destructor TLockThread.Destroy;
begin
  try
    try
      while m_MsgQueue.Count > 0 do
        Dispose(pTMsgRcd(m_MsgQueue.Pop));
      m_MsgQueue.Free;
    finally
      DeleteCriticalSection(FLock);
      inherited;
    end;
  except on e:Exception do
    sLog.outError('[TLockThread.Destroy]===>' + e.Message);
  end;
end;

procedure TLockThread.Lock;
begin
  EnterCriticalSection(FLock);
end;

procedure TLockThread.UnLock;
begin
  LeaveCriticalSection(FLock);
end;

procedure TLockThread.Execute;
var
  MsgRcd: TMsgRcd;
  processhandle: THandle;
  dwTick: DWORD;
  lpNumberOfBytesWritten: DWORD_PTR;
  boRes: BOOL;
begin
  try
    m_Open := True;  //标识线程已经执行

    if Form1.m_targetProcessId <> 0 then begin
      {获取进程句柄}
      processhandle := OpenProcess(PROCESS_ALL_ACCESS, False, Form1.m_targetProcessId);

      //锁定目标地址的值
      while IsOpen do begin
        if (not g_boEnabled_tl_confrontation_TL) or
          (not g_boHandlerGetTickCountCheck_TL) then begin        
          Break;        
        end;
        if processhandle <> 0 then begin
          dwTick := 0;
          if lastTick_ptr <> 0 then begin
            boRes := WriteProcessMemory(processhandle,
            Pointer(lastTick_ptr),
            @dwTick,
            SizeOf(DWORD),
            lpNumberOfBytesWritten);

            if boRes then begin
              if m_handled_GetTickCount = False then begin
                m_handled_GetTickCount := True;
                sLog.outInfo(_STR_TL_EXE + ' 已处理GetTickCount检测');
              end;
            end else begin
              sLog.outDebug(_STR_TL_EXE + ' error:' + GetLastError.ToString);
              Sleep(1500);
            end;
          end;
        end;
        Sleep(50);  //锁定间隔50ms
      end;
      CloseHandle(processhandle);
//      sLog.outDebug(_STR_TL_EXE + ' 退出锁定线程');
    end;
  except on e:Exception do
    sLog.outError('[TLockThread.Execute]===>' + e.Message);
  end;
end;

//从消息队列中取出消息
function TLockThread.GetMsg(pMsgRcd: pTMsgRcd): Boolean;
var
  TmpMsgRcd: pTMsgRcd;
begin
  try
    Result := False;
    if pMsgRcd = nil then Exit;

    m_MsgQueue.Lock;
    try
      while m_MsgQueue.Count > 0 do begin
        TmpMsgRcd := pTMsgRcd(m_MsgQueue.Pop);
        if TmpMsgRcd <> nil then begin
          FillChar(pMsgRcd^,SizeOf(TMsgRcd),#0);
          pMsgRcd.nAction := TmpMsgRcd.nAction;
          Dispose(TmpMsgRcd);
          Result := True;
          Break;
        end;
      end;
    finally
      m_MsgQueue.UnLock;
    end;
  except on e:Exception do
    sLog.outError('[TLockThread.GetMsg]===>' + e.Message);
  end;
end;

//将消息插入消息队列
procedure TLockThread.SendMsg(MsgRcd: TMsgRcd);
var
  pMsgRcd: pTMsgRcd;
begin
  try
    m_MsgQueue.Lock;
    try
      New(pMsgRcd);
      FillChar(pMsgRcd^,SizeOf(TMsgRcd),#0);
      pMsgRcd.nAction := MsgRcd.nAction;
      m_MsgQueue.Push(pMsgRcd);
    finally
      m_MsgQueue.UnLock;
    end;
  except on e:Exception do
    sLog.outError('[TLockThread.SendMsg]===>' + e.Message);
  end;
end;

//派遣消息
procedure TLockThread.DispatchMsg(MsgRcd: TMsgRcd);
begin
  try

  except on e:Exception do
    sLog.outError('[TLockThread.DispatchMsg]===>' + e.Message);
  end;
end;

procedure TLockThread.Stop;
begin
  try
    Lock;
    try
      while not m_Open do Sleep(50);
      m_Open := False;
    finally
      UnLock;
    end;
  except on e:Exception do
    sLog.outError('[TLockThread.Stop]===>' + e.Message);
  end;
end;

function TLockThread.IsOpen: Boolean;
begin
  try
    Lock;
    try
      Result := m_Open;
    finally
      UnLock;
    end;
  except on e:Exception do
    sLog.outError('[TLockThread.IsOpen]===>' + e.Message);
  end;
end;

end.

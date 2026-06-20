unit EventHandlerThread;

interface

uses Winapi.Windows, Vcl.ComCtrls, Winapi.Messages, System.Classes, System.SysUtils, ExtTQueue;

type
  TMsgRcd = record
    nAction: Integer;
  end;
  pTMsgRcd = ^TMsgRcd;

  TEventHandlerThread = class(TThread)
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
  end;

var
  g_EventHandlerThread: TEventHandlerThread;

const
  EVENT_ENABLED_TL_CONFRONTATION = 1000;

implementation

uses Log, Main;

{ TEventHandlerThread }

constructor TEventHandlerThread.Create(bFlag: Boolean);
begin
  try
    inherited Create(bFlag);  //False创建线程后立即执行
    InitializeCriticalSection(FLock);
    m_MsgQueue := TExtQueue.Create;
  except on e:Exception do
    sLog.outError('[TEventHandlerThread.Create]===>' + e.Message);
  end;
end;

destructor TEventHandlerThread.Destroy;
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
    sLog.outError('[TEventHandlerThread.Destroy]===>' + e.Message);
  end;
end;

procedure TEventHandlerThread.Lock;
begin
  EnterCriticalSection(FLock);
end;

procedure TEventHandlerThread.UnLock;
begin
  LeaveCriticalSection(FLock);
end;

procedure TEventHandlerThread.Execute;
var
  MsgRcd: TMsgRcd;
begin
  try
    m_Open := True;  //标识线程已经执行

    //双while循环可以完美的解决以下程序设计问题
    //1.当没有数据到来时线程一直在空转占用cpu资源
    //2.加Sleep延迟 如果有多条数据需要处理时 每次都需要等候
    while IsOpen do begin
      while GetMsg(@MsgRcd) do begin
        DispatchMsg(MsgRcd);         //消息分发
      end;
      Sleep(100);
    end;
  except on e:Exception do
    sLog.outError('[TEventHandlerThread.Execute]===>' + e.Message);
  end;
end;

//从消息队列中取出消息
function TEventHandlerThread.GetMsg(pMsgRcd: pTMsgRcd): Boolean;
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
    sLog.outError('[TEventHandlerThread.GetMsg]===>' + e.Message);
  end;
end;

//将消息插入消息队列
procedure TEventHandlerThread.SendMsg(MsgRcd: TMsgRcd);
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
    sLog.outError('[TEventHandlerThread.SendMsg]===>' + e.Message);
  end;
end;

//派遣消息
procedure TEventHandlerThread.DispatchMsg(MsgRcd: TMsgRcd);
begin
  try
    case MsgRcd.nAction of
      EVENT_ENABLED_TL_CONFRONTATION: begin
        Form1.Enabled_TL_Confrontation;
      end;
    end;
  except on e:Exception do
    sLog.outError('[TEventHandlerThread.DispatchMsg]===>' + e.Message);
  end;
end;

procedure TEventHandlerThread.Stop;
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
    sLog.outError('[TEventHandlerThread.Stop]===>' + e.Message);
  end;
end;

function TEventHandlerThread.IsOpen: Boolean;
begin
  try
    Lock;
    try
      Result := m_Open;
    finally
      UnLock;
    end;
  except on e:Exception do
    sLog.outError('[TEventHandlerThread.IsOpen]===>' + e.Message);
  end;
end;

end.

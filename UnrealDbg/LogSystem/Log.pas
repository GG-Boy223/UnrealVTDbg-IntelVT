unit Log;

interface

uses Winapi.Windows, Vcl.ComCtrls, Winapi.Messages, System.Classes, System.SysUtils, Vcl.Graphics,
  Vcl.ExtCtrls;

type

  TMyColor = (
      BLACK,
      RED,
      GREEN,
      BROWN,
      BLUE,
      MAGENTA,
      CYAN,
      GREY,
      YELLOW,
      LRED,
      LGREEN,
      LBLUE,
      LMAGENTA,
      LCYAN,
      WHITE
    );

  TLogMsgRcd = record
    sText: string;
    LogType: Word;
    Color: TMyColor;
  end;
  PTLogMsgRcd = ^TLogMsgRcd;

  TLog = class
  private
    FRichEditLog: TRichEdit;
    FLock: TRTLCriticalSection;
//    FTimer: TTimer;
    LogMsgRcdList: TList;
  public
    constructor Create;
    destructor Destroy; override;

    procedure Lock;
    procedure UnLock;
    procedure outDebug(sText: string);
    procedure outInfo(sText: string);
    procedure outError(sText: string);
    procedure LogPrint(sText: string; LogType: Word; Color: TMyColor);
    procedure RichEditClear(value: Word);
    procedure setLog(RichEdit: TRichEdit);
    procedure WriteLogFile(sText: string; LogType: Word);
    procedure setColor(Color: TMyColor);
    procedure AddToMsgList(sText: string; LogType: Word; Color: TMyColor);
    procedure OutputMsg;
    procedure TimerEvent(Sender: TObject);
  end;

var
  sLog: TLog;


implementation

const
  DEBUG_TYPE = 0;
  ERROR_TYPE = 1;
  INFO_TYPE  = 2;

{ TLog }

constructor TLog.Create;
begin
  inherited;
  InitializeCriticalSection(FLock);
  LogMsgRcdList := TList.Create;
//  FTimer := TTimer.Create(nil);
//  FTimer.Interval := 500;
//  FTimer.OnTimer := TimerEvent;
end;

destructor TLog.Destroy;
var
  I: Integer;
begin
//  FTimer.Free;
  for I := 0 to LogMsgRcdList.Count - 1 do
    Dispose(PTLogMsgRcd(LogMsgRcdList.Items[I]));
  LogMsgRcdList.Free;
  DeleteCriticalSection(FLock);
  inherited;
end;

procedure TLog.Lock;
begin
  EnterCriticalSection(FLock);
end;

procedure TLog.UnLock;
begin
  LeaveCriticalSection(FLock);
end;

procedure TLog.setLog(RichEdit: TRichEdit);
begin
  FRichEditLog := RichEdit;
end;

procedure TLog.TimerEvent(Sender: TObject);
begin
  OutputMsg;
end;

procedure TLog.RichEditClear(value: Word);
begin
  if not Assigned(FRichEditLog) then Exit;

  if FRichEditLog.Lines.Count > value then
    FRichEditLog.Clear;
end;

procedure TLog.WriteLogFile(sText: string; LogType: Word);
var
  F: TextFile;
  sLogFile, sLogDir: string;
begin
  if sText <> '' then begin
    case LogType of
      DEBUG_TYPE: begin
        Exit;
      end;
      INFO_TYPE: begin
        sText := DateTimeToStr(Now) + ' [INFORMATION]: ' + sText;
      end;
      ERROR_TYPE: begin
        sText := DateTimeToStr(Now) + ' [Error]: ' + sText;
      end;
    end;

    sLogDir := '.\Log';
    if not DirectoryExists(sLogDir) then
      ForceDirectories(sLogDir);

    sLogFile := sLogDir + '\Log.txt';
    try
      AssignFile(F, sLogFile);
      if not FileExists(sLogFile) then
        Rewrite(F)
      else
        Append(F);

      Writeln(F, sText);
      Flush(F);
    finally
      CloseFile(F);
    end;
  end;
end;

procedure TLog.setColor(Color: TMyColor);
begin
  if not Assigned(FRichEditLog) then
    Exit;

  case Color of
    BLACK:
      FRichEditLog.SelAttributes.Color := clBlack;                 //黑色
    RED:
      FRichEditLog.SelAttributes.Color := clRed;                   //红色
    GREEN:
      FRichEditLog.SelAttributes.Color := clLime;                  //绿色
    BROWN:
      FRichEditLog.SelAttributes.Color := clWebSaddleBrown;        //棕色
    BLUE:
      FRichEditLog.SelAttributes.Color := clWebDeepskyBlue;        //蓝色
    MAGENTA:
      FRichEditLog.SelAttributes.Color := clWebMagenta;            //品红
    CYAN:
      FRichEditLog.SelAttributes.Color := clWebCyan;               //青色
    GREY:
      FRichEditLog.SelAttributes.Color := clWebLightgrey;          //灰色
    YELLOW:
      FRichEditLog.SelAttributes.Color := clYellow;                //黄色
    LRED:
      FRichEditLog.SelAttributes.Color := clWebDarkRed;            //红色加深
    LGREEN:
      FRichEditLog.SelAttributes.Color := clWebLightGreen;         //绿色加深
    LBLUE:
      FRichEditLog.SelAttributes.Color := clWebLightBlue;          //蓝色加深
    LMAGENTA:
      FRichEditLog.SelAttributes.Color := clWebDarkMagenta;        //品红加深
    LCYAN:
      FRichEditLog.SelAttributes.Color := clWebLightCyan;          //青色加深
    WHITE:
      FRichEditLog.SelAttributes.Color := clWhite;                 //米色
  end;
end;

procedure TLog.LogPrint(sText: string; LogType: Word; Color: TMyColor);
begin
  Lock;
  try
    if (sText <> '') and (Assigned(FRichEditLog)) then begin
      try
        setColor(Color);
        FRichEditLog.Lines.Add(DateTimeToStr(Now) + ' ' + sText);
        RichEditClear(200);
      except

      end;
      SendMessage(FRichEditLog.Handle, WM_VSCROLL, SB_BOTTOM, 0); //向上滚动
      WriteLogFile(sText,LogType);
    end;
  finally
    UnLock;
  end;
end;

procedure TLog.AddToMsgList(sText: string; LogType: Word; Color: TMyColor);
var
  sFormat: string;
  LogMsgRcd: PTLogMsgRcd;
begin
  if not Assigned(FRichEditLog) then
    Exit;

  sFormat := sText;
  if sFormat = '' then
    Exit;

  New(LogMsgRcd);
  FillChar(LogMsgRcd^,SizeOf(TLogMsgRcd),#0);
  LogMsgRcd.sText := sFormat;
  LogMsgRcd.LogType := LogType;
  LogMsgRcd.Color := Color;

  Lock;
  try
    LogMsgRcdList.Add(LogMsgRcd);
  finally
    UnLock;
  end;
end;

procedure TLog.OutputMsg;
var
  I: Integer;
  LogMsgRcd: PTLogMsgRcd;
begin
  Lock;
  try
    for I := 0 to LogMsgRcdList.Count - 1 do begin
      LogMsgRcd := LogMsgRcdList.Items[I];
      if Assigned(LogMsgRcd) then begin
        LogPrint(LogMsgRcd.sText,LogMsgRcd.LogType,LogMsgRcd.Color);
        Dispose(LogMsgRcd);
      end;
    end;
    LogMsgRcdList.Clear;
  finally
    UnLock;
  end;
end;

procedure TLog.outDebug(sText: string);
begin
  AddToMsgList(sText,DEBUG_TYPE, GREEN);
end;

procedure TLog.outInfo(sText: string);
begin
  AddToMsgList(sText,INFO_TYPE, GREEN);
end;

procedure TLog.outError(sText: string);
begin
  AddToMsgList(sText,ERROR_TYPE, RED);
end;

initialization
begin
  sLog := TLog.Create;
end;

finalization
begin
  sLog.Free;
end;

end.

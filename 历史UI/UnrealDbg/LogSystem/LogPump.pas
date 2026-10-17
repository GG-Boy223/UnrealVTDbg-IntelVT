unit LogPump;

interface

procedure InstallMainThreadLogPump;
procedure ShutdownMainThreadLogPump;

implementation

uses
  System.Classes,
  System.SysUtils,
  Vcl.ExtCtrls,
  Vcl.Forms,
  Log;

const
  // A short interval keeps the RichEdit log responsive without forcing the
  // main thread into the tight OnIdle loop that was burning CPU.
  LOG_PUMP_INTERVAL_MS = 200;

type
  TLogPumpGuard = class(TComponent)
  protected
    procedure HandleLogPumpTimer(Sender: TObject);
    procedure Notification(AComponent: TComponent; Operation: TOperation); override;
  end;

var
  GLogPumpTimer: TTimer;
  GLogPumpGuard: TLogPumpGuard;

procedure TLogPumpGuard.Notification(AComponent: TComponent; Operation: TOperation);
begin
  inherited;

  if (Operation = opRemove) and (AComponent = Application.MainForm) then
    ShutdownMainThreadLogPump;
end;

procedure TLogPumpGuard.HandleLogPumpTimer(Sender: TObject);
begin
  try
    // Flush queued log messages on the VCL thread so background worker
    // threads never touch the RichEdit directly.
    sLog.OutputMsg;
  except
    // If the UI is already being torn down, stop the pump immediately to
    // avoid touching controls that are no longer valid.
    ShutdownMainThreadLogPump;
  end;
end;

procedure InstallMainThreadLogPump;
begin
  // Main.FormCreate installs an OnIdle handler that keeps Done := False.
  // Replacing it here avoids the permanent busy loop while preserving
  // periodic UI log updates through a throttled timer.
  Application.OnIdle := nil;

  if GLogPumpGuard = nil then
  begin
    GLogPumpGuard := TLogPumpGuard.Create(Application);
  end;

  if Application.MainForm <> nil then
  begin
    Application.MainForm.FreeNotification(GLogPumpGuard);
  end;

  if GLogPumpTimer = nil then
  begin
    GLogPumpTimer := TTimer.Create(nil);
    GLogPumpTimer.Interval := LOG_PUMP_INTERVAL_MS;
    GLogPumpTimer.OnTimer := GLogPumpGuard.HandleLogPumpTimer;
    GLogPumpTimer.Enabled := True;
  end;

  // Flush anything queued during startup so the first screen paint already
  // reflects early initialization logs.
  sLog.OutputMsg;
end;

procedure ShutdownMainThreadLogPump;
begin
  if GLogPumpTimer <> nil then
  begin
    GLogPumpTimer.Enabled := False;
    FreeAndNil(GLogPumpTimer);
  end;

  if GLogPumpGuard <> nil then
    FreeAndNil(GLogPumpGuard);
end;

end.

unit HandlerTLDetection;

interface

uses Winapi.Windows, Vcl.ComCtrls, Winapi.Messages, System.Classes, System.SysUtils, ExtTQueue,
  PsAPI;

procedure Handler_TLDetection(base: DWORD_PTR);

const
  lastTick_offset = $FF0A439;

var
  lastTick_ptr: DWORD_PTR;

implementation

uses LockThread, Log;

procedure Handler_TLDetection(base: DWORD_PTR);
begin
  lastTick_ptr := base + lastTick_offset + 4;

  if lastTick_ptr <> 0 then begin
//    sLog.outDebug('lastTick_ptr: ' + lastTick_ptr.ToHexString);
    if g_LockThread = nil then begin
      g_LockThread := TLockThread.Create(False);
    end;
  end;
end;

end.

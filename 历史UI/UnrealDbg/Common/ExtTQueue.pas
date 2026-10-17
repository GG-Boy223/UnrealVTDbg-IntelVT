unit ExtTQueue;

interface

uses
  Windows, System.Contnrs;

type

  TExtQueue = class(TQueue)
  private
    FLock: TRTLCriticalSection;
  public
    constructor Create;
    destructor Destroy; override;
    procedure Lock;
    procedure UnLock;
  end;

implementation

constructor TExtQueue.Create;
begin
  inherited;
  InitializeCriticalSection(FLock);
end;

destructor TExtQueue.Destroy;
begin
  DeleteCriticalSection(FLock);
  inherited;
end;

procedure TExtQueue.Lock;
begin
  EnterCriticalSection(FLock);
end;

procedure TExtQueue.UnLock;
begin
  LeaveCriticalSection(FLock);
end;

end.

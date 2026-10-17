program ¿¨ÃÜ×¢²á;

uses
  Vcl.Forms,
  Main in 'Forms\Main.pas' {frmCardCode};

{$R *.res}

begin
  Application.Initialize;
  Application.MainFormOnTaskbar := True;
  Application.CreateForm(TfrmCardCode, frmCardCode);
  Application.CreateForm(TfrmCardCode, frmCardCode);
  Application.Run;
end.

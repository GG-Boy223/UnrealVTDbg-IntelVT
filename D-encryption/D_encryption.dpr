program D_encryption;

uses
  Vcl.Forms,
  Main in 'Forms\Main.pas' {Form3},
  D_encryptionDll in 'ExternalCall\D_encryptionDll.pas';

{$R *.res}

begin
  Application.Initialize;
  Application.MainFormOnTaskbar := True;
  Application.CreateForm(TForm3, Form3);
  Application.Run;
end.

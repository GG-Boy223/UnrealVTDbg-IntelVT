{
  注意: RAD Studio 11 编译64位程序时，如果将项目名命名为中文则无法进行调试。
}

unit Main;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants, System.Classes, Vcl.Graphics,
  Vcl.Controls, Vcl.Forms, Vcl.Dialogs, Vcl.StdCtrls, System.IniFiles;

type
  TForm2 = class(TForm)
    RadioButtonMirrorServer: TRadioButton;
    RadioButtonMicrosoftServer: TRadioButton;
    RadioButtonCustomServer: TRadioButton;
    EditCustomServer: TEdit;
    procedure FormCreate(Sender: TObject);
    procedure RadioButtonMirrorServerClick(Sender: TObject);
    procedure RadioButtonMicrosoftServerClick(Sender: TObject);
    procedure RadioButtonCustomServerClick(Sender: TObject);
    procedure EditCustomServerChange(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
    procedure LoadSymbolState;
    procedure SetSymbolState(selectSvr: Integer);
    procedure RefRadioButtonState;
  end;

var
  Form2: TForm2;
  g_SelectServer: Integer; //选择的服务器
  g_CustomServer: string;
  g_LoadingSymbolState: Boolean;

const
  _STR_CONFIG_INI = 'Config.ini';
  _STR_SYMBOL_SECTION = 'Symbol';
  _STR_SERVER = 'Server';
  _STR_CUSTOM_SERVER = 'CustomServer';

implementation

{$R *.dfm}


procedure TForm2.LoadSymbolState;
var
  Config: TIniFile;
  sPath: string;
begin
  try
    g_LoadingSymbolState := True;
    sPath := ExtractFilePath(Application.ExeName);
    if sPath <> '' then begin
      sPath := sPath + _STR_CONFIG_INI;
      if FileExists(sPath) then begin
        Config := TIniFile.Create(sPath);
        try
          g_SelectServer := Config.ReadInteger('符号','服务器',
            Config.ReadInteger(_STR_SYMBOL_SECTION,_STR_SERVER, g_SelectServer));
          g_CustomServer := Trim(Config.ReadString('符号','自定义服务器',
            Config.ReadString('符号','服务器地址',
              Config.ReadString(_STR_SYMBOL_SECTION,_STR_CUSTOM_SERVER, g_CustomServer))));
        finally
          Config.Free;
        end;
      end;
    end;
    EditCustomServer.Text := g_CustomServer;
    RefRadioButtonState;
  except on e:Exception do
  end;
  g_LoadingSymbolState := False;
end;
procedure TForm2.SetSymbolState(selectSvr: Integer);
var
  Config: TIniFile;
  sPath: string;
begin
  try
    g_SelectServer := selectSvr;
    g_CustomServer := Trim(EditCustomServer.Text);

    sPath := ExtractFilePath(Application.ExeName);
    if sPath <> '' then begin
      sPath := sPath + _STR_CONFIG_INI;
      Config := TIniFile.Create(sPath);
      try
        Config.WriteInteger('符号','服务器',g_SelectServer);
        Config.WriteInteger(_STR_SYMBOL_SECTION,_STR_SERVER,g_SelectServer);
        Config.WriteString('符号','自定义服务器',g_CustomServer);
        Config.WriteString(_STR_SYMBOL_SECTION,_STR_CUSTOM_SERVER,g_CustomServer);
      finally
        Config.Free;
      end;
    end;

    RefRadioButtonState;
  except on e:Exception do
  end;
end;
procedure TForm2.RadioButtonMicrosoftServerClick(Sender: TObject);
begin
  SetSymbolState(200);  //微软服务器
end;

procedure TForm2.RadioButtonMirrorServerClick(Sender: TObject);
begin
  SetSymbolState(100);  //镜像服务器
end;

procedure TForm2.RadioButtonCustomServerClick(Sender: TObject);
begin
  SetSymbolState(300);
  if EditCustomServer.CanFocus then begin
    EditCustomServer.SetFocus;
  end;
end;

procedure TForm2.EditCustomServerChange(Sender: TObject);
begin
  if g_LoadingSymbolState then begin
    Exit;
  end;

  g_CustomServer := Trim(EditCustomServer.Text);
  if RadioButtonCustomServer.Checked then begin
    SetSymbolState(300);
  end;
end;

procedure TForm2.RefRadioButtonState;
begin
  if (g_SelectServer <> 100) and (g_SelectServer <> 200) and (g_SelectServer <> 300) then begin
    g_SelectServer := 100;
  end;

  RadioButtonMirrorServer.Checked := g_SelectServer = 100;
  RadioButtonMicrosoftServer.Checked := g_SelectServer = 200;
  RadioButtonCustomServer.Checked := g_SelectServer = 300;
  EditCustomServer.Enabled := RadioButtonCustomServer.Checked;
end;
procedure TForm2.FormCreate(Sender: TObject);
begin
  g_LoadingSymbolState := False;
  RadioButtonMirrorServer.Checked := False;
  RadioButtonMicrosoftServer.Checked := False;
  RadioButtonCustomServer.Checked := False;
  EditCustomServer.Text := g_CustomServer;
  EditCustomServer.Enabled := False;
  LoadSymbolState;
end;
initialization
begin
  g_SelectServer := 100;  //默认选择镜像服务器
  g_CustomServer := '';
  g_LoadingSymbolState := False;
end;

end.

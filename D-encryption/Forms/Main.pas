{
  注意: RAD Studio 11 编译64位程序时，如果将项目名命名为中文则无法进行调试。
}

unit Main;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants, System.Classes, Vcl.Graphics,
  Vcl.Controls, Vcl.Forms, Vcl.Dialogs, Vcl.StdCtrls, System.JSON, Winapi.ShellAPI;

type
  TForm3 = class(TForm)
    Label1: TLabel;
    Edit1: TEdit;
    Label2: TLabel;
    Edit2: TEdit;
    Button1: TButton;
    Label3: TLabel;
    procedure Button1Click(Sender: TObject);
    procedure FormCreate(Sender: TObject);
  private
    { Private declarations }
    procedure WMDropFiles(var Msg: TWMDropFiles); message WM_DROPFILES;
  public
    { Public declarations }
  end;

var
  Form3: TForm3;

const
  _STR_KEY = 'nwkjhdn23e1awe1233';  {虚幻调试器经16位md5计算得到}

implementation

uses D_encryptionDll;

{$R *.dfm}

procedure TForm3.Button1Click(Sender: TObject);
var
  json: TJSONObject;
  PlainText: string;
begin
  try
    if (Edit1.Text <> '') and (Edit2.Text <> '') then begin
      json := TJSONObject.Create;
      json.AddPair('虚幻调试器标题',Edit1.Text);
      json.AddPair('QQ群日志',Edit2.Text);
      PlainText := json.ToString;
      D_encryption_EncryptDataToFile(PlainText,'F3copyright.db',_STR_KEY);
      MessageBox(0,'F3copyright.db生成完毕!','完成',MB_OK or MB_ICONINFORMATION);
      json.Free;
    end;
  except on e:Exception do
  end;
end;

procedure TForm3.FormCreate(Sender: TObject);
begin
  try
    DragAcceptFiles(Handle, True);
  except on e:Exception do
  end;
end;

procedure TForm3.WMDropFiles(var Msg: TWMDropFiles);
var
  FileName: array[0..255] of Char;
  sFileName: string;
  decryptedDataLen: Integer;
  PlainText: array of Char;
  PlainTextLen: Integer;
  jsonStr: string;
  json: TJSONObject;
begin
  try
    FillChar(FileName,SizeOf(FileName),#0);
    DragQueryFile(Msg.Drop, 0, FileName, SizeOf(FileName));
    sFileName := string(PChar(@FileName[0]));
    if sFileName <> '' then begin
      //返回的是字节数，包含结尾空字符
      decryptedDataLen := D_encryption_DecryptDataFromFile(sFileName,_STR_KEY,nil);
      if decryptedDataLen > 0 then begin
        PlainTextLen := (decryptedDataLen div 2) + 1;
        SetLength(PlainText,PlainTextLen); //长度是字符个数
        FillChar(PlainText[0],Length(PlainText) * SizeOf(Char),#0);
        decryptedDataLen := D_encryption_DecryptDataFromFile(sFileName,_STR_KEY,@PlainText[0]);
        jsonStr := string(PChar(@PlainText[0]));
        json := json.ParseJSONValue(jsonStr) as TJSONObject;     //解析json
        Edit1.Text := json.Values['虚幻调试器标题'].AsType<string>;
        Edit2.Text := json.Values['QQ群日志'].AsType<string>;
        json.Free;
        SetLength(PlainText,0);
      end;
    end;
  finally
    DragFinish(Msg.Drop);
  end;
  Msg.Result := 0;
end;

end.

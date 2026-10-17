{
  注意: RAD Studio 11 编译64位程序时，如果将项目名命名为中文则无法进行调试。
}

unit Main;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants, System.Classes, Vcl.Graphics,
  Vcl.Controls, Vcl.Forms, Vcl.Dialogs, Vcl.StdCtrls, System.IniFiles;

type
  TfrmCardCode = class(TForm)
    Edit1: TEdit;
    Label1: TLabel;
    Button1: TButton;
    Label2: TLabel;
    procedure Button1Click(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
  end;

var
  frmCardCode: TfrmCardCode;

const
  _STR_CDK_FILE = 'License.key';

implementation

{$R *.dfm}

procedure TfrmCardCode.Button1Click(Sender: TObject);
var
  Config: TStringList;
  sPath: string;
begin
  try
    if Edit1.Text <> '' then begin
      sPath := ExtractFilePath(Application.ExeName);
      if sPath <> '' then begin
        sPath := sPath + _STR_CDK_FILE;
        if FileExists(sPath) then begin
          DeleteFile(sPath);
        end;

        Config := TStringList.Create;
        Config.Add(Edit1.Text);
        Config.SaveToFile(_STR_CDK_FILE);
        Config.Free;

        MessageBox(0,'注册卡密成功，请勿将License.key文件泄露给他人！','恭喜!!!', MB_OK or MB_ICONINFORMATION or MB_SYSTEMMODAL);
        ExitProcess(0);
      end;
    end else begin
      MessageBox(0,'卡密不能为空！','错误:', MB_OK or MB_ICONWARNING or MB_SYSTEMMODAL);
    end;
  except on e:Exception do
  end;
end;

end.

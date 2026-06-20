unit D_encryptionDll;

interface

uses Winapi.Windows;

type
  TD_encryption_EncryptDataToFile = procedure(data: string; filename: string; userKey: string);

  //返回的是字节数包含结尾的空字符
  TD_encryption_DecryptDataFromFile = function(filename: string; userKey: string; decryptedData: Pointer): Integer;

var
  D_encryption_mod: THandle;
  D_encryption_EncryptDataToFile: TD_encryption_EncryptDataToFile;
  D_encryption_DecryptDataFromFile: TD_encryption_DecryptDataFromFile;

implementation

initialization
begin
{$IFNDEF RELEASE}
  D_encryption_mod := LoadLibrary('E:\Projects\VS\repos\D-encryption\x64\Debug\D-encryption.dll');
{$ELSE}
  D_encryption_mod := LoadLibrary('D-encryption.dll');
{$ENDIF}
  if D_encryption_mod = 0 then begin
    MessageBox(0,'没有找到 D-encryption.dll','错误信息:',MB_ICONERROR);
    ExitProcess(0);
  end;
  D_encryption_EncryptDataToFile := GetProcAddress(D_encryption_mod,'EncryptDataToFile');
  D_encryption_DecryptDataFromFile := GetProcAddress(D_encryption_mod,'DecryptDataFromFile');
end;

end.

unit Grobal;

interface

uses Winapi.ActiveX, System.Win.ComObj, Vcl.Forms, System.SysUtils, Winapi.Windows;

function GetValidStr3(Str: string; var Dest: string; const Divider: array of Char): string;
function GetWMIBIOS(WMIProperty: string): string;
function GetWMIBaseBoard(WMIProperty: string): string;
function GetWMIPhysicalMedia(WMIProperty: string): string;
function GetApplicationPath: string;
procedure SetPrivilege;
//生成随机字符串
function GenerateRandomString(nCount: Integer): string;

const
BUFFERSIZE = 500 * 1024;

implementation

{
截断字符串
功能：  根据分割字符,最先匹配的位置(当前位置字符丢弃),截取字符串,dest 保存截取位置之前的字符串
        没有匹配,截取全部
返回值：保存截取位置之后的字符串

参数1   str       源操作数
参数2   dest      目的操作数
参数3   divider   分割字符数组
}
function GetValidStr3(Str: string; var Dest: string; const Divider: array of Char): string;
const
  BUF_SIZE                  = BUFFERSIZE;
var
  buf                       : array[0..BUF_SIZE] of Char;
  i, BufCount, count        : Longint;
  SrcLen, ArrCount          : Longint;
  ch                        : Char;
label
  CATCH_DIV;
begin
  ch := #0;
  FillChar(buf,SizeOf(buf),0);

  try
    SrcLen := Length(Str);
    BufCount := 0;
    count := 1;

    if SrcLen >= BUF_SIZE - 1 then begin //源字符串超长，直接退出
      Result := '';
      Dest := '';
      Exit;
    end;

    if Str = '' then begin //源字符串为空，直接退出
      Dest := '';
      Result := Str;
      Exit;
    end;
    ArrCount := SizeOf(Divider) div SizeOf(Char); //字符数组长度

    while True do begin
      if count <= SrcLen then begin
        ch := Str[count];
        for i := 0 to ArrCount - 1 do begin
          if ch = Divider[i] then
            goto CATCH_DIV;
        end;
      end;

      if (count > SrcLen) then begin
CATCH_DIV:
        if (BufCount > 0) then begin
          if BufCount < BUF_SIZE - 1 then begin
            buf[BufCount] := #0;
            Dest := string(buf);
            Result := Copy(Str, count + 1, SrcLen - count);
          end;
          Break;
        end else begin
          if (count > SrcLen) then begin
            Dest := '';
            Result := Copy(Str, count + 2, SrcLen - 1);
            Break;
          end;
        end;
      end else begin
        if BufCount < BUF_SIZE - 1 then begin
          buf[BufCount] := ch;
          Inc(BufCount);
        end;
      end;

      Inc(count);
    end;
  except
    Dest := '';
    Result := '';
  end;
end;

function GetWMIBIOS(WMIProperty: string): string;
var
  Wmi, Objs, Obj: OleVariant;
  Enum: IEnumVariant;
  C: Cardinal;
begin
  Wmi := CreateOleObject('WbemScripting.SWbemLocator');
  Objs := Wmi.ConnectServer('.','root/cimv2').ExecQuery('Select ' + WMIProperty + ' from Win32_BIOS');
  Enum := IEnumVariant(IUnknown(Objs._NewEnum));
  Enum.Reset;
  Enum.Next(1, Obj, C);
  Obj := Obj.Properties_.Item(WMIProperty, 0).Value;
  Result := Obj;
end;

function GetWMIBaseBoard(WMIProperty: string): string;
var
  Wmi, Objs, Obj: OleVariant;
  Enum: IEnumVariant;
  C: Cardinal;
begin
  Wmi := CreateOleObject('WbemScripting.SWbemLocator');
  Objs := Wmi.ConnectServer('.','root/cimv2').ExecQuery('Select ' + WMIProperty + ' from Win32_BaseBoard');
  Enum := IEnumVariant(IUnknown(Objs._NewEnum));
  Enum.Reset;
  Enum.Next(1, Obj, C);
  Obj := Obj.Properties_.Item(WMIProperty, 0).Value;
  Result := Obj;
end;

function GetWMIPhysicalMedia(WMIProperty: string): string;
var
  Wmi, Objs, Obj: OleVariant;
  Enum: IEnumVariant;
  C: Cardinal;
begin
  Wmi := CreateOleObject('WbemScripting.SWbemLocator');
  Objs := Wmi.ConnectServer('.','root/cimv2').ExecQuery('Select ' + WMIProperty + ' from Win32_PhysicalMedia');
  Enum := IEnumVariant(IUnknown(Objs._NewEnum));
  Enum.Reset;
  Enum.Next(1, Obj, C);
  Obj := Obj.Properties_.Item(WMIProperty, 0).Value;
  Result := Obj;
end;

function GetApplicationPath: string;
var
  sPath: string;
begin
  Result := '';
  sPath := ExtractFilePath(Application.ExeName);
  if sPath <> '' then begin
    Result := sPath;
  end;
end;

//进程提权
procedure SetPrivilege;
var
  currToken: THandle;
  newState: TTokenPrivileges;
  prevStateLen: DWORD;
  Luid: TLargeInteger;
begin
  try
    if OpenProcessToken(GetCurrentProcess,TOKEN_ADJUST_PRIVILEGES,currToken) then begin  //获得进程访问令牌的句柄
      if LookupPrivilegeValue(nil, 'SeDebugPrivilege',Luid) then begin
        newState.PrivilegeCount := 1;
        newState.Privileges[0].Attributes := 2;
        newState.Privileges[0].Luid := Luid;
        prevStateLen := 0;
        AdjustTokenPrivileges(currToken, False, newState, SizeOf(TTokenPrivileges),nil, prevStateLen);
        CloseHandle(currToken);
      end;
    end;
  except on e:Exception do
    MessageBox(0,'进程提权时发生了崩溃。','提权失败!', MB_ICONWARNING or MB_SYSTEMMODAL);
  end;
end;

//生成随机字符串
function GenerateRandomString(nCount: Integer): string;
var
  i: Integer;
  ch: Char;
begin
  Randomize; // 初始化随机数种子
  Result := '';
  for i := 1 to nCount do begin
    ch := Chr(Random(26) + Ord('A')); // 生成随机大写字母
    Result := Result + ch;
  end;
end;

end.

unit KeyVerification;

interface

uses IdHTTP, System.SysUtils, System.Classes, System.JSON, System.DateUtils,
  IdHashMessageDigest, Vcl.Forms,
  Winapi.Windows;

type
  // 身份验证
  TAuthentication = class
  private
    //m_appkey: string; // 软件秘钥
    //m_token: string; // 登录返回的token令牌
    //m_cdkey: string; // 登录密钥
  public
    constructor Create;
    destructor Destroy; override;
    function SoftwareInit: Boolean;
    function cdkeyLogin: Boolean;
    function LoadCdkey: string;
    function cdkeyLoginHeartbeat: Boolean;
  end;

var
  g_Authentication: TAuthentication;

const
  _STR_CDK_FILE = 'License.key';

implementation

uses Log, VMProtectSDK, Grobal, Main, GlobalVar;

/// <summary>
/// 执行授权验证：序列号/联网/硬件锁
/// </summary>
/// <returns>
/// True  授权通过
/// False 授权失败
/// </returns>
function TAuthentication.cdkeyLogin: Boolean;
begin
  // TODO: 在此处插入真实授权逻辑，例如：
  // 1. 读取本地序列号文件 → 解密 → 校验格式与哈希
  // 2. 联网到授权服务器 → 对比机器码、到期时间、并发点数
  // 3. 检查 USB/加密狗 是否存在并验证签名
  // 4. 任一环节失败 → Exit(False) 并记录日志
  // 当前**直接返回 True**，任何人启动程序即视为“已授权”
  // 方便开发调试，但发布前必须改掉，否则等于无保护
  Result := True;
end;

function TAuthentication.cdkeyLoginHeartbeat: Boolean;
begin
  Result := True;
end;

constructor TAuthentication.Create;
begin
end;

destructor TAuthentication.Destroy;
begin
  inherited;
end;

//
function TAuthentication.LoadCdkey: string;
begin
end;

function TAuthentication.SoftwareInit: Boolean;
begin
Result := True;
end;

end.

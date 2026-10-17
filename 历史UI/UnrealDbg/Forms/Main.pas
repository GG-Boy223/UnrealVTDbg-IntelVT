{
  注意: RAD Studio 11 编译64位程序时，如果将项目名命名为中文则无法进行调试。
}

unit Main;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants,
  System.Classes, Vcl.Graphics, System.Types, // ← 新增这一行
  Vcl.Controls, Vcl.Forms, Vcl.Dialogs, Vcl.ExtCtrls, Vcl.StdCtrls,
  Vcl.Imaging.jpeg, System.ImageList, Vcl.ImgList, Vcl.Buttons, Vcl.ComCtrls,
  ActiveX, ComObj, GList, System.IniFiles, Vcl.Menus, System.JSON;

type

  TProtectRecord = record
    filename, filePath: string;
  end;

  PTProtectRecord = ^TProtectRecord;

  TDebugger = TProtectRecord;
  PTDebugger = ^TDebugger;

  TForm1 = class(TForm)
    RichEdit1: TRichEdit;
    GroupBox1: TGroupBox;
    Label_SystemName: TLabel;
    Label_SystemVer: TLabel;
    Label_CPU: TLabel;
    GroupBox2: TGroupBox;
    CheckBox1: TCheckBox;
    CheckBox2: TCheckBox;
    CheckBox3: TCheckBox;
    CheckBox4: TCheckBox;
    CheckBox5: TCheckBox;
    CheckBox6: TCheckBox;
    EnterVTDebuggingMode: TButton;
    PageControl1: TPageControl;
    TabSheet1: TTabSheet;
    DbgListView: TListView;
    OpenDialog1: TOpenDialog;
    DebuggerPopupMenu: TPopupMenu;
    StartDebuggerMenu: TMenuItem;
    AddDebuggerMenu: TMenuItem;
    DelDebuggerMenu: TMenuItem;
    TabSheet2: TTabSheet;
    pgcConfrontation: TPageControl;
    TabSheet3: TTabSheet;
    TL_EnabledConfrontat: TCheckBox;
    Image1: TImage;
    Label1: TLabel;
    GroupBox3: TGroupBox;
    TL_HandlerGetTickCountCheck: TCheckBox;
    TL_BlockResumeThread: TCheckBox;
    Label2: TLabel;
    procedure FormCreate(Sender: TObject);
    procedure EnterVTDebuggingModeClick(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure AddDebuggerMenuClick(Sender: TObject);
    procedure DelDebuggerMenuClick(Sender: TObject);
    procedure DbgListViewMouseDown(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
    procedure StartDebuggerMenuClick(Sender: TObject);
    procedure Image1MouseDown(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
    procedure Image1MouseUp(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
    procedure Image1MouseMove(Sender: TObject; Shift: TShiftState; X, Y: Integer);
    procedure TL_EnabledConfrontatClick(Sender: TObject);
    procedure TL_HandlerGetTickCountCheckClick(Sender: TObject);
    procedure TL_BlockResumeThreadClick(Sender: TObject);
    procedure FormCloseQuery(Sender: TObject; var CanClose: Boolean);
  private
    { Private declarations }
    procedure AppOnIdle(Sender: TObject; var Done: Boolean);
  public
    { Public declarations }
    lastHandle: HWND;
    m_SelectedProcessId: DWORD;
    m_targetProcessId: DWORD;


   
    function StartDebugger(szExe: string): Boolean;        
    
    procedure GetWindowsNTVer;
    procedure RefCheckBoxState;
    procedure RefDebuggerListView;
    procedure LoadCheckBoxState;
    procedure SaveCheckBoxState;
    procedure LoadDebuggerList;
    procedure SaveDebuggerList;
    procedure Enabled_TL_Confrontation;
    procedure LoadCopyright;

  private
    m_Caption: string;
    m_Log1: string;
    FTimeTimer: TTimer; // 专门用于刷新标题时间
    procedure TimerUpdateTitle(Sender: TObject); // ← 新增：定时事件
  end;

procedure PrintLog(lpData: Pointer); stdcall;

exports PrintLog;

var
  Form1: TForm1;
  g_DebuggerList: TGList; // 调试器路径列表

const
  _STR_KEY = 'nwkjhdn23e1awe1233'; { 虚幻调试器经16位md5计算得到 }  //nwkjhdn23e1awe1233       //9dd14d00f5dd71bd
  _STR_COPYRIGHT = 'F3copyright.db';
  _STR_TL_EXE = '[TL.exe]';
  _STR_DEBUGGER_INI = 'DebuggerList.ini';
  _STR_CONFIG_INI = 'Config.ini';
  _STR_STARTUP_INFO_INI = 'StartupInfo.ini';
  crScope = 1; // Delphi的光标状态都是负数，所以定义正数就可以避免与系统的光标冲突

  // ----------------------------------------------------------------------------------------

implementation

// procedure      无返回值
// function       有返回值

uses Log, UnrealDbgDll, Grobal, LockThread, HandlerTLDetection, EventHandlerThread,
  GlobalVar, KeyVerification, VMProtectSDK, D_encryptionDll;

{$R *.dfm}     // 把窗体资源打包进程序” 的开关

// 导出函数供外部模块调用
procedure PrintLog(lpData: Pointer); stdcall;
begin
  try
    sLog.outInfo(WideCharToString(lpData));
  except
    on e: Exception do
      sLog.outError('[PrintLog]===>' + e.Message);
  end;
end;

{ 高度，宽度窗口 }
procedure HighlightWindowBorder(Handle: HWND);
var
  DC: HDC;
  Rect: TRect;
  Pen: HGDIOBJ;
  Rgn: HRGN;
  Brush: HBRUSH;
  SysColor: DWORD;
  original_pen: HGDIOBJ;
  original_brush: HGDIOBJ;
  WndWidth: Integer;
  WndHeight: Integer;
  frameWidth: Integer;
  frameHeight: Integer;
  screen_Width: Integer;
  screen_height: Integer;
begin
  if Handle <> 0 then
  begin
    DC := GetWindowDC(Handle);
    if DC <> 0 then
    begin
      try
        WndWidth := GetSystemMetrics(SM_CXBORDER); // 窗口边框的宽度（以像素为单位）
        WndHeight := GetSystemMetrics(SM_CYBORDER); // 窗口边框的高度（以像素为单位）

        Rgn := CreateRectRgn(0, 0, 0, 0);
        Pen := CreatePen(PS_INSIDEFRAME, 3 * WndWidth, RGB(0, 0, 0)); // 创建画笔
        original_pen := SelectObject(DC, Pen);
        original_brush := SelectObject(DC, GetStockObject(NULL_BRUSH));
        // 选择一个空心画刷 NULL_BRUSH
        SetROP2(DC, R2_NOT);
        if GetWindowRgn(Handle, Rgn) <> 0 then
        begin
          SysColor := GetSysColor(COLOR_WINDOWFRAME);
          Brush := CreateHatchBrush(HS_DIAGCROSS, SysColor); // 45 度交叉影线
          FrameRgn(DC, Rgn, Brush, 3 * WndWidth, 3 * WndHeight);
          DeleteObject(Brush);
        end
        else
        begin
          frameWidth := GetSystemMetrics(SM_CXFRAME);
          // SM_CXFRAME 边框的宽度（以像素为单位）
          frameHeight := GetSystemMetrics(SM_CYFRAME);
          // SM_CYFRAME 边框的高度（以像素为单位）
          screen_Width := GetSystemMetrics(SM_CXSCREEN);
          // SM_CXSCREEN 主显示器的屏幕宽度（以像素为单位）
          screen_height := GetSystemMetrics(SM_CYSCREEN);
          // SM_CYSCREEN 主显示器的屏幕高度（以像素为单位）
          GetWindowRect(Handle, Rect); // 获取窗口尺寸

          // 检查窗口是否最大化
          if IsZoomed(Handle) then
          begin
            Rectangle(DC, frameWidth, frameHeight, frameWidth + screen_Width,
              screen_height + frameHeight);
          end
          else
          begin
            Rectangle(DC, 0, 0, Rect.right - Rect.left, Rect.bottom - Rect.top);
            // 绘制矩形边框 使用当前笔勾勒矩形轮廓，使用当前画刷填充矩形。
          end;
        end;
        SelectObject(DC, original_brush); // 还原画刷
        SelectObject(DC, original_pen); // 还原画笔
        DeleteObject(Pen);
        DeleteObject(Rgn);

      finally
        ReleaseDC(Handle, DC);
      end;
    end;
  end;
end;

// 清理边框
procedure CleanBorder(Handle: HWND);
begin
  HighlightWindowBorder(Handle);
end;

// 启动TL对抗
procedure TForm1.Enabled_TL_Confrontation;
var
  base: DWORD_PTR;
begin
  if m_targetProcessId <> 0 then
  begin
    if g_boEnabled_tl_confrontation_TL then
    begin
      if g_boHandlerGetTickCountCheck_TL then
      begin
        sLog.outDebug(_STR_TL_EXE + ' 正在处理GetTickCount检测...');

        // 判断之前是否已经创建
        if Assigned(g_LockThread) then
        begin
          g_LockThread.Stop;
          g_LockThread.WaitFor;
          g_LockThread.Free;
          g_LockThread := nil;
        end;
        base := GetProcessModuleBase(m_targetProcessId, 'TL.exe');
        Handler_TLDetection(base); // 处理TL的检测
      end;

      if g_boBlockResumeThread_TL then
      begin
        sLog.outDebug(_STR_TL_EXE + ' 阻止游戏恢复线程');
        Unreal_TL_BlockGameResumeThread(m_targetProcessId);
      end;
    end;
  end;
end;

// 按钮开启VT调试}
procedure TForm1.EnterVTDebuggingModeClick(Sender: TObject);
begin
  // 1. 标记 VMProtect 虚拟化保护开始，防止逆向分析这段代码
  VMProtectBeginVirtualization('VMP');
  try
    // 2. 首先判断全局变量：是否已通过登录（授权）验证
    // g_boLoginSuccess 在 TAuthentication.cdkeyLogin 成功后置 True
    if g_boLoginSuccess then
    begin
      // 3. 置位“服务已启动”标志，后续各模块据此决定是否继续工作
      g_boStartService := True;

      // 4. 禁用按钮，防止用户重复点击造成重复初始化
      EnterVTDebuggingMode.Enabled := False;

      if not UnrealDbgDllReady then
      begin
        sLog.outError('UnrealDbgDll 未能成功加载: ' + UnrealDbgDllLastError);
        EnterVTDebuggingMode.Enabled := True;
        g_boStartService := False;
      end
      else
      begin
        // 5. 调用驱动层初始化函数，传入硬编码授权码 0x9dd14d00f5dd71bd
        // 返回值：True = 驱动加载/通信成功；False = 失败
        if not Unreal_Initialize($9DD14D00F5DD0000) then      //9DD14D00F5DD0000           //9DD14D00F5DD71BD
        begin
          // 6. 【输出日志】授权成功，但驱动级初始化失败
          // 常见原因：驱动未签名、未加载、系统不兼容、授权码错误
          sLog.outError('设备未能成功运行!');
          EnterVTDebuggingMode.Enabled := True;
          g_boStartService := False;
        end;
      end;
      // 7. 若 Unreal_Initialize 返回 True，则全程无日志输出，静默进入 VT 调试模式
    end
    else
    begin
      // 8. 【输出日志】登录/授权验证未通过（cdkeyLogin 失败）
      // 此时不会尝试加载驱动，直接提示用户
      sLog.outError('未通过授权认证!');
    end;
  except
    // 9. 【输出日志】try 块里任何一步抛出异常（如驱动接口不存在、内存访问违规等）
    // 捕获异常并记录，防止程序崩溃
    on e: Exception do
    begin
      sLog.outError('[TForm1.EnterVTDebuggingModeClick]===>' + e.Message);
      EnterVTDebuggingMode.Enabled := True;
      g_boStartService := False;
    end;
  end;
  // 10. 结束 VMProtect 虚拟化保护区域
  VMProtectEnd;
end;

// 关闭窗口}
procedure TForm1.FormCloseQuery(Sender: TObject; var CanClose: Boolean);
var
  btn: Integer;
begin
  try
    if g_boStartService then
    begin
      btn := Application.MessageBox('您确定要关闭程序窗口吗？只有重启电脑才能完全退出VT调试模式!', '警告:',
        MB_YESNO or MB_ICONWARNING or MB_SYSTEMMODAL);
      if btn = ID_YES then
      begin
        CanClose := True;
      end
      else
      begin
        CanClose := False;
      end;
    end
    else
    begin
      CanClose := True;
    end;
  except
    on e: Exception do
      sLog.outError('[TForm1.FormCloseQuery]===>' + e.Message);
  end;
end;

{ -------------------------------------------------------------------------------
  过程: TForm1.FormCreate
  功能: 主窗口创建时的一次性初始化入口
  说明:
  1. 绑定日志控件 → 输出欢迎语 → 加载系统信息
  2. 加载版权文件(失败则直接退出进程)
  3. 设置自定义光标
  4. 全局标志初始化
  5. VMProtect 虚拟化保护区内：
  - 负责日志绑定、欢迎语输出、系统信息收集、授权验证、驱动前置条件准备
  - 若授权失败则仅记录日志，不创建线程与列表，后续功能被禁用
  ------------------------------------------------------------------------------- }
procedure TForm1.FormCreate(Sender: TObject);
begin
  // ---- 日志控件初始化 ----
  RichEdit1.Clear; // 清空残留日志
  sLog.setLog(RichEdit1); // 把 RichEdit1 绑定到日志管理器

  // ---- 注册空闲事件 → 用于实时刷日志队列 ----
  Application.OnIdle := AppOnIdle;

  // ---- 版权加载：解密 copyright.db → 得窗口标题与群号 ----
  // 若文件不存在或解密失败会弹框并 ExitProcess(0)
  LoadCopyright;
  Caption := m_Caption + '' + DateTimeToStr(Now); // 立即更新标题栏

  // ---- 固定欢迎语（一定会输出） ----
  sLog.outDebug('本软件目前免费使用，如果你是购买的，则说明你被骗了');
  //sLog.outDebug('by: Bug工程师,由 F3 进行修改编译。');
  sLog.outDebug('虚幻调试器 QQ号：3913268575');
  //sLog.outDebug(m_Log1); // 含 QQ 群号
    sLog.outDebug('--------------------------------------------------------------');
  //sLog.outDebug('感谢！微雨问海棠，界面参考的HvmDbg！');
  sLog.outDebug('使用本软件，对您造成的直接或者间接的损失，由您自行承担！');
  sLog.outDebug('支持Win10_22h2 -- Win11_22h2 其他版本自行测试; ');
  sLog.outDebug('说明：1.通过调试器启动的CE，可过检测及其他检测;');
  sLog.outDebug('      2.tool文件夹已经提供了 CE 和 xebug;');
  sLog.outDebug('      3.支持查找地址访问,int3断点调试;');
  sLog.outDebug('--------------------------------------------------------------');
  // ---- 系统信息收集：OS 版本、内核文件版本、CPU 名称 ----
  // 失败时写日志 "[TForm1.GetWindowsNTVer]===>异常信息"
  GetWindowsNTVer;

  // ---- 加载自定义光标 → 供拖拽选进程时使用 ----
  Screen.Cursors[crScope] := LoadCursor(HInstance, 'Cursor_1');

  // ---- 全局状态标志初始化 ----
  g_boStartService := False; // VT 调试服务尚未启动
  g_boLoginSuccess := True; // 硬编码：永远当成“已登录”
  // 产品化必须改为：g_boLoginSuccess := g_Authentication.cdkeyLogin;

  // ---- VMProtect 虚拟化保护开始 ----
  VMProtectBeginVirtualization('VMP');
  try
    // 创建授权对象并立即验证
    g_Authentication := TAuthentication.Create;
    if g_Authentication.cdkeyLogin then // 当前永远返回 True
    begin
      // 授权通过 → 提权 + 创建线程/列表 + 加载配置
      SetPrivilege; // 内部会写“提权成功/失败”日志
      g_DebuggerList := TGList.Create; // 线程安全的调试器路径列表
      g_EventHandlerThread := TEventHandlerThread.Create(False); // 后台事件线程
      LoadDebuggerList; // 从 DebuggerList.ini 加载历史
      LoadCheckBoxState; // 从 Config.ini 加载 TL 对抗复选框状态
    end;
  finally
    VMProtectEnd; // 结束虚拟化保护
  end;

  FTimeTimer := TTimer.Create(nil);
  FTimeTimer.Interval := 1000; // 1 秒
  FTimeTimer.OnTimer := TimerUpdateTitle;
  FTimeTimer.Enabled := True; // 立即开始

end;

// 过程: TForm1.LoadCopyright  功能: 解密 copyright.db → 读取窗口标题与群
procedure TForm1.LoadCopyright;
var
  sFileName, sPath: string; // 完整路径与文件名
  decryptedDataLen: Integer; // 第一次调用：获取解密后字节数
  PlainText: array of Char; // 动态字符数组：存解密后 JSON
  PlainTextLen: Integer; // 字符个数（含 #0）
  jsonStr: string; // 最终 JSON 文本
  JSON: TJSONObject; // 解析器对象
begin
  try
    // 1. 给默认值（版权文件不存在时仍显示基础文字）
    m_Caption := 'F3虚幻调试器  || QQ号：3913268575 || ';
    m_Log1 := 'F3虚幻调试器 QQ号：3913268575';

    // 2. 取当前 EXE 目录
    sPath := ExtractFilePath(Application.ExeName);
    if sPath <> '' then
    begin
      sFileName := sPath + _STR_COPYRIGHT; // 拼接文件名

      // 3. 文件存在才继续
      if FileExists(sFileName) then
      begin
        // 4. 第一次传 nil 拿所需缓冲区字节数（含末尾 #0）
        decryptedDataLen := D_encryption_DecryptDataFromFile(sFileName,
          _STR_KEY, nil);

        if decryptedDataLen > 0 then
        begin
          // 5. 字符数 = 字节数 / 2 + 1（宽字符）
          PlainTextLen := (decryptedDataLen div 2) + 1;
          SetLength(PlainText, PlainTextLen);

          // 6. 清零缓冲区，避免旧数据干扰
          FillChar(PlainText[0], Length(PlainText) * SizeOf(Char), #0);

          // 7. 第二次真正解密，内容写入 PlainText
          decryptedDataLen := D_encryption_DecryptDataFromFile(sFileName,
            _STR_KEY, @PlainText[0]);
           //sLog.outInfo('解密长度: ' + decryptedDataLen.ToString);

          // 8. PChar 转 string，得到完整 JSON 文本
          jsonStr := string(PChar(@PlainText[0])); // 赋值运算符

          // 9. 解析 JSON 并提取字段
          JSON := JSON.ParseJSONValue(jsonStr) as TJSONObject;
          //sLog.outInfo('解密长度: ' + JSON.ToString);
          m_Caption := JSON.Values['虚幻调试器标题'].AsType<string>;
          m_Log1 := JSON.Values['QQ群日志'].AsType<string>;

          //sLog.outInfo('m_Caption: ' + m_Caption);
          // 10. 释放资源
          JSON.Free;
          SetLength(PlainText, 0);
            //MessageBox(0, '获取版权信息失败!  联系QQ: 3913268575', '错误! ', MB_ICONERROR);
        end
        else
            begin
                    MessageBox(0, '获取版权信息失败!  联系QQ: 3913268575', '错误! ', MB_ICONERROR);
                    ExitProcess(0);
            end;
      end
      else
      begin
            MessageBox(0, '获取版权信息失败!  联系QQ: 3913268575', '错误! ', MB_ICONERROR);
            ExitProcess(0);
      end;
    end;
  except
    on e: Exception do
    begin
      // 11. 任何异常 → 弹框+强制退出进程
      MessageBox(0, '获取版权信息失败!  联系QQ: 3913268575', '错误! ', MB_ICONERROR);
      ExitProcess(0);
    end;
  end;
end;

procedure TForm1.AppOnIdle(Sender: TObject; var Done: Boolean);
begin
  try
    sLog.OutputMsg;
    Done := False;
  except
    on e: Exception do
  end;
end;

procedure TForm1.RefCheckBoxState;
begin
  try
    TL_EnabledConfrontat.Checked := g_boEnabled_tl_confrontation_TL;
    TL_HandlerGetTickCountCheck.Checked := g_boHandlerGetTickCountCheck_TL;
    TL_BlockResumeThread.Checked := g_boBlockResumeThread_TL;
  except
    on e: Exception do
      sLog.outError('[TForm1.RefCheckBoxState]===>' + e.Message);
  end;
end;

procedure TForm1.RefDebuggerListView;
var
  I: Integer;
  pTemp: PTDebugger;
  Item: TListItem;
begin
  try
    DbgListView.Clear;
    g_DebuggerList.Lock;
    try
      for I := 0 to g_DebuggerList.Count - 1 do
      begin
        pTemp := g_DebuggerList.Items[I];
        if Assigned(pTemp) then
        begin
          Item := DbgListView.Items.Add;
          Item.Caption := IntToStr(DbgListView.Items.Count); // 序号
          Item.SubItems.Add(pTemp.filename); // 文件名
          Item.SubItems.Add(pTemp.filePath); // 目录
        end;
      end;
    finally
      g_DebuggerList.UnLock;
    end;
  except
    on e: Exception do
      sLog.outError('[TForm1.RefDebuggerListView]===>' + e.Message);
  end;
end;

// 创建文件副本
function CreateCopyFile(filename: string; filePath: string): string;
var
  iniFile: TIniFile;
  sPath, sfixedPath: string;
  PrefixName, suffixName, sName: string;
begin
  try
    Result := '';
    sPath := ExtractFilePath(filePath);
    sfixedPath := sPath;
    if sPath <> '' then
    begin
      sPath := sPath + _STR_STARTUP_INFO_INI;

      iniFile := TIniFile.Create(sPath);
      if FileExists(sPath) then
      begin
        // 取出原文件名
        sName := iniFile.ReadString(filename, 'fileName', '');
        sName := sfixedPath + sName;
        DeleteFile(sName); // 删除旧文件
      end;

      PrefixName := GenerateRandomString(8);
      suffixName := GenerateRandomString(3);
      sName := PrefixName + '.' + suffixName;
      iniFile.WriteString(filename, 'fileName', sName);

      sName := sfixedPath + sName;
      CopyFile(PChar(filePath), PChar(sName), False);
      Result := sName;
      iniFile.Free;
    end;
  except
    on e: Exception do
      sLog.outError('[CreateCopyFile]===>' + e.Message);
  end;
end;


 // 检测是否已经存在相同配置
function DebuggerItemExists(filename, filePath: string): Boolean;
var
  I: Integer; // 循环索引
  pTemp: PTDebugger; // 临时指针，指向当前遍历到的调试器记录
begin
  try
    Result := False; // 默认认为不存在相同项
    // 只有当文件名和路径都不为空时才继续检查
    if (filename <> '') and (filePath <> '') then
    begin
      g_DebuggerList.Lock; // 进入临界区，防止其他线程同时修改列表
      try
        // 遍历整个调试器列表
        for I := 0 to g_DebuggerList.Count - 1 do
        begin
          pTemp := g_DebuggerList.Items[I]; // 取出第 I 个指针
          // 非空指针才继续比较
          if pTemp <> nil then
          begin
            // 同时比对文件名和完整路径
            if (pTemp.filename = filename) and (pTemp.filePath = filePath) then
            begin
              Result := True; // 找到完全相同的配置
              Break; // 立即退出循环，不再继续扫描
            end;
          end;
        end;
      finally
        g_DebuggerList.UnLock; // 离开临界区，释放锁
      end;
    end;
  except
    on e: Exception do
    begin
      // 异常路径也要显式赋值，彻底关闭警告
      Result := False;
      // 捕获任何异常并写日志，防止函数内部错误向上扩散
      sLog.outError('[DebuggerItemExists]===>' + e.Message);
    end;
  end;
end;

function TForm1.StartDebugger(szExe: string): Boolean;
begin
  Result := False;
  if not UnrealDbgDllReady then
  begin
    sLog.outError('UnrealDbgDll 未能成功加载: ' + UnrealDbgDllLastError);
    Exit;
  end;

  if Unreal_StartProcess(PChar(szExe), PChar(GetApplicationPath)) then
  begin
    Result := True;
  end;
end;

{ ----------------------------------------------------
  过程: TForm1.StartDebuggerMenuClick
  功能: 右键菜单“启动调试器”入口
  说明: 取 ListView 当前选中行 → 提取保存的完整路径 → 直接启动
  ---------------------------------------------------- }
procedure TForm1.StartDebuggerMenuClick(Sender: TObject);
var
  // pTemp: PTDebugger; // 预留结构体指针（本例未用）
  // I: Integer; // 预留循环变量（本例未用）
  // sNewFile, // 预留副本路径（已注释）
  filename, // 调试器文件名（仅显示）
  filePath: string; // 调试器完整路径（真正使用）
  Item: TListItem; // 当前选中行对象
begin
  try
    // 1. 有选中行才继续
    if Assigned(DbgListView.Selected) then
    begin
      // 2. 取出 TListItem 对象
      Item := DbgListView.Items.Item[DbgListView.ItemIndex];
      if Assigned(Item) then
      begin
        // 3. 从 SubItems 读“文件名”与“完整路径”
        filename := Item.SubItems.Strings[0]; // 仅用于显示/日志
        filePath := Item.SubItems.Strings[1]; // 真正要启动的 EXE
        if (filename <> '') and (filePath <> '') then
        begin
          // 4. 直接启动原文件（副本逻辑已注释）
          StartDebugger(filePath);
          {
            // 以下代码预留：先生成随机副本再启动
            // sNewFile := CreateCopyFile(fileName,filePath);
            // if sNewFile <> '' then
            //   StartDebugger(sNewFile);
          }
        end;
      end;
    end;
  except
    // 5. 捕获任何异常并写日志
    on e: Exception do
      sLog.outError('[TForm1.StartDebuggerMenuClick]===>' + e.Message);
  end;
end;

// 过程: DbgListViewMouseDown  功能: 列表视图鼠标按下事件（用于弹出右键菜单）
// 参数: Sender 固定为 DbgListView    Button=mbRight 时触发菜单
// 有选中项才启用“删除”菜单项，否则禁用
procedure TForm1.DbgListViewMouseDown(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
var
  curpos: TPoint; // 屏幕坐标
begin
  try
    if Button = mbRight then // 只响应右键
    begin
      // 根据是否有选中项启用/禁用“删除”菜单
      DelDebuggerMenu.Enabled := Assigned(TListView(Sender).Selected);

      // 在鼠标箭头右下方弹出菜单（+10 像素偏移避免遮挡）
      GetCursorPos(curpos);
      DebuggerPopupMenu.Popup(curpos.X + 10, curpos.Y);
    end;
  except
    on e: Exception do
      sLog.outError('[TForm1.DbgListViewMouseDown]===>' + e.Message);
  end;
end;

// 移除选中的项
procedure TForm1.DelDebuggerMenuClick(Sender: TObject);
var
  pTemp: PTDebugger;
  I: Integer;
  sText, filename, filePath: string;
  Item: TListItem;
begin
  try
    if Assigned(DbgListView.Selected) then
    begin
      Item := DbgListView.Items.Item[DbgListView.ItemIndex];
      if Assigned(Item) then
      begin
        filename := Item.SubItems.Strings[0];
        filePath := Item.SubItems.Strings[1];
        if (filename <> '') and (filePath <> '') then
        begin
          if Application.MessageBox('是否确认移除选中的项？', '确认信息',
            MB_YESNO + MB_ICONQUESTION) = IDYES then
          begin
            g_DebuggerList.Lock;
            try
              for I := g_DebuggerList.Count - 1 downto 0 do
              begin
                pTemp := g_DebuggerList.Items[I];
                if pTemp <> nil then
                begin
                  if (pTemp.filename = filename) and (pTemp.filePath = filePath)
                  then
                  begin
                    g_DebuggerList.Remove(pTemp);
                    Dispose(pTemp);
                    Break;
                  end;
                end
                else
                begin
                  g_DebuggerList.Delete(I);
                end;
              end;
            finally
              g_DebuggerList.UnLock;
            end;
            SaveDebuggerList;
            RefDebuggerListView;
          end;
        end;
      end;
    end;
  except
    on e: Exception do
      sLog.outError('[TForm1.DelDebuggerMenuClick]===>' + e.Message);
  end;
end;

// 添加列表项
procedure TForm1.AddDebuggerMenuClick(Sender: TObject);
var
  filename, filePath: string;
  pTemp: PTDebugger;
begin
  try
    OpenDialog1.Title := '请选择调试器';
    OpenDialog1.filename := '';
    OpenDialog1.Execute;
    filename := ExtractFileName(OpenDialog1.filename); // 提取文件名
    filePath := OpenDialog1.filename; // 提取文件路径

    if (filename <> '') and (filePath <> '') then
    begin
      if not DebuggerItemExists(filename, filePath) then
      begin
        New(pTemp);
        FillChar(pTemp^, SizeOf(TDebugger), #0);
        pTemp.filename := filename;
        pTemp.filePath := filePath;

        g_DebuggerList.Lock;
        try
          g_DebuggerList.Add(pTemp);
        finally
          g_DebuggerList.UnLock;
        end;
        SaveDebuggerList;
        RefDebuggerListView;
      end
      else
      begin
        Application.MessageBox('该输入项已经存在于列表中..', '警告:', MB_ICONWARNING);
      end;
    end;
  except
    on e: Exception do
      sLog.outError('[TForm1.AddDebuggerMenuClick]===>' + e.Message);
  end;
end;

// ----------------------------------------------------------------
// 单击“阻止游戏恢复线程”复选框时触发
procedure TForm1.TL_BlockResumeThreadClick(Sender: TObject);
begin
  // 1. 将 CheckBox 当前勾选状态写入全局变量（供后续对抗逻辑使用）
  g_boBlockResumeThread_TL := TL_BlockResumeThread.Checked;

  // 2. 立即保存配置，确保重启程序后复选框保持上次选择
  SaveCheckBoxState;
end;

// 单击“启用 TL 对抗”复选框
procedure TForm1.TL_EnabledConfrontatClick(Sender: TObject);
begin
  g_boEnabled_tl_confrontation_TL := TL_EnabledConfrontat.Checked;
  SaveCheckBoxState;
end;

// 功能: 单击“处理 GetTickCount 检测”复选框
procedure TForm1.TL_HandlerGetTickCountCheckClick(Sender: TObject);
begin
  g_boHandlerGetTickCountCheck_TL := TL_HandlerGetTickCountCheck.Checked;
  SaveCheckBoxState;
end;

// ----------------------------------------------------------------
procedure TForm1.LoadDebuggerList;
var
  Config: TIniFile;
  sPath, sText: string;
  ItemCount: Integer;
  I: Integer;
  filename, filePath: string;
  pTemp: PTDebugger;
begin
  try
    sPath := ExtractFilePath(Application.ExeName);
    if sPath <> '' then
    begin
      sPath := sPath + _STR_DEBUGGER_INI;
      if FileExists(sPath) then
      begin
        Config := TIniFile.Create(sPath);
        ItemCount := Config.ReadString('配置', 'Count', '0').ToInteger;
        for I := 0 to ItemCount - 1 do
        begin
          sText := Config.ReadString('配置', 'Debugger' + I.ToString, '');
          if sText <> '' then
          begin
            sText := GetValidStr3(sText, filename, ['&']);
            sText := GetValidStr3(sText, filePath, ['&']);

            if (filename <> '') and (filePath <> '') then
            begin
              New(pTemp);
              FillChar(pTemp^, SizeOf(TDebugger), #0);
              pTemp.filename := filename;
              pTemp.filePath := filePath;

              g_DebuggerList.Lock;
              try
                g_DebuggerList.Add(pTemp);
              finally
                g_DebuggerList.UnLock;
              end;
              RefDebuggerListView;
            end;
          end;
        end;
        Config.Free;
      end;
    end;
  except
    on e: Exception do
      sLog.outError('[TForm1.LoadDebuggerList]===>' + e.Message);
  end;
end;

procedure TForm1.SaveDebuggerList;
var
  Config: TMemIniFile;
  sPath, sTempPath: string;
  I: Integer;
  pTemp: PTDebugger;
begin
  try
    sPath := ExtractFilePath(Application.ExeName);
    if sPath <> '' then
    begin
      sPath := sPath + _STR_DEBUGGER_INI;
      sTempPath := sPath + '.tmp';
      if FileExists(sTempPath) and not DeleteFile(sTempPath) then
        RaiseLastOSError;

      Config := TMemIniFile.Create(sTempPath);
      try
        g_DebuggerList.Lock;
        try
          Config.WriteString('配置', 'Count', g_DebuggerList.Count.ToString);
          for I := 0 to g_DebuggerList.Count - 1 do
          begin
            pTemp := g_DebuggerList.Items[I];
            if Assigned(pTemp) then
            begin
              Config.WriteString('配置', 'Debugger' + I.ToString,
                pTemp.filename + '&' + pTemp.filePath);
            end;
          end;
        finally
          g_DebuggerList.UnLock;
        end;

        Config.UpdateFile;
      finally
        Config.Free;
      end;

      if not MoveFileEx(PChar(sTempPath), PChar(sPath),
        MOVEFILE_REPLACE_EXISTING or MOVEFILE_WRITE_THROUGH) then
        RaiseLastOSError;
    end;
  except
    on e: Exception do
      sLog.outError('[TForm1.SaveDebuggerList]===>' + e.Message);
  end;
end;

procedure TForm1.LoadCheckBoxState;
var
  Config: TIniFile;
  sPath: string;
begin
  try
    sPath := ExtractFilePath(Application.ExeName);
    if sPath <> '' then
    begin
      sPath := sPath + _STR_CONFIG_INI;
      if FileExists(sPath) then
      begin
        Config := TIniFile.Create(sPath);
        g_boEnabled_tl_confrontation_TL :=
          Config.ReadBool('TL', 'enabled_tl_confrontation',
          g_boEnabled_tl_confrontation_TL);
        g_boHandlerGetTickCountCheck_TL :=
          Config.ReadBool('TL', 'handler_gettickcount_check',
          g_boHandlerGetTickCountCheck_TL);
        g_boBlockResumeThread_TL := Config.ReadBool('TL', 'BlockResumeThread',
          g_boBlockResumeThread_TL);
        Config.Free;
      end;
    end;
    RefCheckBoxState;
  except
    on e: Exception do
      sLog.outError('[TForm1.LoadCheckBoxState]===>' + e.Message);
  end;
end;

procedure TForm1.SaveCheckBoxState;
var
  Config: TIniFile;
  sPath: string;
begin
  try
    sPath := ExtractFilePath(Application.ExeName);
    if sPath <> '' then
    begin
      sPath := sPath + _STR_CONFIG_INI;

      Config := TIniFile.Create(sPath);
      Config.WriteBool('TL', 'enabled_tl_confrontation',
        g_boEnabled_tl_confrontation_TL);
      Config.WriteBool('TL', 'handler_gettickcount_check',
        g_boHandlerGetTickCountCheck_TL);
      Config.WriteBool('TL', 'BlockResumeThread', g_boBlockResumeThread_TL);
      Config.Free;
    end;
  except
    on e: Exception do
      sLog.outError('[TForm1.SaveCheckBoxState]===>' + e.Message);
  end;
end;

// ----------------------------------------------------------------
// 按下左键,把图片换成“空”图，切换光标
procedure TForm1.Image1MouseDown(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
var
  ResourceStream: TResourceStream;
begin
  if (ssLeft in Shift) then // // 只响应左键按下，其余键忽略
  begin
    // 从资源加载第一个图片
    ResourceStream := TResourceStream.Create(HInstance, 'Empty', RT_RCDATA);
    try
      Image1.Picture.Graphic.LoadFromStream(ResourceStream); // 换图
      SetCursor(Screen.Cursors[crScope]);
      /// / 换成十字准星光标
    finally
      ResourceStream.Free;
    end;
  end;
end;

// 拖拽中 实时高亮鼠标下的外部窗口
procedure TForm1.Image1MouseMove(Sender: TObject; Shift: TShiftState;
  X, Y: Integer);
var
  Handle: HWND; // 当前鼠标下的窗口句柄
  Point: TPoint; // 屏幕坐标
  SelfProcessId: DWORD; // 本进程 PID，用于排除自己
  targetProcessId: DWORD; // 目标进程 PID
begin
  // 检查鼠标左键是否被按下
  if (ssLeft in Shift) then
  begin

    // 获取鼠标当前位置
    if GetCursorPos(Point) then
    begin
      // 根据屏幕坐标拿到窗口句柄（WindowFromPoint 不会返回子窗口）
      Handle := WindowFromPoint(Point);
      if (Handle <> 0) and (Handle <> lastHandle) then // 排除本进程窗口，避免高亮自己
      begin
        GetWindowThreadProcessId(Application.Handle, @SelfProcessId); // 自己
        GetWindowThreadProcessId(Handle, @targetProcessId); // 目标
        if targetProcessId <> SelfProcessId then // 不是本进程且与上次不同才重画边框，防止闪烁

        begin
          CleanBorder(lastHandle); // 清除旧边框（再画一次反色）
          HighlightWindowBorder(Handle); // 画新边框（反色矩形）
          lastHandle := Handle; // 记录当前句柄
          m_SelectedProcessId := targetProcessId; // 记录目标 PID

          // sLog.outInfo('已选择目标 PID: ' + m_SelectedProcessId.ToString);
        end;
      end;
    end;
  end;
end;

// 松开左键,   确认选择，记录目标 PID，恢复图片
procedure TForm1.Image1MouseUp(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
var
  ResourceStream: TResourceStream;
  sText: string;
  btn: Integer;
  MsgRcd: TMsgRcd;
begin
  try
    if Button = mbLeft then
    begin
      // 切换图片
      if Assigned(Image1.Picture.Graphic) then
      begin
        ResourceStream := TResourceStream.Create(HInstance, 'Original',
          RT_RCDATA);
        try
          Image1.Picture.Graphic.LoadFromStream(ResourceStream);
        finally
          ResourceStream.Free;
        end;
      end;

      CleanBorder(lastHandle);
      lastHandle := 0;

      if m_SelectedProcessId <> 0 then
      begin
        if g_boEnabled_tl_confrontation_TL then
        begin
          sText := 'pid: ' + m_SelectedProcessId.ToString + ' 是TL的进程吗？';
          btn := MessageBox(0, PChar(sText), '选择目标进程:',
            MB_YESNO or MB_SYSTEMMODAL);
          if btn = IDYES then
          begin
            m_targetProcessId := m_SelectedProcessId;
            sLog.outInfo(_STR_TL_EXE + ' pid: ' + m_targetProcessId.ToString);
            MsgRcd.nAction := EVENT_ENABLED_TL_CONFRONTATION;
            g_EventHandlerThread.SendMsg(MsgRcd);
          end;
        end;
      end;
      m_SelectedProcessId := 0;
    end;
  except
    on e: Exception do
      sLog.outError('[TForm1.Image1MouseUp]===>' + e.Message);
  end;
end;

// ----------------------------------------------------------------
function GetWMIOperatingSystem(WMIProperty: string): string;
var
  Wmi, Objs, Obj: OleVariant;
  Enum: IEnumVariant;
  C: Cardinal;
begin
  Wmi := CreateOleObject('WbemScripting.SWbemLocator');
  Objs := Wmi.ConnectServer('.', 'root/cimv2')
    .ExecQuery('Select ' + WMIProperty + ' from Win32_OperatingSystem');
  Enum := IEnumVariant(IUnknown(Objs._NewEnum));
  Enum.Reset;
  Enum.Next(1, Obj, C);
  Obj := Obj.Properties_.Item(WMIProperty, 0).Value;
  Result := Obj;
end;

function GetWMIProcessor(WMIProperty: string): string;
var
  Wmi, Objs, Obj: OleVariant;
  Enum: IEnumVariant;
  C: Cardinal;
begin
  Wmi := CreateOleObject('WbemScripting.SWbemLocator');
  Objs := Wmi.ConnectServer('.', 'root/cimv2')
    .ExecQuery('Select ' + WMIProperty + ' from Win32_Processor');
  Enum := IEnumVariant(IUnknown(Objs._NewEnum));
  Enum.Reset;
  Enum.Next(1, Obj, C);
  Obj := Obj.Properties_.Item(WMIProperty, 0).Value;
  Result := Obj;
end;

{ 系统信息收集：OS 版本、内核文件版本、CPU 名称 }
procedure TForm1.GetWindowsNTVer;
var
  VerInfo: array [0 .. 64] of Char;
  WinDir: array [0 .. 255] of Char;
  ntoskrnl, OSText: string;
begin
  try
    OSText := GetWMIOperatingSystem('Caption') + GetWMIOperatingSystem
      ('Version') + '  ' + GetWMIProcessor('AddressWidth') + '位操作系统';
    Label_SystemName.Caption := Label_SystemName.Caption + '  ' + OSText;

    FillChar(WinDir[0], Length(WinDir) * 2, #0);
    GetSystemDirectory(WinDir, Length(WinDir));
    ntoskrnl := WinDir + '\ntoskrnl.exe';
    FillChar(VerInfo[0], Length(VerInfo) * 2, #0);
    if not UnrealDbgDllReady then
    begin
      sLog.outError('UnrealDbgDll 未能成功加载: ' + UnrealDbgDllLastError);
      Exit;
    end;

    Unreal_GetFileVersion(PChar(ntoskrnl), @VerInfo);
    Label_SystemVer.Caption := Label_SystemVer.Caption + '  ntoskrnl.exe  '
      + VerInfo;

    Label_CPU.Caption := Label_CPU.Caption + '  ' + GetWMIProcessor('Name');

  except
    on e: Exception do
      sLog.outError('[TForm1.GetWindowsNTVer]===>' + e.Message);
  end;
end;

// ----------------------------------------------------------------
procedure TForm1.TimerUpdateTitle(Sender: TObject);
begin
  // 每秒刷新窗口标题
  Caption := m_Caption + '  ' + FormatDateTime('yyyy-mm-dd hh:nn:ss', Now);
end;

// 释放资源之前，一定要先释放线程
procedure TForm1.FormDestroy(Sender: TObject);
var
  I: Integer;
begin
  // 1. 先停掉定时器（防止触发事件）
  if Assigned(FTimeTimer) then
  begin
    FTimeTimer.Enabled := False;
    FTimeTimer.Free;
  end;

  if g_boLoginSuccess then
  begin
    if g_LockThread <> nil then
    begin
      g_LockThread.Stop;
      g_LockThread.WaitFor;
      g_LockThread.Free;
    end;

    if g_EventHandlerThread <> nil then
    begin
      g_EventHandlerThread.Stop;
      g_EventHandlerThread.WaitFor;
      g_EventHandlerThread.Free;
    end;

    for I := 0 to g_DebuggerList.Count - 1 do
      Dispose(PTDebugger(g_DebuggerList.Items[I]));
    g_DebuggerList.Free;
  end;

  g_Authentication.Free;

end;

end.

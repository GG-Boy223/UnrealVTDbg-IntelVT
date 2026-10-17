unit GlobalVar;   // 单元名称：GlobalVar

interface         // 接口部分开始

uses              // 引入以下单元
  Winapi.ActiveX,  // ActiveX/COM 基础
  System.Win.ComObj, // COM 对象辅助
  Vcl.Forms,      // 窗体框架
  System.SysUtils, // 系统工具函数
  Winapi.Windows; // Windows API

var               // 全局变量声明区
  g_boLoginSuccess: Boolean;          // 登录成功标志
  g_boStartService: Boolean;          // 服务启动标志

  g_boEnabled_tl_confrontation_TL: Boolean; // 是否启用 TL 对抗
  g_boHandlerGetTickCountCheck_TL: Boolean; // 是否处理 GetTickCount 检测
  g_boBlockResumeThread_TL: Boolean;        // 是否阻止 ResumeThread

implementation    // 实现部分开始

initialization    // 初始化段：程序启动时执行
begin
  g_boEnabled_tl_confrontation_TL := False; // 默认关闭 TL 对抗
  g_boHandlerGetTickCountCheck_TL := False; // 默认不处理 GetTickCount
  g_boBlockResumeThread_TL := False;        // 默认不阻止 ResumeThread
end;

end. // 单元结束

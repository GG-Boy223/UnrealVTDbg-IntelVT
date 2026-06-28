#include "WinMainInternal.h"

int dotCount = 0;
std::wstring baseText1(L"正在初始化程序");
std::wstring modText;
std::wstring displayText;
int currentTextIndex = 0; // 当前绘制的文本索引

int nWidth = 0;
int progress = 0; // 进度条的当前进度
int tickcount = 0;

bool skipDownload = false; // 是否跳过符号下载

HWND g_hwnd;
std::wstring curdir;
